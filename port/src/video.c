// Vídeo do Neo Geo: LSPC (VRAM, sprites, camada fixa, timer de posição) e paleta.
// Comportamento conforme ref/mame/neogeo_spr.cpp e neogeo_v.cpp (BSD-3).
#include "neo.h"
#include "m68k.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint64_t u64;

u32 neo_fb[NEO_H][NEO_W];

static u16 vram[0x8000 + 0x800];
static u16 vram_off, vram_buf, vram_mod;
static u8 anim_vel, anim_desligada, anim_quadro;
u8 video_anim_contador;
int video_shadow, video_banco_paleta, video_fix_cart;
u32 video_irq2_ctrl;
s32 video_irq2_ciclos = -1;
u32 video_contador;

static u16 pal_ram[0x2000];
static u32 rgb[0x2000], rgb_sombra[0x2000];
static u8 nivel[32][4];          // [valor 5 bits][normal, escuro, sombra, escuro+sombra]

// ---------------------------------------------------------------- paleta
static void cria_niveis(void)
{
	static const double res[5] = {3900, 2200, 1000, 470, 220};
	static const double pd[4] = {0, 1.0 / 8200, 1.0 / 150, 1.0 / 8200 + 1.0 / 150};
	double gtot = 0;
	for (int i = 0; i < 5; i++) gtot += 1.0 / res[i];
	for (int v = 0; v < 32; v++)
		for (int k = 0; k < 4; k++) {
			double g = 0;
			for (int i = 0; i < 5; i++) if (v >> i & 1) g += 1.0 / res[i];
			double x = g / (gtot + pd[k]) * 255.0 + 0.5;
			nivel[v][k] = (u8)(x > 255 ? 255 : x);
		}
}

static void atualiza_cor(u32 i)
{
	u16 d = pal_ram[i];
	int escuro = d >> 15;
	int r = ((d >> 14) & 1) | ((d >> 7) & 0x1e);
	int g = ((d >> 13) & 1) | ((d >> 3) & 0x1e);
	int b = ((d >> 12) & 1) | ((d << 1) & 0x1e);
	rgb[i] = 0xff000000u | nivel[r][escuro] << 16 | nivel[g][escuro] << 8 | nivel[b][escuro];
	rgb_sombra[i] = 0xff000000u | nivel[r][escuro + 2] << 16 | nivel[g][escuro + 2] << 8 | nivel[b][escuro + 2];
}

u16 paleta_r(u32 off) { return pal_ram[(video_banco_paleta ? 0x1000 : 0) + (off & 0xfff)]; }

void paleta_w(u32 off, u16 d, u16 mask)
{
	u32 i = (video_banco_paleta ? 0x1000 : 0) + (off & 0xfff);
	pal_ram[i] = (u16)((pal_ram[i] & ~mask) | (d & mask));
	atualiza_cor(i);
}

// ---------------------------------------------------------------- registradores
static void vram_endereco(u16 d)
{
	vram_off = (d & 0x8000) ? (u16)(d & 0x87ff) : d;
	vram_buf = vram[vram_off];
}

void video_irq2_recarregar(void)
{
	video_irq2_ciclos = (s32)((u64)(video_contador + 1) * 2 * neo_cpu_pct / 100);   // 2 ciclos por pixel a 100%
}

u16 video_reg_r(u32 off, int so_lsb)
{
	(void)so_lsb;
	switch (off & 3) {
	case 0: case 1: return vram_buf;
	case 2: return vram_mod;
	default: {
		int v = neo_linha + 0x100;
		if (v >= 0x200) v -= NEO_LINHAS;
		return (u16)((v << 7) | (video_anim_contador & 7));
	}
	}
}

