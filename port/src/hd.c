// Desenho em alta resolução. Ver hd.h.
#include "hd.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_image.h"
extern char font8x8_basic[128][8];        // ui.c
extern char font8x8_ext_latin[96][8];

static u32 *B; static int W, H;
static u32 *quadro;
HdImagem hd_cfg_fundo, hd_treino_titulo, hd_dpad, hd_botao_b;

static void decodifica(HdImagem *im, const unsigned char *dados, int tam)
{
	int w, h, n;
	unsigned char *px = stbi_load_from_memory(dados, tam, &w, &h, &n, 4);
	if (!px) return;
	im->px = malloc((size_t)w * h * 4);
	for (int i = 0; i < w * h; i++) im->px[i] = (u32)px[i * 4 + 3] << 24 | (u32)px[i * 4] << 16 | (u32)px[i * 4 + 1] << 8 | px[i * 4 + 2];
	im->w = w; im->h = h;
	stbi_image_free(px);
}

#ifdef KOF_SOLO
#define ARTE(nome) extern const unsigned char embutido_##nome[]; extern const size_t embutido_##nome##_tam;
ARTE(hd_cfg_png) ARTE(hd_treino_png) ARTE(hd_dpad_png) ARTE(hd_b_png)
void hd_init(const char *base)
{
	(void)base;
	decodifica(&hd_cfg_fundo, embutido_hd_cfg_png, (int)embutido_hd_cfg_png_tam);
	decodifica(&hd_treino_titulo, embutido_hd_treino_png, (int)embutido_hd_treino_png_tam);
	decodifica(&hd_dpad, embutido_hd_dpad_png, (int)embutido_hd_dpad_png_tam);
	decodifica(&hd_botao_b, embutido_hd_b_png, (int)embutido_hd_b_png_tam);
}
#else
static void carrega(HdImagem *im, const char *base, const char *nome)
{
	char arq[800];
	snprintf(arq, sizeof arq, "%sarte/%s", base ? base : "", nome);
	FILE *f = fopen(arq, "rb");
	if (!f) return;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	unsigned char *d = malloc((size_t)n);
	if (fread(d, 1, (size_t)n, f) == (size_t)n) decodifica(im, d, (int)n);
	free(d); fclose(f);
}
void hd_init(const char *base)
{
	carrega(&hd_cfg_fundo, base, "hd_cfg_fundo.png");
	carrega(&hd_treino_titulo, base, "hd_treino_titulo.png");
	carrega(&hd_dpad, base, "hd_dpad.png");
	carrega(&hd_botao_b, base, "hd_botao_b.png");
}
#endif

u32 *hd_buffer(void)
{
	if (!quadro) quadro = calloc((size_t)HD_W * HD_H, 4);
	hd_alvo(quadro, HD_W, HD_H);
	return quadro;
}

void hd_limpa(void) { if (B) memset(B, 0, (size_t)W * H * 4); }

void hd_alvo(u32 *buf, int w, int h) { B = buf; W = w; H = h; }

// mistura "por cima" (alfa do destino pode ser 0 = camada transparente)
void hd_mistura(int x, int y, u32 c, int a)
{
	if (!B || x < 0 || y < 0 || x >= W || y >= H || a <= 0) return;
	if (a > 255) a = 255;
	u32 *p = &B[y * W + x], d = *p;
	u32 da = d >> 24;
	if (a >= 255) { *p = 0xff000000u | (c & 0xffffff); return; }
	if (!da) { *p = (u32)a << 24 | (c & 0xffffff); return; }
	u32 r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
	u32 g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
	u32 b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
	u32 na = a + da * (255 - a) / 255;
	*p = na << 24 | r << 16 | g << 8 | b;
}

void hd_caixa(int x0, int y0, int x1, int y1, u32 cor, int alfa)
{
	if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
	for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) hd_mistura(x, y, cor, alfa);
}

void hd_borda(int x0, int y0, int x1, int y1, u32 cor, int esp)
{
	hd_caixa(x0, y0, x1, y0 + esp, cor, 255); hd_caixa(x0, y1 - esp, x1, y1, cor, 255);
	hd_caixa(x0, y0, x0 + esp, y1, cor, 255); hd_caixa(x1 - esp, y0, x1, y1, cor, 255);
}

void hd_faixa(int x0, int x1, int yc, int alt, u32 cor, int a0, int a1)
{
	for (int x = x0; x < x1; x++) {
		int a = a0 + (a1 - a0) * (x - x0) / (x1 - x0 ? x1 - x0 : 1);
		for (int y = yc - alt / 2; y < yc + alt / 2; y++) hd_mistura(x, y, cor, a);
	}
}

