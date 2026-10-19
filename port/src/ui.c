#include "ui.h"
#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"

static u32 (*T)[NEO_W];

void ui_alvo(u32 (*tela)[NEO_W]) { T = tela; }

void ui_px(int x, int y, u32 c) { if (x >= 0 && x < NEO_W && y >= 0 && y < NEO_H) T[y][x] = c; }

void ui_mistura(int x, int y, u32 c, int alfa)
{
	if (x < 0 || x >= NEO_W || y < 0 || y >= NEO_H) return;
	u32 d = T[y][x];
	if (!(d >> 24)) { T[y][x] = (u32)(alfa > 255 ? 255 : alfa) << 24 | (c & 0xffffff); return; }   // camada transparente: fica a cor com esse alfa
	u32 r = (((c >> 16) & 255) * alfa + ((d >> 16) & 255) * (256 - alfa)) >> 8;
	u32 g = (((c >> 8) & 255) * alfa + ((d >> 8) & 255) * (256 - alfa)) >> 8;
	u32 b = ((c & 255) * alfa + (d & 255) * (256 - alfa)) >> 8;
	T[y][x] = 0xff000000u | r << 16 | g << 8 | b;
}

void ui_caixa(int x0, int y0, int x1, int y1, u32 cor, int alfa)
{
	for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) ui_mistura(x, y, cor, alfa);
}

void ui_retangulo(int x0, int y0, int x1, int y1, u32 cor, int alfa_fundo)
{
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++) {
			if (y == y0 || y == y1 || x == x0 || x == x1) ui_px(x, y, cor);
			else if (alfa_fundo) ui_mistura(x, y, cor, alfa_fundo);
		}
}

int ui_texto(int x, int y, const char *s, u32 cor)
{
	const unsigned char *p = (const unsigned char *)s;
	while (*p) {
		unsigned cp = *p++;
		if (cp >= 0xc0 && (*p & 0xc0) == 0x80) cp = ((cp & 0x1f) << 6) | (*p++ & 0x3f);
		const char *g = cp < 128 ? font8x8_basic[cp] : (cp >= 0xa0 && cp <= 0xff) ? font8x8_ext_latin[cp - 0xa0] : font8x8_basic['?'];
		for (int r = 0; r < 8; r++)
			for (int c = 0; c < 8; c++)
				if ((g[r] >> c) & 1) { ui_px(x + c + 1, y + r + 1, 0xff000000u); ui_px(x + c, y + r, cor); }
		x += 8;
	}
	return x;
}

int ui_largura(const char *s)
{
	int n = 0;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) if ((*p & 0xc0) != 0x80) n++;
	return n * 8;
}

void ui_texto_centro(int y, const char *s, u32 cor) { ui_texto((NEO_W - ui_largura(s)) / 2, y, s, cor); }
