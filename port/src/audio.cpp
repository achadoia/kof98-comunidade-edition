// Som do Neo Geo: Z80 (superzazu/z80, MIT) + YM2610 (ymfm, BSD-3).
// Comportamento das portas/bancos conforme ref/mame/neogeo.cpp (BSD-3).
#include "neo.h"
#include "musica.h"
#include "efeitos.h"
extern "C" {
#include "z80.h"
}
#include "ymfm_opn.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>

static const u32 YM_CLOCK = 8000000;   // NEOGEO_YM2610_CLOCK
static const u32 Z80_CLOCK = 4000000;

static z80 cpu;
static u8 zram[0x800];
static int usa_cart;                   // 0x0000-0x7FFF: 0 = sm1 (BIOS), 1 = m1 do cartucho
static u32 banco[4];                   // f000, e000, c000, 8000
static u8 latch_cmd, latch_resp;
static int cmd_pendente, nmi_ligado;
static long ciclos_sobra;

// ---------------------------------------------------------------- YM2610
class Interface : public ymfm::ymfm_interface {
public:
	int64_t timer[2] = {-1, -1};
	bool irq = false;
	void ymfm_set_timer(uint32_t t, int32_t dur) override { timer[t] = dur < 0 ? -1 : dur; }
	void ymfm_update_irq(bool a) override { irq = a; }
	uint8_t ymfm_external_read(ymfm::access_class tipo, uint32_t a) override
	{
		(void)tipo;
		return a < R.vrom_size ? R.vrom[a] : 0;
	}
	void avanca(int64_t clocks)
	{
		for (int t = 0; t < 2; t++) {
			if (timer[t] < 0) continue;
			timer[t] -= clocks;
			if (timer[t] <= 0) { timer[t] = -1; m_engine->engine_timer_expired(t); }
		}
	}
};
static Interface intf;
static ymfm::ym2610 *ym;

// saída: anel de amostras estéreo
static s16 saida[65536 * 2];
static u32 saida_ini, saida_fim;
static double frac_amostra;
static double amostras_por_linha;

// ---------------------------------------------------------------- saída: filtro da placa + 48 kHz
// O YM2610 gera ADPCM e PSG "em degraus" e a placa do Neo Geo suaviza com filtro analógico. Sem isso
// sobra chiado agudo, e a reamostragem 55,5k -> 48k do Windows dobrava esse agudo pra faixa audível.
// Aqui: passa-baixa Butterworth de 4ª ordem (2 biquads) e reamostragem própria pra AUDIO_TAXA.
struct Biquad { double b0, b1, b2, a1, a2, z1[2], z2[2]; };
static Biquad lp[2];
static double rs_ant[2], rs_fase, rs_passo, corte_hz = 12000.0;

static void prepara_filtro(double fs, double corte)
{
	static const double Q[2] = {0.5411961, 1.3065630};   // Butterworth 4ª ordem
	for (int k = 0; k < 2; k++) {
		double w = 2.0 * 3.14159265358979 * corte / fs, c = cos(w), s = sin(w), al = s / (2 * Q[k]), a0 = 1 + al;
		Biquad &b = lp[k];
		b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = (1 - c) / 2 / a0;
		b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
		b.z1[0] = b.z1[1] = b.z2[0] = b.z2[1] = 0;
	}
	rs_ant[0] = rs_ant[1] = 0;
	rs_fase = 0;
	rs_passo = fs / AUDIO_TAXA;
}

static double filtra(int ch, double x)
{
	for (int k = 0; k < 2; k++) {
		Biquad &b = lp[k];
		double y = b.b0 * x + b.z1[ch];
		b.z1[ch] = b.b1 * x - b.a1 * y + b.z2[ch];
		b.z2[ch] = b.b2 * x - b.a2 * y;
		x = y;
	}
	return x;
}

static void empurra_saida(double l, double r)
{
	while (rs_fase <= 1.0) {
		double ol = rs_ant[0] + (l - rs_ant[0]) * rs_fase, orr = rs_ant[1] + (r - rs_ant[1]) * rs_fase;
		if (ol > 32767) ol = 32767; if (ol < -32768) ol = -32768;
		if (orr > 32767) orr = 32767; if (orr < -32768) orr = -32768;
		u32 i = saida_fim & 65535;
		saida[i * 2] = (s16)ol;
		saida[i * 2 + 1] = (s16)orr;
		saida_fim++;
		rs_fase += rs_passo;
	}
	rs_fase -= 1.0;
	rs_ant[0] = l; rs_ant[1] = r;
}

// ---------------------------------------------------------------- memória do Z80
static u8 z_le(void *, u16 a)
{
	if (a < 0x8000) return usa_cart ? R.m1[a % R.m1_size] : R.sm1[a];
	if (a >= 0xf800) return zram[a & 0x7ff];
	int r = a >= 0xf000 ? 0 : a >= 0xe000 ? 1 : a >= 0xc000 ? 2 : 3;
	static const u16 base[4] = {0xf000, 0xe000, 0xc000, 0x8000};
	return R.m1[(banco[r] + (a - base[r])) % R.m1_size];
}

