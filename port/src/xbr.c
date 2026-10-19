// Filtro xBR (nível 2) 4x na CPU: bordas em curva, visual do Frame Perfect / shaders xBR do RetroArch.
// Algoritmo de Hyllian (xBR-lv2, MIT), reescrito do zero em C: pra cada pixel E e cada canto da vizinhança 5x5
//      A1 B1 C1
//   A0 A  B  C  C4
//   D0 D  E  F  F4
//   G0 G  H  I  I4
//      G5 H5 I5
// decide se há uma borda diagonal passando pelo canto (peso das distâncias de cor em YUV) e, se há, pinta a
// parte do bloco 4x4 de saída que fica do lado de lá da borda com a cor vizinha mais parecida (F ou H).
// As "linhas" fx/fx_left/fx_up definem a inclinação (45°, mais deitada, mais em pé) com cobertura suavizada.
#include "neo.h"
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ESC 4
#define LIMIAR_IGUAL (15 * 255 * 16)     // XBR_EQ_THRESHOLD 15 (escala do shader: pesos somam 48, cores 0-1)

static u32 *yuv;                           // por pixel: Y<<16 | U<<8 | V  (pesos aplicados na distância)
static int tab_pronta;
static u8 cob[3][4][4][4];                 // [linha fx/left/up][rotação][sy][sx] = cobertura 0..16 do subpixel

static inline int dist(u32 a, u32 b)       // 14.352|dR| + 28.176|dG| + 5.472|dB| em ponto fixo ×16 (0..~195840)
{
	int dr = (int)((a >> 16) & 255) - (int)((b >> 16) & 255), dg = (int)((a >> 8) & 255) - (int)((b >> 8) & 255), db = (int)(a & 255) - (int)(b & 255);
	if (dr < 0) dr = -dr; if (dg < 0) dg = -dg; if (db < 0) db = -db;
	return dr * 230 + dg * 451 + db * 88;
}
static inline int igual(u32 a, u32 b) { return dist(a, b) < LIMIAR_IGUAL; }
static inline u32 mistura(u32 e, u32 p, int c)   // c = 0..16
{
	if (c <= 0) return e;
	if (c >= 16) return p;
	u32 r = ((e >> 16) & 255) + ((((int)((p >> 16) & 255) - (int)((e >> 16) & 255)) * c) >> 4);
	u32 g = ((e >> 8) & 255) + ((((int)((p >> 8) & 255) - (int)((e >> 8) & 255)) * c) >> 4);
	u32 b = (e & 255) + ((((int)(p & 255) - (int)(e & 255)) * c) >> 4);
	return 0xff000000u | r << 16 | g << 8 | b;
}

static void prepara_tabelas(void)
{
	// coordenadas canônicas do subpixel (canto inferior direito); cobertura = clamp((v - C) * ESC + 0.5)
	const float A[3] = {1.0f, 0.5f, 2.0f}, B[3] = {1.0f, 2.0f, 0.5f}, C[3] = {1.5f, 2.0f, 2.0f};
	for (int r = 0; r < 4; r++)
		for (int sy = 0; sy < ESC; sy++)
			for (int sx = 0; sx < ESC; sx++) {
				int cx = sx, cy = sy;
				for (int k = 0; k < r; k++) { int nx = ESC - 1 - cy, ny = cx; cx = nx; cy = ny; }   // gira o bloco 90° horário r vezes
				float fx = (cx + 0.5f) / ESC, fy = (cy + 0.5f) / ESC;
				for (int l = 0; l < 3; l++) {
					float v = (A[l] * fy + B[l] * fx - C[l]) * ESC + 0.5f;
					if (v < 0) v = 0; if (v > 1) v = 1;
					cob[l][r][sy][sx] = (u8)(v * 16.0f + 0.5f);
				}
			}
	tab_pronta = 1;
}

// vizinhança 5x5 girada: posição canônica (x,y) -> pixel da imagem
static inline u32 viz(const u32 *src, int w, int h, int px, int py, int x, int y, int r)
{
	for (int k = 0; k < r; k++) { int ox = y, oy = 4 - x; x = ox; y = oy; }   // inverso da rotação horária
	int ix = px + x - 2, iy = py + y - 2;
	if (ix < 0) ix = 0; if (ix >= w) ix = w - 1; if (iy < 0) iy = 0; if (iy >= h) iy = h - 1;
	return src[iy * w + ix];
}

