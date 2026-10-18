// Menu inicial "KOF '98 Ultimate Match - Comunidade Edition": arte em 1672x941 (arte/menu_fundo.bmp, gerada
// pelo usuário) com as opções desenhadas por cima. O jogo continua rodando por trás: um piloto aperta Start
// na tela de título e espera no MODE SELECT do console; ao escolher, ele desce até o item e confirma com A
// (em avanço rápido, escondido atrás do menu) e só então mostra o jogo. Esc no jogo volta pra cá.
//
// MODE SELECT do Anniversary (AES): 0 Team Play, 1 Team VS, 2 Single Play, 3 Single VS, 4 Survivor,
// 5 Practice, 6 Option. Tempos do piloto = bin/roteiro_pratica.txt (Start no quadro 700, menu pronto no 960).
#include "frontend.h"
#include "config.h"
extern char font8x8_basic[128][8];   // definida em ui.c (font8x8_basic.h)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define AW 1672
#define AH 941
#define Q_START 700           // Start na tela de título
#define Q_MENU 960            // MODE SELECT já aceita o direcional
#define PASSO 16              // quadros entre um toque e outro no MODE SELECT
// RAM do Anniversary (achado 18/10 por diff de dumps): tela atual, cursor do MODE SELECT, modo confirmado, OPTION
#define RAM_TELA 0x10A790     // 0 título, 1 MODE SELECT (e OPTION), 2 jogo
#define RAM_CURSOR 0x1085D2   // 0-6 = item do MODE SELECT
#define RAM_FASE 0x10A789     // 00 parado no MODE SELECT; 0x80 ao confirmar um modo (e dentro do OPTION); 03/83 no título
#define RAM_OPTION 0x10051C   // 1 dentro do OPTION

int frontend_ativo;
static SDL_Texture *tex;
static u32 *fundo, *tela;     // arte pura / quadro composto (ARGB)
static int sel = 0, sujo = 1, pisca;
static u8 antes[8];           // estado anterior das teclas do menu (borda de subida)

// itens: linha, coluna, texto, ação (item do MODE SELECT; -1 = assistir, -2 = sair, -3 = em breve)
typedef struct { int linha, col; const char *txt; int modo; } Item;
static const Item ITENS[] = {
	{0, 0, "ARCADE PLAY", 2}, {1, 0, "TEAM PLAY", 0}, {1, 1, "2P VS", 1}, {2, 0, "SURVIVAL", 4},
	{2, 1, "COOP", -3}, {3, 0, "PRACTICE", 5}, {3, 1, "OPTIONS", 6}, {4, 0, "WATCH", -1}, {4, 1, "EXIT", -2},
};
#define NITENS (int)(sizeof ITENS / sizeof ITENS[0])
static const int LINHA_Y[5] = {200, 272, 345, 420, 493};   // centro de cada linha (mockup)

// piloto: 0 desligado, 1 indo pro MODE SELECT, 2 escolhendo, 3 assistindo (sem Start)
static int piloto, alvo;
static int cursor_ini;        // item onde o cursor do MODE SELECT estava ao escolher (voltando do OPTION fica no 6)
static int vigia_n;           // quadros seguidos com o MODE SELECT original na tela
static u32 q_rel, q_escolha, q_som;   // quadros desde que o menu abriu (neo_reset não zera neo_quadros); q_som = confirmação

