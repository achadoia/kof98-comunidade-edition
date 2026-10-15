// Mapa de memória do 68000, E/S e agendador por linha do Neo Geo MVS.
#include "neo.h"
#include "m68k.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static u8 ram[0x10000];          // 0x100000-0x10FFFF (espelhada até 0x1FFFFF)
static u8 sram[0x10000];         // backup RAM MVS 0xD00000
static u32 banco_base;           // janela 0x200000 -> R.p + banco_base
static int vetores_cart;         // Q1 da trava do sistema
static int sram_liberada;        // Q6
static u8 trava;                 // estado das 8 saídas da trava do sistema (HC259)
static s32 watchdog;             // ciclos 68k desde o último chute (escrita em 0x300001)
#define WATCHDOG_CICLOS (3244030 / 2) // 3244030 ciclos do clock mestre de 24MHz

u8 neo_in_p1 = 0xff, neo_in_p2 = 0xff, neo_in_start = 0x0f, neo_in_coin = 0x07, neo_in_test = 1;
int neo_linha;
int neo_cpu_pct = 200;
int neo_aes = 1;
u32 neo_quadros;
int irq_vblank, irq_raster, irq_cold;

unsigned int m68k_estado_tam(void);
void m68k_estado_pega(void *dst);
void m68k_estado_poe(const void *src);

// ---------------------------------------------------------------- relógio uPD4990A (mínimo)
// a BIOS lê TP (bit 6) e o dado serial (bit 7) em 0x320001. TP pulsa a 1Hz; a hora vem do
// registrador de deslocamento carregado no comando de leitura.
static int rtc_tp, rtc_dado, rtc_clk, rtc_stb;
static u8 rtc_cmd;
static u8 rtc_reg[6];            // seg, min, hora, dia, mês(hex)|dia-da-semana, ano (BCD)
static int rtc_bit;

static void rtc_carrega_hora(void)
{
	rtc_reg[0] = 0x00; rtc_reg[1] = 0x00; rtc_reg[2] = 0x12;
	rtc_reg[3] = 0x15; rtc_reg[4] = 0x10 << 4 | 4; rtc_reg[5] = 0x26;
	rtc_bit = 0;
}

static void rtc_escreve(u8 d)
{
	int dado = d & 1, clk = (d >> 1) & 1, stb = (d >> 2) & 1;
	if (clk && !rtc_clk) {                 // borda de subida: desloca
		rtc_cmd = (u8)((rtc_cmd >> 1) | (dado << 3));
		rtc_bit++;
		int byte = (rtc_bit / 8) % 6, bit = rtc_bit % 8;
		rtc_dado = (rtc_reg[byte] >> bit) & 1;
	}
	if (stb && !rtc_stb) {                 // strobe: executa comando de 4 bits
		if ((rtc_cmd & 0x0f) == 0x03 || (rtc_cmd & 0x0f) == 0x01) { rtc_carrega_hora(); rtc_dado = rtc_reg[0] & 1; }
	}
	rtc_clk = clk; rtc_stb = stb;
}

// ---------------------------------------------------------------- interrupções
void neo_atualiza_irq(void)
{
	int nivel = irq_cold ? 3 : irq_raster ? 2 : irq_vblank ? 1 : 0;
	m68k_set_irq(nivel);
}

// ---------------------------------------------------------------- leitura
static inline u16 rd16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

