// Trilha trocável: se existir som/musica/<código>*.ogg|.mp3|.wav ao lado do EXE, o EXE toca esse arquivo
// no lugar da música do chip. Sem arquivo, toca a música original do YM2610 como sempre.
//
// O 68000 manda bytes pro Z80 em 0x320000. Mapa do driver de som do KOF98 Anniversary (varredura 15/10,
// KOF_SO_SOM):
//   0x21-0x56  música (0x4b = 0x21); 0x24 0x2c 0x2e 0x2f 0x49 0x50 0x51 0x56 são vinhetas (não repetem)
//   0x20       para a música          0x01 0x03 0x04 0x0d  param tudo (reset/troca de slot)
//   0x02       fade-out (~6 s)        0x0a xx  fade-out curto (xx = parâmetro)
//   0x05 0x0a 0x0e 0x15 0x16-0x1e  levam 1 byte de parâmetro (0x16-0x1e = vozes/efeitos)
//   0x14 P X   para o som P X (ex.: 14 1D 11 = fim dos passos da corrida)
//   0x57-0xff  efeitos de 1 byte
// Vozes/efeitos com arquivo em som/efeitos (efeitos.c) também tocam aqui e não chegam ao Z80.
// Com arquivo, o código da música vira 0x20 pro Z80 (o chip fica livre pros efeitos) e o arquivo toca aqui.
// Repetição: tags LOOPSTART/LOOPLENGTH (amostras) do OGG, senão o arquivo inteiro.
#include "musica.h"
#include "efeitos.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include "dr_mp3.h"
#include "dr_wav.h"

static char *arquivo[256];           // código -> caminho UTF-8
static char pasta[600];

typedef struct {
	int cod;                          // código tocando (-1 = nenhum)
	int tipo;                         // 1 ogg, 2 mp3, 3 wav
	void *dados; size_t tam;          // arquivo inteiro na memória
	stb_vorbis *ogg; drmp3 mp3; drwav wav;
	u32 taxa, canais;
	uint64_t total, pos;                   // em quadros do arquivo
	uint64_t loop_ini, loop_fim;           // fim = ponto em que volta pra loop_ini (0 = não repete)
	double frac;                      // reamostragem pra AUDIO_TAXA
	s16 atual[2], prox[2];
	float ganho, fade_passo;          // fade-out: ganho cai fade_passo por amostra
} Faixa;
static Faixa F = {.cod = -1};

int musica_ligada = 1;
int musica_silenciosa;
int musica_ultima_cod = -1;           // última música pedida pelo jogo (pra mostrar no F1)
static int param_resta;              // bytes de parâmetro que ainda vêm (não são comandos)

static int eh_vinheta(int c)
{
	return c == 0x24 || c == 0x2c || c == 0x2e || c == 0x2f || c == 0x49 || c == 0x50 || c == 0x51 || c == 0x56;
}

static int hexval(int ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; }

static int extensao_ok(const char *n)
{
	const char *p = strrchr(n, '.');
	return p && (!_stricmp(p, ".ogg") || !_stricmp(p, ".mp3") || !_stricmp(p, ".wav"));
}