#ifdef KOF_SOLO
#include "stb_image.h"
extern const unsigned char embutido_menu_png[];
extern const size_t embutido_menu_png_tam;
int frontend_carrega(const char *base)
{
	(void)base;
	int w, h, n;
	u8 *px = stbi_load_from_memory(embutido_menu_png, (int)embutido_menu_png_tam, &w, &h, &n, 4);
	if (!px || w != AW || h != AH) { stbi_image_free(px); return 0; }
	fundo = malloc(AW * AH * 4);
	tela = malloc(AW * AH * 4);
	for (int i = 0; i < AW * AH; i++) fundo[i] = 0xff000000u | (u32)px[i * 4] << 16 | (u32)px[i * 4 + 1] << 8 | px[i * 4 + 2];
	stbi_image_free(px);
	return 1;
}
#else
int frontend_carrega(const char *base)
{
	char arq[700];
	snprintf(arq, sizeof arq, "%sarte/menu_fundo.bmp", base ? base : "");
	SDL_Surface *s = SDL_LoadBMP(arq);
	if (!s) return 0;
	SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(s);
	if (!c || c->w != AW || c->h != AH) { if (c) SDL_FreeSurface(c); return 0; }
	fundo = malloc(AW * AH * 4);
	tela = malloc(AW * AH * 4);
	for (int y = 0; y < AH; y++) memcpy(fundo + y * AW, (u8 *)c->pixels + y * c->pitch, AW * 4);
	SDL_FreeSurface(c);
	return 1;
}
#endif

// ---------------------------------------------------------------- desenho em 1672x941
static void mistura(int x, int y, u32 c, int a)
{
	if (x < 0 || x >= AW || y < 0 || y >= AH || a <= 0) return;
	if (a > 255) a = 255;
	u32 d = tela[y * AW + x];
	u32 r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
	u32 g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
	u32 b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
	tela[y * AW + x] = 0xff000000u | r << 16 | g << 8 | b;
}

// faixa inclinada (paralelogramo): x0..x1 na linha de baixo, inclina pra direita subindo
static void faixa(int x0, int x1, int yc, int alt, u32 cor, int a0, int a1)
{
	for (int y = yc - alt / 2; y < yc + alt / 2; y++) {
		int d = (yc + alt / 2 - y) * 2 / 5;
		for (int x = x0 + d; x < x1 + d; x++) mistura(x, y, cor, a0 + (a1 - a0) * (x - x0 - d) / (x1 - x0));
	}
}

static int texto(int x, int y, const char *s, u32 cor, int esc)
{
	for (; *s; s++, x += 8 * esc) {
		unsigned ch = (unsigned char)*s;
		if (ch >= 128) continue;
		const char *g = font8x8_basic[ch];
		for (int gy = 0; gy < 8; gy++)
			for (int gx = 0; gx < 8; gx++) {
				int on = (g[gy] >> gx) & 1, neg = gx > 0 && ((g[gy] >> (gx - 1)) & 1);   // negrito: 2 px
				if (!on && !neg) continue;
				for (int yy = 0; yy < esc; yy++)
					for (int xx = 0; xx < esc; xx++) {
						mistura(x + gx * esc + xx + 3, y + gy * esc + yy + 3, 0xff000000u, 170);   // sombra
						tela[(y + gy * esc + yy) * AW + x + gx * esc + xx] = cor;
					}
			}
	}
	return x;
}
static int largura(const char *s, int esc) { return (int)strlen(s) * 8 * esc; }

static void seta(int x, int yc, int dir, u32 cor)   // triângulo ◀ (dir -1) ou ▶ (1)
{
	for (int i = 0; i < 14; i++)
		for (int y = yc - (14 - i); y <= yc + (14 - i); y++) mistura(x + dir * i, y, cor, 255);
}