static u16 ler16(u32 a)
{
	a &= 0xffffff;
	switch (a >> 20) {
	case 0x0:
		if (a < 0x80 && !vetores_cart) return rd16(R.bios + a);
		return rd16(R.p + a);
	case 0x1: return rd16(ram + (a & 0xffff));
	case 0x2: {
		u32 o = banco_base + (a & 0xfffff);
		return o + 1 < R.p_size ? rd16(R.p + o) : 0xffff;
	}
	case 0x3:
		if (neo_aes) {   // console: sem DIPs, fichas, relógio nem cartão de memória
			if (a < 0x320000) return (u16)(neo_in_p1 << 8 | 0xff);
			if (a < 0x340000) return (u16)(audio_resposta() << 8);
			if (a < 0x360000) return (u16)(neo_in_p2 << 8 | 0xff);
			if (a < 0x380000) return 0xffff;
			if (a < 0x3a0000) return (u16)(0x00ff | (neo_in_start & 0x0f) << 8 | 0x7000);
		}
		if (a < 0x320000) {
			if ((a & 0x80) == 0) return (u16)(neo_in_p1 << 8 | 0xff);     // P1 + DIPs (todas desligadas)
			return (u16)(0xff00 | (neo_in_test ? 0x80 : 0) | 0x40);       // TEST
		}
		if (a < 0x340000)
			return (u16)(audio_resposta() << 8 | (neo_in_coin & 7) | 0x18 | (rtc_tp << 6) | (rtc_dado << 7));
		if (a < 0x360000) return (u16)(neo_in_p2 << 8 | 0xff);
		if (a < 0x380000) return 0xffff;
		if (a < 0x3a0000) {
			u16 st = 0x00ff | 0x0200 | 0x0800 | 0x7000 | 0x8000;
			if (neo_in_start & 1) st |= 0x0100;
			if (neo_in_start & 4) st |= 0x0400;
			return st;
		}
		if (a < 0x3c0000) return 0xffff;
		if (a < 0x3e0000) return video_reg_r((a >> 1) & 7, 0);
		return 0xffff;
	case 0x4: case 0x5: case 0x6: case 0x7: return paleta_r((a >> 1) & 0xfff);
	case 0xc: return rd16(R.bios + (a & 0x1ffff));
	case 0xd: return neo_aes ? 0xffff : rd16(sram + (a & 0xffff));
	default: return 0xffff;
	}
}

unsigned int m68k_read_memory_16(unsigned int a) { return ler16(a & ~1u); }
unsigned int m68k_read_memory_8(unsigned int a)
{
	u16 w = ler16(a & ~1u);
	return (a & 1) ? (w & 0xff) : (w >> 8);
}
unsigned int m68k_read_memory_32(unsigned int a) { return (u32)ler16(a) << 16 | ler16(a + 2); }

// ---------------------------------------------------------------- escrita
static void escreve(u32 a, u16 d, u16 mask)
{
	a &= 0xffffff;
#ifdef KOF_TESTE
	{ static long vigia = -2; if (vigia == -2) vigia = getenv("KOF_VIGIA") ? strtol(getenv("KOF_VIGIA"), 0, 16) : -1;
	  if (vigia >= 0 && (a & ~1u) == ((u32)vigia & ~1u)) fprintf(stderr, "vigia %06x <- %04x masc %04x pc=%06x quadro=%u\n", a, d, mask, m68k_get_reg(NULL, M68K_REG_PPC), neo_quadros); }
#endif
	switch (a >> 20) {
	case 0x1: {
		if (getenv("KOF_DEBUG_FD82") && (a & 0xfffe) == 0xfd82)
			fprintf(stderr, "10fd82 <- %04x mask %04x pc=%06x quadro=%u\n", d, mask, m68k_get_reg(NULL, M68K_REG_PPC), neo_quadros);
		u8 *p = ram + (a & 0xfffe);
		if (mask & 0xff00) p[0] = (u8)(d >> 8);
		if (mask & 0x00ff) p[1] = (u8)d;
		return;
	}
	case 0x2:
		if (a >= 0x2ffff0) {
			int banco = d & 7;
			if ((u32)(banco + 1) * 0x100000 >= R.p_size) banco = 0;
			banco_base = (u32)(banco + 1) * 0x100000;
		}
		return;
	case 0x3:
		if (a < 0x320000) {
			if (mask & 0x00ff) watchdog = 0;
			return;
		}
		if (a >= 0x320000 && a < 0x340000) {
			if (mask & 0xff00) audio_comando((u8)(d >> 8));
			return;
		}
		if (a >= 0x380000 && a < 0x3a0000) {
			if (mask & 0x00ff) {
				u32 off = (a & 0xff) >> 1;
				if ((off & 0x78) == 0x28) rtc_escreve((u8)d);
			}
			return;
		}
		if (a >= 0x3a0000 && a < 0x3c0000) {
			if (mask & 0x00ff) {
				u32 off = (a >> 1) & 0xf;
				int q = off & 7, v = (off >> 3) & 1;
				trava = (u8)((trava & ~(1 << q)) | (v << q));
				switch (q) {
				case 0: video_shadow = v; break;
				case 1: vetores_cart = v; break;
				case 5: if (!neo_aes) { video_fix_cart = v; audio_bios_ou_cart(v); } break;
				case 6: sram_liberada = v; break;
				case 7: video_banco_paleta = v; break;
				}
			}
			return;
		}
		if (a >= 0x3c0000 && a < 0x3e0000) { video_reg_w((a >> 1) & 7, d, mask); return; }
		return;
	case 0x4: case 0x5: case 0x6: case 0x7:
		paleta_w((a >> 1) & 0xfff, d, mask);
		return;
	case 0xd:
		if (getenv("KOF_DEBUG_SRAM") && (a & 0xfffe) == 0x100)
			fprintf(stderr, "sram[100] <- %04x mask %04x liberada=%d pc=%06x quadro=%u\n", d, mask, sram_liberada, m68k_get_reg(NULL, M68K_REG_PPC), neo_quadros);
		if (sram_liberada) {
			u8 *p = sram + (a & 0xfffe);
			if (mask & 0xff00) p[0] = (u8)(d >> 8);
			if (mask & 0x00ff) p[1] = (u8)d;
		}
		return;
	default:
		return;
	}
}

