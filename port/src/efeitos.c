// Vozes e efeitos trocáveis: som/efeitos/<P><XX>*.wav ao lado do EXE substitui o som do chip.
// Código de 12 bits: P = nibble do prefixo (0x16-0x1e -> 6-E) e XX = parâmetro; P = 0 é efeito de 1 byte (XX).
// É o mesmo nome dos sons do KOF '98 Ultimate Match (WaveBank.xwb: "015D_A1E" = comando 1A 1E), importados
// por tools/importar_um.py. Cada prefixo tem um canal (som novo corta o anterior do mesmo prefixo, como no chip);
// efeitos de 1 byte dividem 7 canais em rodízio.
#include "efeitos.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "dr_wav.h"

typedef struct { s16 *d; u32 n, taxa; } Som;
static Som som[0x1000];
static char pasta[600];
int efeitos_ligados = 1;

#define NCANAIS 16
typedef struct { const Som *s; double pos, passo; } Canal;
static Canal canal[NCANAIS];
static int rodizio;

static int hexval(int ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; }

static void carrega(int cod, const char *arq)
{
	SDL_RWops *rw = SDL_RWFromFile(arq, "rb");
	if (!rw) return;
	Sint64 n = SDL_RWsize(rw);
	void *buf = malloc((size_t)n);
	size_t lidos = SDL_RWread(rw, buf, 1, (size_t)n);
	SDL_RWclose(rw);
	unsigned ch, taxa;
	drwav_uint64 q;
	s16 *pcm = drwav_open_memory_and_read_pcm_frames_s16(buf, lidos, &ch, &taxa, &q, NULL);
	free(buf);
	if (!pcm || !q || !taxa) { drwav_free(pcm, NULL); return; }
	if (ch > 1)                           // guarda mono
		for (drwav_uint64 i = 0; i < q; i++) {
			int s = 0;
			for (unsigned c = 0; c < ch; c++) s += pcm[i * ch + c];
			pcm[i] = (s16)(s / (int)ch);
		}
	som[cod] = (Som){pcm, (u32)q, taxa};
}

void efeitos_procura(void)
{
	efeitos_para_tudo();
	for (int i = 0; i < 0x1000; i++) { drwav_free(som[i].d, NULL); som[i].d = NULL; som[i].n = 0; }
	wchar_t wp[600], busca[620];
	MultiByteToWideChar(CP_UTF8, 0, pasta, -1, wp, 600);
	_snwprintf(busca, 620, L"%ls\\*.wav", wp);
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(busca, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		char nome[400], arq[1100];
		WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nome, sizeof nome, NULL, NULL);
		int a = hexval(nome[0]), b = hexval(nome[1]), c = hexval(nome[2]);
		if (a < 0 || b < 0 || c < 0 || hexval(nome[3]) >= 0) continue;   // "A1E.wav", "A1E qualquer.wav"
		int cod = a << 8 | b << 4 | c;
		if (som[cod].d) continue;
		snprintf(arq, sizeof arq, "%s/%s", pasta, nome);
		carrega(cod, arq);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

void efeitos_init(const char *base)
{
	snprintf(pasta, sizeof pasta, "%ssom/efeitos", base ? base : "");
	efeitos_procura();
}

int efeitos_quantos(void) { int n = 0; for (int i = 0; i < 0x1000; i++) n += som[i].d != NULL; return n; }
int efeitos_tem(int cod) { return cod >= 0 && cod < 0x1000 && som[cod].d; }

void efeitos_toca(int canal_prefixo, int cod)
{
	if (!efeitos_tem(cod)) return;
#ifdef KOF_TESTE
	if (getenv("KOF_DEBUG_SOM")) fprintf(stderr, "efeito UM %03x\n", cod);
#endif
	int c;
	if (canal_prefixo >= 0 && canal_prefixo < 9) c = canal_prefixo;
	else { c = 9 + rodizio; rodizio = (rodizio + 1) % (NCANAIS - 9); }
	canal[c] = (Canal){&som[cod], 0.0, (double)som[cod].taxa / AUDIO_TAXA};
}

void efeitos_para_tudo(void) { memset(canal, 0, sizeof canal); }

void efeitos_para(int cod)
{
	if (cod < 0 || cod >= 0x1000) return;
	for (int c = 0; c < NCANAIS; c++) if (canal[c].s == &som[cod]) {
#ifdef KOF_TESTE
		if (getenv("KOF_DEBUG_SOM")) fprintf(stderr, "para UM %03x (canal %d, tocou %.1f s)\n", cod, c, canal[c].pos / som[cod].taxa);
#endif
		canal[c].s = NULL;
	}
}

void efeitos_mistura(s16 *buf, int n)
{
	for (int c = 0; c < NCANAIS; c++) {
		Canal *k = &canal[c];
		if (!k->s) continue;
		for (int i = 0; i < n; i++) {
			u32 p = (u32)k->pos;
			if (p + 1 >= k->s->n) { k->s = NULL; break; }
			double f = k->pos - p;
			int v = (int)(k->s->d[p] + (k->s->d[p + 1] - k->s->d[p]) * f);
			for (int lado = 0; lado < 2; lado++) {
				int s = buf[i * 2 + lado] + v;
				buf[i * 2 + lado] = (s16)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
			}
			k->pos += k->passo;
		}
	}
}