static void compoe(void)
{
	memcpy(tela, fundo, AW * AH * 4);
	for (int l = 1; l < 5; l++) {                     // linhas com 2 opções: faixa escura + traço laranja
		faixa(120, 850, LINHA_Y[l], 56, 0xff07080a, 200, 120);
		for (int x = 130; x < 850; x++) for (int t = 0; t < 3; t++) mistura(x + 4, LINHA_Y[l] + 30 + t, 0xffff5a1e, 200);
	}
	for (int i = 0; i < NITENS; i++) {
		const Item *it = &ITENS[i];
		int y = LINHA_Y[it->linha], escolhido = i == sel, emb = it->modo == -3;
		u32 cor = emb ? 0xff6a6460u : escolhido ? 0xffffffffu : 0xfff0e6dcu;
		int x = it->col ? 790 - largura(it->txt, 4) : 152;
		if (escolhido) {
			int x0 = it->linha == 0 ? 60 : it->col ? 470 : 110, x1 = it->linha == 0 ? 760 : it->col ? 860 : 500;
			faixa(x0, x1, y, it->linha == 0 ? 60 : 56, 0xffff4b14, 245, 30);
			u32 sc = (pisca / 20) & 1 ? 0xffffffffu : 0xffffd0b0u;
			seta(x - 22, y, -1, sc);
			seta(x + largura(it->txt, 4) + 22, y, 1, sc);
		}
		texto(x, y - 16, it->txt, cor, 4);
		if (emb && escolhido) texto(x + largura(it->txt, 4) - 130, y + 18, "EM BREVE", 0xffffb080u, 2);
	}
	texto(60, 40, "SETAS escolhem   A / ENTER confirma   ESC no jogo volta aqui", 0xffb8b0a8u, 2);
	if (piloto == 2) {                                 // avanço rápido acontecendo por trás
		const char *c = "CARREGANDO...";
		faixa(560, 1110, 470, 90, 0xff000000, 220, 220);
		texto((AW - largura(c, 5)) / 2, 450, c, 0xffffffffu, 5);
	}
}

// ---------------------------------------------------------------- lógica
void frontend_abre(void)
{
	if (!fundo) return;
	neo_reset();
	frontend_ativo = 1;
	piloto = 1;
	q_rel = 0;
	sujo = 1;
	memset(antes, 1, sizeof antes);   // não aceita a tecla que ainda está apertada
}

static void escolhe(int *rodando)
{
	const Item *it = &ITENS[sel];
	if (it->modo == -3) return;
	if (it->modo == -2) { *rodando = 0; return; }
	if (it->modo == -1) { neo_reset(); piloto = 3; frontend_ativo = 0; return; }
	alvo = it->modo;
	cursor_ini = (int)m68k_read_memory_8(RAM_CURSOR);
	if (cursor_ini < 0 || cursor_ini > 6 || m68k_read_memory_8(RAM_TELA) != 1) cursor_ini = 0;
	piloto = 2;
	q_escolha = 0;
	q_som = q_rel;
	audio_comando(0x71);               // som de confirmação do próprio MODE SELECT
	sujo = 1;
}

static int mover(int dl, int dc)
{
	const Item *a = &ITENS[sel];
	int l = a->linha + dl, c = dc ? !a->col : a->col;
	if (l < 0) l = 4;
	if (l > 4) l = 0;
	for (int i = 0; i < NITENS; i++) if (ITENS[i].linha == l && ITENS[i].col == c) return i;
	for (int i = 0; i < NITENS; i++) if (ITENS[i].linha == l) return i;   // linha de 1 opção só
	return sel;
}

void frontend_controles(int *rodando)
{
	const u8 *k = SDL_GetKeyboardState(NULL);
	u8 agora[8] = {
		k[SDL_SCANCODE_UP] || acao_ativa[0][AC_CIMA] || acao_ativa[1][AC_CIMA],
		k[SDL_SCANCODE_DOWN] || acao_ativa[0][AC_BAIXO] || acao_ativa[1][AC_BAIXO],
		k[SDL_SCANCODE_LEFT] || acao_ativa[0][AC_ESQ] || acao_ativa[1][AC_ESQ],
		k[SDL_SCANCODE_RIGHT] || acao_ativa[0][AC_DIR] || acao_ativa[1][AC_DIR],
		k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER] || k[SDL_SCANCODE_Z] || acao_ativa[0][AC_A] || acao_ativa[1][AC_A] ||
			acao_ativa[0][AC_START] || acao_ativa[1][AC_START],
	};
	int novo[8];
	for (int i = 0; i < 8; i++) { novo[i] = agora[i] && !antes[i]; antes[i] = agora[i]; }
	if (piloto == 2) return;                           // já escolhido: esperando o jogo chegar lá
	int s = sel;
	if (novo[0]) s = mover(-1, 0);
	if (novo[1]) s = mover(1, 0);
	if (novo[2] || novo[3]) s = mover(0, 1);
	if (s != sel) { sel = s; sujo = 1; audio_comando(0x70); }   // som de cursor do próprio MODE SELECT
	if (novo[4]) escolhe(rodando);
}