void video_reg_w(u32 off, u16 d, u16 mask)
{
	if (mask == 0x00ff) return;                    // só o byte baixo: não mapeado
	if (mask == 0xff00) d = (u16)((d & 0xff00) | (d >> 8));
	switch (off & 7) {
	case 0: vram_endereco(d); break;
	case 1:
#ifdef KOF_TESTE
		{ static long vv = -2; if (vv == -2) vv = getenv("KOF_VIGIA_VRAM") ? strtol(getenv("KOF_VIGIA_VRAM"), 0, 16) : -1;
		  if (vv >= 0 && vram_off == (u32)vv) fprintf(stderr, "vram %04x <- %04x pc=%06x volta=%06x quadro=%u\n", vram_off, d, m68k_get_reg(NULL, M68K_REG_PPC), m68k_read_memory_32(m68k_get_reg(NULL, M68K_REG_SP)), neo_quadros); }
#endif
		vram[vram_off] = d;
		vram_endereco((u16)((vram_off & 0x8000) | ((vram_off + vram_mod) & 0x7fff)));
		break;
	case 2: vram_mod = d; break;
	case 3:
		anim_vel = (u8)(d >> 8);
		anim_desligada = (d >> 3) & 1;
		video_irq2_ctrl = d & 0xf0;
		m68k_end_timeslice();
		break;
	case 4: video_contador = (video_contador & 0xffff) | ((u32)d << 16); break;
	case 5:
		video_contador = (video_contador & 0xffff0000u) | d;
		if (video_irq2_ctrl & 0x20) { video_irq2_recarregar(); m68k_end_timeslice(); }
		break;
	case 6:
		if (d & 1) irq_cold = 0;
		if (d & 2) irq_raster = 0;
		if (d & 4) irq_vblank = 0;
		neo_atualiza_irq();
		break;
	default: break;
	}
}

void video_vsync(void)
{
	if (anim_quadro == 0) { anim_quadro = anim_vel; video_anim_contador++; }
	else anim_quadro--;
}

void video_init(void) { cria_niveis(); }

void video_reset(void)
{
	memset(vram, 0, sizeof vram);
	memset(pal_ram, 0, sizeof pal_ram);
	for (u32 i = 0; i < 0x2000; i++) atualiza_cor(i);
	vram_off = vram_buf = vram_mod = 0;
	anim_vel = anim_desligada = anim_quadro = 0;
	video_anim_contador = 0;
	video_shadow = video_banco_paleta = video_fix_cart = 0;
	video_irq2_ctrl = 0;
	video_irq2_ciclos = -1;
	video_contador = 0;
}

// ---------------------------------------------------------------- sprites
#define MAX_SPRITES_LINHA 96
int video_limite_linha = MAX_SPRITES_LINHA;   // 96 = hardware; MAX_SPRITES_TELA = sem limite (sem sprites sumindo)
static u16 listas[2][MAX_SPRITES_TELA + 2];   // lista de sprites de cada linha (par/ímpar)
static int nlista[2];                         // quantos sprites de verdade em cada lista

static const u16 zoom_x_tab[16] = {
	0x0080, 0x0880, 0x0888, 0x2888, 0x288a, 0x2a8a, 0x2aaa, 0xaaaa,
	0xaaea, 0xbaea, 0xbaeb, 0xbbeb, 0xbbef, 0xfbef, 0xfbff, 0xffff };

static inline int sprite_na_linha(int linha, int y, int linhas)
{
	return linhas == 0 || linhas >= 0x20 || ((linha - y) & 0x1ff) < linhas * 0x10;
}

static void ovl_zera(void);
void video_linha_inicio(int linha)
{
	if (linha == 16) ovl_zera();
	u16 *lista = listas[linha & 1];
	int y = 0, linhas = 0, n = 0;
	for (u16 s = 0; s < MAX_SPRITES_TELA; s++) {
		u16 yc = vram[0x8200 | s];
		if (!(yc & 0x40)) { y = 0x200 - (yc >> 7); linhas = yc & 0x3f; }
		if (linhas == 0) continue;
		if (!sprite_na_linha(linha, y, linhas)) continue;
		*lista++ = s;
		if (++n == video_limite_linha) break;
	}
	memset(lista, 0, sizeof(u16) * (video_limite_linha - n + 1));
	nlista[linha & 1] = n;
}