static void z_escreve(void *, u16 a, u8 d)
{
	if (a >= 0xf800) zram[a & 0x7ff] = d;
}

// endereço completo de 16 bits do IN: "in a,(n)" usa A no byte alto, "in r,(c)" usa B
static u16 porta_completa(z80 *z, u8 p)
{
	u8 op = z_le(nullptr, (u16)(z->pc - 2));
	u8 alto = op == 0xdb ? z->a : z->b;
	return (u16)(alto << 8 | p);
}

static u16 ym_end_alto;
static u8 ym_regs[256];

static void dispara_nmi(void) { if (cmd_pendente && nmi_ligado) z80_gen_nmi(&cpu); }

static u8 z_in(z80 *z, u8 p)
{
	switch (p & 0x0f) {
	case 0x00: cmd_pendente = 0; return latch_cmd;
	case 0x04: case 0x05: case 0x06: case 0x07: return ym->read(p & 3);
	case 0x08: case 0x09: case 0x0a: case 0x0b: {
		u16 full = porta_completa(z, p);
		int r = full & 3;                    // 08 -> f000, 09 -> e000, 0a -> c000, 0b -> 8000
		u32 b = full >> 8;
		banco[r] = (b << (11 + r)) & (R.m1_size - 1);
		return 0;
	}
	default:
		if (getenv("KOF_DEBUG_Z80")) fprintf(stderr, "z80 IN porta %02x (pc %04x)\n", p, z->pc);
		return 0;
	}
}

static void z_out(z80 *, u8 p, u8 d)
{
	switch (p & 0x1f) {
	case 0x00: case 0x10: cmd_pendente = 0; break;          // limpa o latch
	case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x14: case 0x15: case 0x16: case 0x17:
		if ((p & 3) == 2) ym_end_alto = 0x100 | d;
#ifdef KOF_TESTE
		{   // KOF_LOG_YM=arq: cada escrita num registrador do YM (amostra de saída, banco, reg, valor) — acha o loop das músicas
			static FILE *lg; static int tentou; static u8 end_a;
			if (!tentou) { tentou = 1; if (getenv("KOF_LOG_YM")) lg = fopen(getenv("KOF_LOG_YM"), "wb"); }
			if ((p & 3) == 0) end_a = d;
			if (lg && (p & 1)) { u32 rec[2] = {saida_fim, (u32)((p & 2) << 15 | ((p & 2) ? ym_end_alto & 0xff : end_a) << 8 | d)}; fwrite(rec, 4, 2, lg); }
		}
#endif
		if ((p & 3) == 3 && getenv("KOF_DEBUG_ADPCM")) {
			ym_regs[ym_end_alto & 0xff] = d;
			int r = ym_end_alto & 0xff;
			if (r >= 0x18 && r <= 0x1d) {          // byte alto do início do canal ADPCM-A
				u32 ini = ((u32)ym_regs[r] << 8 | ym_regs[r - 8]) << 8;
				fprintf(stderr, "adpcmA canal %d inicio %06x\n", r - 0x18, ini);
			}
		}
		ym->write(p & 3, d);
		break;
	case 0x08: nmi_ligado = 1; dispara_nmi(); break;
	case 0x18: nmi_ligado = 0; break;
	case 0x0c: case 0x1c: latch_resp = d; break;
	default: if (getenv("KOF_DEBUG_Z80")) fprintf(stderr, "z80 OUT porta %02x = %02x\n", p, d); break;
	}
}

// ---------------------------------------------------------------- interface com o 68k
extern "C" u32 audio_cmd_total;
extern "C" u32 audio_cmd_perdidos;
u32 audio_cmd_total, audio_cmd_perdidos;   // diagnóstico: comandos que chegaram com o anterior ainda na fila

// entrega o próximo byte da fila (musica.c) quando o Z80 já leu o anterior
static void entrega(void)
{
	u8 c;
	if (cmd_pendente || !musica_fila_tira(&c)) return;
	latch_cmd = c;
	cmd_pendente = 1;
	dispara_nmi();
}

extern "C" void audio_comando(u8 cmd)
{
	audio_cmd_total++;
	if (cmd_pendente) audio_cmd_perdidos++;
#ifdef KOF_TESTE
	if (getenv("KOF_DEBUG_SOM")) fprintf(stderr, "som %u %02x\n", neo_quadros, cmd);
#endif
	musica_comando(cmd);   // toca arquivo (música/voz/efeito) ou enfileira pro chip
	entrega();
}

extern "C" u8 audio_resposta(void) { return latch_resp; }
extern "C" void audio_bios_ou_cart(int cart) { usa_cart = cart; }