int frontend_tem_arte(void) { return fundo != NULL; }

// O MODE SELECT original nunca fica na tela: sempre que o jogo chega nele por conta própria (Start no título,
// volta do OPTION, fim de jogo), o nosso menu abre por cima, sem reset, já pronto pra escolher.
void frontend_vigia(void)
{
	if (!fundo || frontend_ativo || piloto) { vigia_n = 0; return; }
	int ms = m68k_read_memory_8(RAM_TELA) == 1 && !m68k_read_memory_8(RAM_FASE) && !m68k_read_memory_8(RAM_OPTION);
	if (!ms) { vigia_n = 0; return; }
	if (++vigia_n < 6) return;
	vigia_n = 0;
	frontend_ativo = 1;
	piloto = 1;
	q_rel = Q_MENU;              // já está no MODE SELECT: nada de Start
	sujo = 1;
	memset(antes, 1, sizeof antes);
}
int frontend_passos(void) { return piloto == 2 && q_rel - q_som >= 30 ? 12 : 1; }   // 30 quadros normais: o som de confirmar toca inteiro

int frontend_piloto(void)
{
	if (!piloto) return 0;
	neo_in_p1 = neo_in_p2 = 0xff; neo_in_start = 0x0f; neo_in_coin = 0x07; neo_in_test = 1;
	u32 q = q_rel++;
	if (piloto == 3) { piloto = 0; return 0; }        // assistir: só não aperta nada no primeiro quadro
	if (q >= Q_START && q < Q_START + 5) neo_in_start &= (u8)~1;
	if (piloto == 2) {
		if (q < Q_MENU) return 1;
		if (!q_escolha) q_escolha = q;
		u32 r = q - q_escolha;
		int toques = alvo - cursor_ini, bit = 2;                 // baixo; voltando do OPTION o cursor está no 6: sobe
		if (toques < 0) { toques = -toques; bit = 1; }
		for (int i = 0; i < toques; i++) if (r >= (u32)(i * PASSO) && r < (u32)(i * PASSO + 4)) neo_in_p1 &= (u8)bit;
		u32 qa = (u32)(toques * PASSO + 8);
		if (r >= qa && r < qa + 5) neo_in_p1 &= (u8)~0x10;                                                           // A
		if (r >= qa + 30) { piloto = 0; frontend_ativo = 0; }
	}
	return 1;
}

void frontend_desenha(SDL_Renderer *ren)
{
	pisca++;
	if (!tex) tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, AW, AH);
	if (sujo || pisca % 20 == 0 || piloto == 2) { compoe(); SDL_UpdateTexture(tex, NULL, tela, AW * 4); sujo = 0; }
	int ow, oh;
	SDL_GetRendererOutputSize(ren, &ow, &oh);
	SDL_Rect r = {0, 0, ow, ow * AH / AW};
	if (r.h > oh) { r.h = oh; r.w = oh * AW / AH; }
	r.x = (ow - r.w) / 2; r.y = (oh - r.h) / 2;
	SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, &r);
	SDL_RenderPresent(ren);
}

#ifdef KOF_TESTE
void frontend_escolhe_teste(int item) { sel = item; int r = 1; escolhe(&r); }
void frontend_teste_zera(void) { frontend_ativo = 0; piloto = 0; }
void frontend_teste_arte(void) { if (!fundo) fundo = calloc(AW * AH, 4); if (!tela) tela = calloc(AW * AH, 4); }
#endif