static u32 entre(u32 c0, u32 c1, int t, int n)
{
	u32 r = (((c0 >> 16) & 255) * (n - t) + ((c1 >> 16) & 255) * t) / n;
	u32 g = (((c0 >> 8) & 255) * (n - t) + ((c1 >> 8) & 255) * t) / n;
	u32 b = ((c0 & 255) * (n - t) + (c1 & 255) * t) / n;
	return r << 16 | g << 8 | b;
}

// degradê horizontal c0 -> c1; inclina > 0: bordas laterais inclinadas (paralelogramo), a linha de cima
// deslocada "inclina" px pra direita
void hd_degrade(int x0, int y0, int x1, int y1, u32 c0, u32 c1, int alfa, int inclina)
{
	int n = x1 - x0 > 0 ? x1 - x0 : 1;
	for (int y = y0; y < y1; y++) {
		int d = inclina ? inclina * (y1 - 1 - y) / (y1 - y0 > 1 ? y1 - y0 - 1 : 1) : 0;
		for (int x = x0; x < x1; x++) hd_mistura(x + d, y, entre(c0, c1, x - x0, n), alfa);
	}
}

int hd_texto(int x, int y, const char *s, u32 cor, int esc)
{
	for (; *s; s++, x += 8 * esc) {
		unsigned ch = (unsigned char)*s;
		if (ch >= 0xc0 && (s[1] & 0xc0) == 0x80) { ch = ((ch & 0x1f) << 6) | (s[1] & 0x3f); s++; }
		const char *g = ch < 128 ? font8x8_basic[ch] : (ch >= 0xa0 && ch <= 0xff) ? font8x8_ext_latin[ch - 0xa0] : font8x8_basic['?'];
		int ds = esc > 2 ? 3 : 2;
		for (int passo = 0; passo < 2; passo++)        // 0 = sombra, 1 = letra
			for (int gy = 0; gy < 8; gy++)
				for (int gx = 0; gx < 8; gx++) {
					int on = (g[gy] >> gx) & 1, neg = gx > 0 && ((g[gy] >> (gx - 1)) & 1);   // negrito: 2 px
					if (!on && !neg) continue;
					for (int yy = 0; yy < esc; yy++)
						for (int xx = 0; xx < esc; xx++) {
							if (!passo) hd_mistura(x + gx * esc + xx + ds, y + gy * esc + yy + ds, 0xff000000u, 170);
							else hd_mistura(x + gx * esc + xx, y + gy * esc + yy, cor, 255);
						}
				}
	}
	return x;
}

int hd_largura(const char *s, int esc)
{
	int n = 0;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) if ((*p & 0xc0) != 0x80) n++;
	return n * 8 * esc;
}

int hd_texto_dir(int xdir, int y, const char *s, u32 cor, int esc) { return hd_texto(xdir - hd_largura(s, esc), y, s, cor, esc); }
int hd_texto_centro(int xc, int y, const char *s, u32 cor, int esc) { return hd_texto(xc - hd_largura(s, esc) / 2, y, s, cor, esc); }

void hd_seta(int x, int yc, int dir, int tam, u32 cor)
{
	for (int i = 0; i < tam; i++)
		for (int y = yc - (tam - i); y <= yc + (tam - i); y++) hd_mistura(x + dir * i + 2, y + 2, 0xff000000u, 150);
	for (int i = 0; i < tam; i++)
		for (int y = yc - (tam - i); y <= yc + (tam - i); y++) hd_mistura(x + dir * i, y, cor, 255);
}

void hd_imagem(int x, int y, const HdImagem *im)
{
	if (!im || !im->px) return;
	for (int j = 0; j < im->h; j++)
		for (int i = 0; i < im->w; i++) {
			u32 c = im->px[j * im->w + i];
			hd_mistura(x + i, y + j, c, (int)(c >> 24));
		}
}

// maiúsculas: ASCII e acentos de 2 bytes (á é í ó ú â ê ô ã õ ç -> Á É Í Ó Ú Â Ê Ô Ã Õ Ç)
void hd_maiusc(char *dst, const char *src, int tam)
{
	int n = 0;
	for (const unsigned char *p = (const unsigned char *)src; *p && n < tam - 2; p++) {
		unsigned char c = *p;
		if (c >= 'a' && c <= 'z') c -= 32;
		else if (c == 0xc3 && p[1] >= 0xa0 && p[1] <= 0xbe && p[1] != 0xb7) { dst[n++] = (char)c; p++; c = (unsigned char)(p[0] - 0x20); }
		dst[n++] = (char)c;
	}
	dst[n] = 0;
}