void m68k_write_memory_8(unsigned int a, unsigned int d)
{
	if (a & 1) escreve(a & ~1u, (u16)(d & 0xff), 0x00ff);
	else escreve(a, (u16)((d & 0xff) << 8), 0xff00);
}
void m68k_write_memory_16(unsigned int a, unsigned int d) { escreve(a & ~1u, (u16)d, 0xffff); }
void m68k_write_memory_32(unsigned int a, unsigned int d)
{
	escreve(a & ~1u, (u16)(d >> 16), 0xffff);
	escreve((a + 2) & ~1u, (u16)d, 0xffff);
}

// ---------------------------------------------------------------- inicialização
#ifdef KOF_TESTE
// KOF_PC=hex: imprime os registradores toda vez que o 68000 chega nesse endereço (build de teste)
void kof_pc_hook(unsigned int pc)
{
	static long alvo = -2;
	if (alvo == -2) alvo = getenv("KOF_PC") ? strtol(getenv("KOF_PC"), 0, 16) : -1;
	{   // KOF_PC_FAIXA=ini (hex): lista, por quadro, os endereços executados em [ini, ini+0x20000)
		static long faixa = -2; static u8 visto[0x10000]; static u32 q_ant;
		if (faixa == -2) faixa = getenv("KOF_PC_FAIXA") ? strtol(getenv("KOF_PC_FAIXA"), 0, 16) : -1;
		if (faixa >= 0) {
			if (neo_quadros != q_ant) {
				fprintf(stderr, "faixa %u:", q_ant);
				for (int i = 0; i < 0x10000; i++) if (visto[i]) fprintf(stderr, " %lx", faixa + 2L * i);
				fputc(10, stderr);
				memset(visto, 0, sizeof visto); q_ant = neo_quadros;
			}
			if ((long)pc >= faixa && (long)pc < faixa + 0x20000) visto[(pc - faixa) / 2] = 1;
		}
	}
	if ((long)pc != alvo) return;
	fprintf(stderr, "pc %06x quadro %u:", pc, neo_quadros);
	for (int r = M68K_REG_D0; r <= M68K_REG_A7; r++) fprintf(stderr, " %s%d=%08x", r < M68K_REG_A0 ? "d" : "a", r < M68K_REG_A0 ? r - M68K_REG_D0 : r - M68K_REG_A0, m68k_get_reg(NULL, r));
	fputc('\n', stderr);
}
#endif

void neo_init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	video_init();
	audio_init(55555);
	neo_reset();
}