static inline u8 pixel_sprite(u32 romaddr)
{
	romaddr &= (R.spr_mask << 1) | 1;            // endereço em pixels: 2 por byte
	const u8 *src = R.spr + ((((romaddr & ~0xffu) >> 1) | (((romaddr & 8) ^ 8) << 3) | ((romaddr & 0xf0) >> 2)) & R.spr_mask);
	int x = romaddr & 7;
	return (u8)((((src[3] >> x) & 1) << 3) | (((src[1] >> x) & 1) << 2) | (((src[2] >> x) & 1) << 1) | ((src[0] >> x) & 1));
}

static u32 linha_buf[512];
static u8 linha_ovl[512];     // alvo de foto que pintou por último cada pixel desta linha (0 = nenhum)

// Tiles trocados (arte/troca_tiles.bin, tools/retratos.py): o tile da ROM com esse código é pintado com
// estes 16x16 pixels ARGB em vez da paleta; alfa 0 = pixel original, alfa 1 = transparente. Usado nos retratos personalizados.
#define NTROCA 1024
static struct { u32 code; u32 px[256]; } *troca;
static int ntroca;
static int troca_hash[NTROCA * 2];     // índice+1 em troca[], 0 = vazio
static const u32 *troca_busca(u32 code)
{
	if (!ntroca) return NULL;
	for (u32 h = (code * 2654435761u) % (NTROCA * 2);; h = (h + 1) % (NTROCA * 2)) {
		int i = troca_hash[h];
		if (!i) return NULL;
		if (troca[i - 1].code == code) return troca[i - 1].px;
	}
}
static int troca_carrega_bytes(const u8 *d, size_t n)
{
	free(troca);
	troca = malloc(sizeof *troca * NTROCA);
	ntroca = 0;
	memset(troca_hash, 0, sizeof troca_hash);
	for (size_t i = 0; ntroca < NTROCA && i + sizeof *troca <= n; i += sizeof *troca) {
		memcpy(&troca[ntroca], d + i, sizeof *troca);
		u32 h = (troca[ntroca].code * 2654435761u) % (NTROCA * 2);
		while (troca_hash[h]) h = (h + 1) % (NTROCA * 2);
		troca_hash[h] = ++ntroca;
	}
	return ntroca;
}
// Fotos em alta resolução (arte/retratos.json -> bin/arte/fotos.bin): o renderizador só anota, por quadro, onde os
// tiles de cada foto foram pintados (caixa por lado da tela e se estavam espelhados); o main.c desenha a foto por
// cima, no tamanho da janela, sem passar pelos 320x224.
#define NFOTO 64
OvlCaixa video_ovl[NFOTO][2];
int video_ovl_n;
static struct { u32 code; u8 alvo; s16 dx, dy; } ovl_tab[2048];
static int ovl_idx_atual;   // índice em ovl_tab do tile sendo pintado
static int ovl_hash[4096];
static int ovl_busca(u32 code)
{
	if (!video_ovl_n) return 0;
	for (u32 h = (code * 2654435761u) % 4096;; h = (h + 1) % 4096) {
		int i = ovl_hash[h];
		if (!i) return 0;
		if (ovl_tab[i - 1].code == code) { ovl_idx_atual = i - 1; return ovl_tab[i - 1].alvo; }
	}
}
// formato: u32 n_alvos; por alvo: u32 n_codes, u32 codes[]  (alvo = índice + 1)
int video_ovl_carrega_bytes(const u8 *d, size_t n)
{
	memset(ovl_hash, 0, sizeof ovl_hash);
	video_ovl_n = 0;
	int nt = 0;
	size_t i = 4;
	if (n < 4) return 0;
	u32 na; memcpy(&na, d, 4);
	for (u32 a = 0; a < na && a < NFOTO && i + 4 <= n; a++) {
		u32 nc; memcpy(&nc, d + i, 4); i += 4;
		for (u32 k = 0; k < nc && i + 8 <= n && nt < 2048; k++, i += 8) {
			u32 c; s16 dx, dy; memcpy(&c, d + i, 4); memcpy(&dx, d + i + 4, 2); memcpy(&dy, d + i + 6, 2);
			ovl_tab[nt].code = c; ovl_tab[nt].alvo = (u8)(a + 1); ovl_tab[nt].dx = dx; ovl_tab[nt].dy = dy;
			u32 h = (c * 2654435761u) % 4096;
			while (ovl_hash[h]) h = (h + 1) % 4096;
			ovl_hash[h] = ++nt;
		}
		video_ovl_n = (int)a + 1;
	}
	return video_ovl_n;
}
int video_ovl_carrega(const char *arq)
{
	FILE *f = fopen(arq, "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *d = malloc((size_t)n);
	size_t lidos = fread(d, 1, (size_t)n, f);
	fclose(f);
	int r = video_ovl_carrega_bytes(d, lidos);
	free(d);
	return r;
}
static void ovl_zera(void) { memset(video_ovl, 0, sizeof video_ovl); for (int a = 0; a < NFOTO; a++) for (int l = 0; l < 2; l++) { video_ovl[a][l].x0 = video_ovl[a][l].y0 = 9999; video_ovl[a][l].ox = video_ovl[a][l].oy = 9999; video_ovl[a][l].rx = -9999; video_ovl[a][l].vx0 = video_ovl[a][l].vy0 = 9999; video_ovl[a][l].vx1 = video_ovl[a][l].vy1 = -1; } }
// um tile da arte foi pintado: x = esquerda do tile na tela, ytopo = linha de cima do tile, esp = espelhado
static inline void ovl_marca_tile(int alvo, int idx, int x, int ytopo, int esp)
{
	OvlCaixa *c = &video_ovl[alvo - 1][x < NEO_W / 2 ? 0 : 1];
	int dx = ovl_tab[idx].dx, dy = ovl_tab[idx].dy;
	if (!esp) { if (x - dx < c->ox) c->ox = x - dx; }
	else { if (x + 16 + dx > c->rx) c->rx = x + 16 + dx; }     // espelhado: a origem é a borda direita
	if (ytopo - dy < c->oy) c->oy = ytopo - dy;
	if (x < c->x0) c->x0 = x; if (x + 15 > c->x1) c->x1 = x + 15;
	if (ytopo < c->y0) c->y0 = ytopo; if (ytopo + 15 > c->y1) c->y1 = ytopo + 15;
	c->n++; c->esp += esp;
}

#ifdef KOF_SOLO
extern const unsigned char embutido_tiles[];
extern const size_t embutido_tiles_tam;
int video_troca_embutida(void) { return troca_carrega_bytes(embutido_tiles, embutido_tiles_tam); }
#endif
int video_troca_carrega(const char *arq)
{
	FILE *f = fopen(arq, "rb");
	if (!f) return 0;
	free(troca);
	troca = malloc(sizeof *troca * NTROCA);
	ntroca = 0;
	memset(troca_hash, 0, sizeof troca_hash);
	while (ntroca < NTROCA && fread(&troca[ntroca], sizeof *troca, 1, f) == 1) {
		u32 h = (troca[ntroca].code * 2654435761u) % (NTROCA * 2);
		while (troca_hash[h]) h = (h + 1) % (NTROCA * 2);
		troca_hash[h] = ++ntroca;
	}
	fclose(f);
	return ntroca;
}
#ifdef KOF_TESTE
static u32 mapa_buf[512];     // KOF_MAPA="quadro,arq": tile (code<<8 | linha<<4 | coluna) que pintou cada pixel
#endif

static void desenha_sprites(int linha, const u32 *pens)
{
	u16 *lista = listas[linha & 1];
	// Melhoria: só os sprites que estão na lista. O hardware (e o MAME) continua desenhando o sprite 0
	// nas posições vazias do fim da lista; no Anniversary o sprite 0 é um tile preto em x=37 e virava
	// uma faixa preta vertical (cenário da rua com carros).
	int maximo = nlista[linha & 1] - 1;

	int y = 0, x = 0, linhas = 0, zoom_y = 0, zoom_x = 0;
	for (int idx = 0; idx <= maximo; idx++) {
		u16 s = lista[idx] & 0x1ff;
		u16 yc = vram[0x8200 | s];
		u16 zc = vram[0x8000 | s];
		if (yc & 0x40) {
			x = (x + zoom_x + 1) & 0x1ff;
			zoom_x = (zc >> 8) & 0x0f;
		} else {
			y = 0x200 - (yc >> 7);
			x = vram[0x8400 | s] >> 7;
			zoom_y = zc & 0xff;
			zoom_x = (zc >> 8) & 0x0f;
			linhas = yc & 0x3f;
		}
		if (x >= 0x140 && x <= 0x1f0) continue;
		if (!sprite_na_linha(linha, y, linhas)) continue;

		int sl = (linha - y) & 0x1ff;
		int zl = sl & 0xff;
		int inv = (sl >> 8) & 1;
		if (inv) zl ^= 0xff;
		if (linhas > 0x20) {
			zl = zl % ((zoom_y + 1) << 1);
			if (zl > zoom_y) { zl = ((zoom_y + 1) << 1) - 1 - zl; inv = !inv; }
		}
		u8 yt = R.zoom[(zoom_y << 8) | zl];
		int sy = yt & 0x0f, tile = yt >> 4;
		if (inv) { sy ^= 0x0f; tile ^= 0x1f; }
		u32 o = ((u32)s << 6) | ((u32)tile << 1);
		u16 attr = vram[o + 1];
		u32 code = (((u32)attr << 12) & 0xf0000) | vram[o];
		if (!anim_desligada) {
			if (attr & 8) code = (code & ~7u) | (video_anim_contador & 7);
			else if (attr & 4) code = (code & ~3u) | (video_anim_contador & 3);
		}
		if (attr & 2) sy ^= 0x0f;
		u16 zx = zoom_x_tab[zoom_x];
		u32 gfx = (code << 8) | ((u32)sy << 4);
		int inc = 1;
		if (attr & 1) { gfx += 0x0f; inc = -1; }
		const u32 *lp = pens + ((attr >> 8) << 4);
		const u32 *tr = troca_busca(code);
		int ov = ovl_busca(code);
		if (ov && x < NEO_W && zoom_x == 0x0f) ovl_marca_tile(ov, ovl_idx_atual, x, linha - 16 - sy, attr & 1);
		int px = x;
#ifdef KOF_TESTE
		static int spr_l = -2, spr_q; if (spr_l == -2) { spr_l = getenv("KOF_SPR") ? atoi(getenv("KOF_SPR")) : -1; spr_q = getenv("KOF_SPR_Q") ? atoi(getenv("KOF_SPR_Q")) : 0; }
		if (spr_l >= 0 && linha == spr_l && neo_quadros + 1 >= (u32)spr_q) {
			int larg = 0; for (u16 t = zx; t; t = (u16)(t << 1)) if (t & 0x8000) larg++;
			if (getenv("KOF_SPR_TUDO") || ((40 - x) & 0x1ff) < 16 || ((48 - x) & 0x1ff) < 16)
				fprintf(stderr, "spr %3d x=%3d larg=%2d zx=%x zy=%02x linhas=%02x code=%05x attr=%04x yc=%04x pal=%02x\n", s, x, larg, zoom_x, zoom_y, linhas, code, attr, yc, attr >> 8);
		}
#endif
		for (int i = 0; i < 16; i++) {
			if (zx & 0x8000) {
				u8 c = pixel_sprite(gfx);
				u32 t = tr ? tr[(sy << 4) | (gfx & 0x0f)] : 0;
				if (t >> 24 > 1) { linha_buf[px & 0x1ff] = t; linha_ovl[px & 0x1ff] = (u8)ov; }      // cor do retrato
				else if (!t && c) { linha_buf[px & 0x1ff] = lp[c]; linha_ovl[px & 0x1ff] = (u8)ov; }  // alfa 0 = original; alfa 1 = transparente
#ifdef KOF_TESTE
				if (c) mapa_buf[px & 0x1ff] = code << 8 | (u32)sy << 4 | (gfx & 0x0f);
#endif
				px++;
			}
			zx = (u16)(zx << 1);
			if (!zx) break;
			gfx += (u32)inc;
		}
	}
}

static void desenha_fix(int linha, const u32 *pens)
{
	const u8 *base = video_fix_cart ? R.s1 : R.sfix;
	u32 mask = (video_fix_cart ? R.s1_size : 0x20000) - 1;
	static const u32 desl[4] = {0x10, 0x18, 0x00, 0x08};
	const u16 *vd = &vram[0x7000 | (linha >> 3)];
	for (int col = 0; col < 40; col++, vd += 0x20) {
		u16 cp = *vd;
		u32 code = cp & 0x0fff;
		u32 g = ((code << 5) | (linha & 7)) & mask;
		const u32 *cpens = pens + ((cp >> 12) << 4);
		u32 *dst = &linha_buf[col * 8];
		for (int i = 0; i < 4; i++) {
			u8 d = base[(g + desl[i]) & mask];
			if (d & 0x0f) dst[i * 2] = cpens[d & 0x0f];
			if (d & 0xf0) dst[i * 2 + 1] = cpens[d >> 4];
		}
	}
}

void video_linha_fim(int linha)
{
	if (linha < 16 || linha >= 16 + NEO_H) return;
	const u32 *pens = (video_shadow ? rgb_sombra : rgb) + (video_banco_paleta ? 0x1000 : 0);
	u32 fundo = pens[0xfff];
	for (int i = 0; i < 512; i++) linha_buf[i] = fundo;
	memset(linha_ovl, 0, sizeof linha_ovl);
#ifdef KOF_TESTE
	memset(mapa_buf, 0xff, sizeof mapa_buf);
#endif
	desenha_sprites(linha, pens);
	for (int x = 0; x < NEO_W; x++) if (linha_ovl[x]) {        // parte da arte que ficou visível (o resto foi coberto)
		OvlCaixa *c = &video_ovl[linha_ovl[x] - 1][x < NEO_W / 2 ? 0 : 1];
		int y = linha - 16;
		if (x < c->vx0) c->vx0 = x; if (x > c->vx1) c->vx1 = x;
		if (y < c->vy0) c->vy0 = y; if (y > c->vy1) c->vy1 = y;
	}
#ifdef KOF_TESTE
	{
		static FILE *fm; static long q = -2; static char arq[300];
		if (q == -2) { q = -1; const char *e = getenv("KOF_MAPA"); if (e) sscanf(e, "%ld,%299s", &q, arq); }
		if (q >= 0 && neo_quadros + 1 == (u32)q) {
			if (!fm) fm = fopen(arq, "wb");
			if (fm) { fwrite(mapa_buf, 4, NEO_W, fm); if (linha == 16 + NEO_H - 1) { fclose(fm); fm = NULL; q = -1; } }
		}
	}
#endif
	desenha_fix(linha, pens);
	memcpy(neo_fb[linha - 16], linha_buf, sizeof(u32) * NEO_W);
}

void video_estado(Estado *e)
{
	EST(e, vram); EST(e, vram_off); EST(e, vram_buf); EST(e, vram_mod);
	EST(e, anim_vel); EST(e, anim_desligada); EST(e, anim_quadro); EST(e, video_anim_contador);
	EST(e, video_shadow); EST(e, video_banco_paleta); EST(e, video_fix_cart);
	EST(e, video_irq2_ctrl); EST(e, video_irq2_ciclos); EST(e, video_contador); EST(e, pal_ram);
	if (!e->salvar) for (u32 i = 0; i < 0x2000; i++) atualiza_cor(i);
}

#ifdef KOF_TESTE
#include <stdio.h>
void video_despeja_fix(const char *arq)
{
	FILE *f = fopen(arq, "w");
	if (!f) return;
	for (int lin = 0; lin < 32; lin++) {
		for (int col = 0; col < 40; col++) fprintf(f, "%04x ", vram[0x7000 + col * 32 + lin]);
		fprintf(f, "\n");
	}
	fprintf(f, "7500:");
	for (int i = 0; i < 0x100; i++) fprintf(f, " %04x", vram[0x7500 + i]);
	fprintf(f, "\n");
	fclose(f);
}
#endif