void musica_procura(void)
{
	for (int i = 0; i < 256; i++) { free(arquivo[i]); arquivo[i] = NULL; }
	wchar_t wp[600], busca[620];
	MultiByteToWideChar(CP_UTF8, 0, pasta, -1, wp, 600);
	_snwprintf(busca, 620, L"%ls\\*", wp);
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(busca, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		char nome[400];
		WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nome, sizeof nome, NULL, NULL);
		int a = hexval(nome[0]), b = hexval(nome[1]);
		if (a < 0 || b < 0 || hexval(nome[2]) >= 0 || !extensao_ok(nome)) continue;   // "28 qualquer nome.ogg"
		int c = a * 16 + b;
		if (arquivo[c]) continue;
		size_t n = strlen(pasta) + strlen(nome) + 2;
		arquivo[c] = malloc(n);
		snprintf(arquivo[c], n, "%s/%s", pasta, nome);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

int musica_quantas(void) { int n = 0; for (int i = 0; i < 256; i++) n += arquivo[i] != NULL; return n; }

void musica_init(const char *base)
{
	snprintf(pasta, sizeof pasta, "%ssom/musica", base ? base : "");
	musica_procura();
}

static void fecha(void)
{
	if (F.tipo == 1 && F.ogg) stb_vorbis_close(F.ogg);
	if (F.tipo == 2) drmp3_uninit(&F.mp3);
	if (F.tipo == 3) drwav_uninit(&F.wav);
	free(F.dados);
	memset(&F, 0, sizeof F);
	F.cod = -1;
}

static int abre(int cod)
{
	fecha();
	const char *arq = arquivo[cod];
	if (!arq) return 0;
	SDL_RWops *rw = SDL_RWFromFile(arq, "rb");
	if (!rw) return 0;
	Sint64 n = SDL_RWsize(rw);
	F.dados = malloc((size_t)n);
	F.tam = SDL_RWread(rw, F.dados, 1, (size_t)n);
	SDL_RWclose(rw);
	const char *ext = strrchr(arq, '.');
	if (!_stricmp(ext, ".ogg")) {
		int erro;
		F.ogg = stb_vorbis_open_memory(F.dados, (int)F.tam, &erro, NULL);
		if (!F.ogg) { fecha(); return 0; }
		F.tipo = 1;
		stb_vorbis_info in = stb_vorbis_get_info(F.ogg);
		F.taxa = in.sample_rate; F.canais = (u32)in.channels;
		F.total = stb_vorbis_stream_length_in_samples(F.ogg);
		stb_vorbis_comment cm = stb_vorbis_get_comment(F.ogg);
		uint64_t li = 0, ll = 0;
		for (int i = 0; i < cm.comment_list_length; i++) {
			if (!_strnicmp(cm.comment_list[i], "LOOPSTART=", 10)) li = _strtoui64(cm.comment_list[i] + 10, NULL, 10);
			if (!_strnicmp(cm.comment_list[i], "LOOPLENGTH=", 11)) ll = _strtoui64(cm.comment_list[i] + 11, NULL, 10);
		}
		if (ll) { F.loop_ini = li; F.loop_fim = li + ll; }
	} else if (!_stricmp(ext, ".mp3")) {
		if (!drmp3_init_memory(&F.mp3, F.dados, F.tam, NULL)) { fecha(); return 0; }
		F.tipo = 2;
		F.taxa = F.mp3.sampleRate; F.canais = F.mp3.channels;
		F.total = drmp3_get_pcm_frame_count(&F.mp3);
	} else {
		if (!drwav_init_memory(&F.wav, F.dados, F.tam, NULL)) { fecha(); return 0; }
		F.tipo = 3;
		F.taxa = F.wav.sampleRate; F.canais = F.wav.channels;
		F.total = F.wav.totalPCMFrameCount;
	}
	if (!F.taxa || !F.canais || !F.total) { fecha(); return 0; }
	if (!F.loop_fim && !eh_vinheta(cod)) { F.loop_ini = 0; F.loop_fim = F.total; }
	if (F.loop_fim > F.total) F.loop_fim = F.total;
	F.cod = cod;
	F.ganho = 1.0f;
	return 1;
}

static void busca(uint64_t q)
{
	if (F.tipo == 1) stb_vorbis_seek(F.ogg, (unsigned)q);
	else if (F.tipo == 2) drmp3_seek_to_pcm_frame(&F.mp3, q);
	else drwav_seek_to_pcm_frame(&F.wav, q);
	F.pos = q;
}

// lê 1 quadro estéreo do arquivo; 0 = acabou (vinheta)
static int le_quadro(s16 out[2])
{
	if (F.loop_fim && F.pos >= F.loop_fim) busca(F.loop_ini);
	if (F.pos >= F.total) return 0;
	short b[8] = {0};
	int ok;
	if (F.tipo == 1) ok = stb_vorbis_get_samples_short_interleaved(F.ogg, (int)F.canais < 8 ? (int)F.canais : 8, b, (int)F.canais);
	else if (F.tipo == 2) ok = (int)drmp3_read_pcm_frames_s16(&F.mp3, 1, b);
	else ok = (int)drwav_read_pcm_frames_s16(&F.wav, 1, b);
	if (ok <= 0) { F.pos = F.total; return F.loop_fim ? (busca(F.loop_ini), le_quadro(out)) : 0; }
	F.pos++;
	out[0] = b[0];
	out[1] = F.canais > 1 ? b[1] : b[0];
	return 1;
}

void musica_mistura(s16 *buf, int n)
{
	if (F.cod < 0) return;
	double passo = (double)F.taxa / AUDIO_TAXA;
	for (int i = 0; i < n; i++) {
		while (F.frac >= 1.0) {
			F.frac -= 1.0;
			F.atual[0] = F.prox[0]; F.atual[1] = F.prox[1];
			if (!le_quadro(F.prox)) { fecha(); return; }
		}
		for (int c = 0; c < 2; c++) {
			double v = F.atual[c] + (F.prox[c] - F.atual[c]) * F.frac;
			int s = buf[i * 2 + c] + (int)(v * F.ganho);
			buf[i * 2 + c] = (s16)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
		}
		F.frac += passo;
		if (F.fade_passo > 0) {
			F.ganho -= F.fade_passo;
			if (F.ganho <= 0) { fecha(); return; }
		}
	}
}

static void fade(double segundos) { if (F.cod >= 0) F.fade_passo = (float)(1.0 / (segundos * AUDIO_TAXA)); }

// Fila de bytes pro Z80: o prefixo de voz (0x16-0x1e) fica guardado até chegar o parâmetro, porque só
// então dá pra saber se o som tem arquivo. O audio.cpp entrega um byte por vez (quando o Z80 já leu o anterior).
static u8 fila[32];
static int fila_ini, fila_n, prefixo = -1;
static int para_n, para_p;          // 0x14 P X = "pare o som P X" (P = prefixo 16-1E): 2 parâmetros
static void poe(u8 c) { if (fila_n < 32) { fila[(fila_ini + fila_n) % 32] = c; fila_n++; } }
int musica_fila_tira(u8 *c)
{
	if (!fila_n) return 0;
	*c = fila[fila_ini];
	fila_ini = (fila_ini + 1) % 32;
	fila_n--;
	return 1;
}

void musica_comando(u8 cmd)
{
	if (prefixo >= 0) {                  // parâmetro de voz/efeito: código = (prefixo & 0xf) << 8 | parâmetro
		int p = prefixo, cod = (p & 0xf) << 8 | cmd;
		prefixo = -1;
		if (efeitos_ligados && efeitos_tem(cod)) { if (!musica_silenciosa) efeitos_toca(p - 0x16, cod); return; }
		poe((u8)p); poe(cmd);
		return;
	}
	if (para_n) {                       // os passos da corrida (1D 11) duram 72 s no UM: só param com isto
		if (para_n == 2) para_p = cmd;
		else if (!musica_silenciosa) efeitos_para((para_p & 0xf) << 8 | cmd);
		para_n--;
		poe(cmd);
		return;
	}
	if (param_resta) { param_resta--; poe(cmd); return; }
	switch (cmd) {
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e:
		prefixo = cmd; return;
	case 0x05: case 0x0e: case 0x15: param_resta = 1; poe(cmd); return;
	case 0x14: para_n = 2; poe(cmd); return;
	case 0x0a: param_resta = 1; if (!musica_silenciosa) fade(1.5); poe(cmd); return;
	case 0x02: if (!musica_silenciosa) fade(6.0); poe(cmd); return;
	case 0x01: case 0x03: case 0x04: case 0x0d: if (!musica_silenciosa) efeitos_para_tudo(); /* fallthrough */
	case 0x20: if (!musica_silenciosa) { fecha(); musica_ultima_cod = -1; } poe(cmd); return;
	}
	if (cmd >= 0x21 && cmd <= 0x56) {
		int c = cmd == 0x4b ? 0x21 : cmd;
		if (musica_silenciosa) {         // run-ahead: o Z80 recebe o mesmo que receberia, o arquivo não muda
			if (musica_ligada && arquivo[c]) { poe(0x20); return; }
			poe(cmd); return;
		}
		musica_ultima_cod = c;
		if (musica_ligada && arquivo[c] && abre(c)) {
			F.frac = 1.0;
			poe(0x20);                   // o chip para a música dele; a do arquivo toca no lugar
			return;
		}
		fecha();
	} else if (cmd > 0x56 && efeitos_ligados && efeitos_tem(cmd)) {   // efeito de 1 byte
		if (!musica_silenciosa) efeitos_toca(-1, cmd);
		return;
	}
	poe(cmd);
}

int musica_tocando(void) { return F.cod; }
const char *musica_arquivo(int cod) { return cod >= 0 && cod < 256 ? arquivo[cod] : NULL; }

void musica_reset(void)
{
	fecha();
	efeitos_para_tudo();
	param_resta = 0; prefixo = -1; para_n = 0; fila_n = fila_ini = 0;
	musica_ultima_cod = -1;
}

// run-ahead: só o que vai pro Z80 (prefixo pendente, parâmetros, fila, "pare o som"); o arquivo de música e as
// vozes por arquivo são tempo real e continuam de onde estão.
void musica_fila_estado(Estado *e)
{
	EST(e, param_resta); EST(e, prefixo); EST(e, fila_n); EST(e, fila_ini); EST(e, fila); EST(e, para_n); EST(e, para_p);
}

// savestate: música (código, posição, fade) + fila/prefixo. Bloco opcional no fim: estados antigos carregam sem ele.
void musica_estado(Estado *e)
{
	s32 cod = F.cod, pr = param_resta, pf = prefixo, fn = fila_n, fi = fila_ini, pn = para_n, pp = para_p;
	uint64_t pos = F.pos;
	float ganho = F.ganho, fp = F.fade_passo;
	size_t tam = sizeof cod * 7 + sizeof pos + sizeof ganho * 2 + sizeof fila;
	if (!e->salvar && e->pos + tam > e->cap) { musica_reset(); return; }
	EST(e, cod); EST(e, pr); EST(e, pf); EST(e, fn); EST(e, fi);
	if (estado_versao_lida >= 3) { EST(e, pn); EST(e, pp); } else pn = pp = 0;
	EST(e, pos); EST(e, ganho); EST(e, fp); EST(e, fila);
	if (e->salvar) return;
	efeitos_para_tudo();
	param_resta = pr; prefixo = pf; fila_n = fn; fila_ini = fi; para_n = pn; para_p = pp;
	if (cod < 0 || !arquivo[cod] || !abre(cod)) { fecha(); return; }
	busca(pos < F.total ? pos : 0);
	F.frac = 1.0;
	F.ganho = ganho; F.fade_passo = fp;
}