// reset suave (watchdog): CPUs, trava do sistema e som voltam ao início; RAMs e VRAM ficam
static void neo_reset_suave(void)
{
	vetores_cart = 0;
	trava = 0;
	sram_liberada = 0;
	video_shadow = video_banco_paleta = video_fix_cart = 0;
	audio_reset();
	if (neo_aes) { video_fix_cart = 1; audio_bios_ou_cart(1); }
	watchdog = 0;
	m68k_pulse_reset();
	neo_atualiza_irq();
}

void neo_reset(void)
{
	memset(ram, 0, sizeof ram);
	banco_base = R.p_size > 0x100000 ? 0x100000 : 0;
	vetores_cart = 0;
	trava = 0;
	sram_liberada = 0;
	rtc_carrega_hora();
	video_reset();
	audio_reset();
	if (neo_aes) { video_fix_cart = 1; audio_bios_ou_cart(1); }
	irq_vblank = irq_raster = 0;
	irq_cold = 1;               // IRQ3 da partida a frio
	m68k_pulse_reset();
	neo_atualiza_irq();
	neo_linha = 0;
}

// ---------------------------------------------------------------- agendador
static s32 sobra;                 // ciclos 68k a mais executados na fatia anterior

static void roda_68k(s32 ciclos)
{
	s32 alvo = ciclos - sobra;
	while (alvo > 0) {
		s32 fatia = alvo;
		if (video_irq2_ciclos >= 0 && video_irq2_ciclos < fatia) fatia = video_irq2_ciclos > 0 ? video_irq2_ciclos : 1;
		s32 feito = m68k_execute(fatia);
		alvo -= feito;
		watchdog += feito;
		if (!neo_aes && watchdog > WATCHDOG_CICLOS * neo_cpu_pct / 100) { neo_reset_suave(); return; }
		if (video_irq2_ciclos >= 0) {
			video_irq2_ciclos -= feito;
			if (video_irq2_ciclos <= 0) {
				if (video_irq2_ctrl & 0x10) { irq_raster = 1; neo_atualiza_irq(); }
				if (video_irq2_ctrl & 0x80) video_irq2_recarregar();
				else video_irq2_ciclos = -1;
			}
		}
	}
	sobra = -alvo;
}

void neo_frame(void)
{
	for (int l = 0; l < NEO_LINHAS; l++) {
		neo_linha = l;
		if (l == 0xf0) {           // NEOGEO_VBSTART: IRQ de vblank + recarga automática do timer
			irq_vblank = 1;
			neo_atualiza_irq();
			if (video_irq2_ctrl & 0x40) video_irq2_recarregar();
		}
		video_linha_inicio(l);
		roda_68k(NEO_CICLOS_LINHA * neo_cpu_pct / 100);
		audio_roda_linha();
		video_linha_fim(l);
		if (l == 0x100) video_vsync();  // contador de animação automática (NEOGEO_VSSTART)
	}
	neo_quadros++;
	if (neo_quadros % 30 == 0) rtc_tp ^= 1;
}

// leituras do disassembler do Musashi (sem efeitos colaterais de E/S)
unsigned int m68k_read_disassembler_8(unsigned int a) { return m68k_read_memory_8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_memory_16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_memory_32(a); }

void neo_estado(Estado *e)
{
	static u8 ctx[4096];
	if (e->salvar) m68k_estado_pega(ctx);
	est_bloco(e, ctx, m68k_estado_tam());
	if (!e->salvar && !e->erro) m68k_estado_poe(ctx);
	EST(e, ram); EST(e, sram); EST(e, banco_base); EST(e, vetores_cart); EST(e, sram_liberada);
	EST(e, trava); EST(e, watchdog); EST(e, rtc_tp); EST(e, rtc_dado); EST(e, rtc_clk); EST(e, rtc_stb);
	EST(e, rtc_cmd); EST(e, rtc_reg); EST(e, rtc_bit); EST(e, neo_linha); EST(e, neo_quadros);
	EST(e, irq_vblank); EST(e, irq_raster); EST(e, irq_cold); EST(e, sobra);
}