extern "C" void audio_init(int taxa)
{
	(void)taxa;
	ym = new ymfm::ym2610(intf);
	ym->set_fidelity(ymfm::OPN_FIDELITY_MIN);
	amostras_por_linha = (double)ym->sample_rate(YM_CLOCK) / (NEO_HZ * NEO_LINHAS);
	const char *c = getenv("KOF_CORTE");
	corte_hz = c ? atof(c) : 12000.0;
	prepara_filtro((double)ym->sample_rate(YM_CLOCK), corte_hz);
}

extern "C" void audio_reset(void)
{
	z80_init(&cpu);
	cpu.read_byte = z_le;
	cpu.write_byte = z_escreve;
	cpu.port_in = z_in;
	cpu.port_out = z_out;
	memset(zram, 0, sizeof zram);
	usa_cart = 0;
	banco[0] = (0x1eu << 11) & (R.m1_size - 1);
	banco[1] = (0x0eu << 12) & (R.m1_size - 1);
	banco[2] = (0x06u << 13) & (R.m1_size - 1);
	banco[3] = (0x02u << 14) & (R.m1_size - 1);
	latch_cmd = latch_resp = 0;
	cmd_pendente = nmi_ligado = 0;
	ciclos_sobra = 0;
	ym->reset();
	intf.timer[0] = intf.timer[1] = -1;
	intf.irq = false;
	saida_ini = saida_fim = 0;
	frac_amostra = 0;
	musica_reset();
	prepara_filtro((double)ym->sample_rate(YM_CLOCK), corte_hz);
}

extern "C" void audio_roda_linha(void)
{
	long alvo = NEO_Z80_CICLOS_LINHA - ciclos_sobra;
	entrega();
	while (alvo > 0) {
		if (intf.irq) cpu.int_pending = 1;    // IRQ do YM é por nível
		unsigned long antes = cpu.cyc;
		z80_step(&cpu);
		long feito = (long)(cpu.cyc - antes);
		if (feito <= 0) feito = 4;
		alvo -= feito;
		intf.avanca((int64_t)feito * (YM_CLOCK / Z80_CLOCK));
	}
	ciclos_sobra = -alvo;

	frac_amostra += amostras_por_linha;
	while (frac_amostra >= 1.0) {
		frac_amostra -= 1.0;
		ymfm::ym2610::output_data o;
		ym->generate(&o, 1);
		int ssg = o.data[2];
		int l = (o.data[0] * 98 + ssg * 84) / 100;
		int r = (o.data[1] * 98 + ssg * 84) / 100;
		empurra_saida(filtra(0, (double)l), filtra(1, (double)r));
	}
}

extern "C" int audio_pega_amostras(s16 *dst, int max_pares)
{
	int n = 0;
	while (saida_ini != saida_fim && n < max_pares) {
		u32 i = saida_ini & 65535;
		dst[n * 2] = saida[i * 2];
		dst[n * 2 + 1] = saida[i * 2 + 1];
		saida_ini++;
		n++;
	}
	musica_mistura(dst, n);
	efeitos_mistura(dst, n);
	// limitador suave: chip + música do arquivo + vozes do UM somados podem passar de 16 bits;
	// em vez de cortar reto (estalo/estouro), comprime só o que passa de 3/4 da escala
	for (int i = 0; i < n * 2; i++) {
		int v = dst[i];
		const int T = 24576;
		if (v > T || v < -T) {
			double x = ((v < 0 ? -v : v) - T) / (32767.0 - T);
			double y = T + (32767.0 - T) * (x / (1.0 + x));
			dst[i] = (s16)(v < 0 ? -y : y);
		}
	}
	return n;
}

extern "C" void audio_estado(Estado *e)
{
	z80 copia = cpu;
	EST(e, copia);
	if (!e->salvar) {
		copia.read_byte = z_le; copia.write_byte = z_escreve; copia.port_in = z_in; copia.port_out = z_out;
		cpu = copia;
	}
	EST(e, zram); EST(e, usa_cart); EST(e, banco); EST(e, latch_cmd); EST(e, latch_resp);
	EST(e, cmd_pendente); EST(e, nmi_ligado); EST(e, ciclos_sobra); EST(e, intf.timer); EST(e, intf.irq);
	EST(e, frac_amostra);
	std::vector<uint8_t> v;
	u32 n = 0;
	if (e->salvar) { ymfm::ymfm_saved_state st(v, true); ym->save_restore(st); n = (u32)v.size(); }
	EST(e, n);
	if (!e->salvar) v.resize(n);
	est_bloco(e, v.data(), n);
	if (!e->salvar) { ymfm::ymfm_saved_state st(v, false); ym->save_restore(st); }
	musica_estado(e);    // por último: estados salvos antes da trilha trocável não têm esse bloco
}