typedef struct { const u32 *src; u32 *dst; int w, h, y0, y1; } Faixa;
static int trabalha(void *arg)
{
	Faixa *f = arg;
	const u32 *src = f->src; int w = f->w, h = f->h;
	u32 bloco[ESC][ESC];
	for (int py = f->y0; py < f->y1; py++)
		for (int px = 0; px < w; px++) {
			u32 E = src[py * w + px] | 0xff000000u;
			for (int sy = 0; sy < ESC; sy++) for (int sx = 0; sx < ESC; sx++) bloco[sy][sx] = E;
			for (int r = 0; r < 4; r++) {
				#define V(x, y) viz(src, w, h, px, py, x, y, r)
				u32 F = V(3, 2), H = V(2, 3);
				if (igual(E, F) || igual(E, H)) continue;                  // restrição nível 1: sem canto aqui
				u32 B = V(2, 1), D = V(1, 2), I = V(3, 3), C = V(3, 1), G = V(1, 3);
				u32 F4 = V(4, 2), I4 = V(4, 3), H5 = V(2, 4), I5 = V(3, 4);
				int e1 = dist(E, C) + dist(E, G) + dist(I, H5) + dist(I, F4) + 4 * dist(H, F);
				int i1 = dist(H, D) + dist(H, I5) + dist(F, I4) + dist(F, B) + 4 * dist(E, I);
				if (e1 >= i1) continue;
				// restrição nível 2 (evita manchas em padrões de 1 pixel)
				int ok = (!igual(F, B) && !igual(F, C)) || (!igual(H, D) && !igual(H, G)) ||
				         (igual(E, I) && ((!igual(F, F4) && !igual(F, I4)) || (!igual(H, H5) && !igual(H, I5)))) || igual(E, G) || igual(E, C);
				if (!ok) continue;
				u32 px_cor = dist(E, F) <= dist(E, H) ? F : H;
				int dfg = dist(F, G), dhc = dist(H, C);
				int esq = 2 * dfg <= dhc && !igual(E, G) && !igual(D, G);
				int cima = dfg >= 2 * dhc && !igual(E, C) && !igual(B, C);
				for (int sy = 0; sy < ESC; sy++)
					for (int sx = 0; sx < ESC; sx++) {
						int c = cob[0][r][sy][sx];
						if (esq && cob[1][r][sy][sx] > c) c = cob[1][r][sy][sx];
						if (cima && cob[2][r][sy][sx] > c) c = cob[2][r][sy][sx];
						if (c) bloco[sy][sx] = mistura(bloco[sy][sx], px_cor, c);
					}
				#undef V
			}
			for (int sy = 0; sy < ESC; sy++) memcpy(f->dst + (py * ESC + sy) * (w * ESC) + px * ESC, bloco[sy], ESC * 4);
		}
	return 0;
}

// src w×h ARGB -> dst (w*4)×(h*4) ARGB
void xbr_4x(const u32 *src, u32 *dst, int w, int h)
{
	if (!tab_pronta) prepara_tabelas();
	(void)yuv;
	enum { NT = 4 };
	Faixa f[NT]; SDL_Thread *t[NT];
	for (int i = 0; i < NT; i++) {
		f[i].src = src; f[i].dst = dst; f[i].w = w; f[i].h = h; f[i].y0 = h * i / NT; f[i].y1 = h * (i + 1) / NT;
		t[i] = i ? SDL_CreateThread(trabalha, "xbr", &f[i]) : NULL;
	}
	Uint64 t0 = SDL_GetPerformanceCounter();
	trabalha(&f[0]);
	for (int i = 1; i < NT; i++) if (t[i]) SDL_WaitThread(t[i], NULL);
	static double soma; static int n; static int dbg = -1;
	if (dbg < 0) dbg = getenv("KOF_DEBUG_XBR") != NULL;
	if (dbg) { soma += (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency(); if (++n == 60) { fprintf(stderr, "xbr: %.2f ms/quadro\n", soma / n); soma = 0; n = 0; } }
}
