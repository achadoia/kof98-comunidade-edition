// KOF98 port nativo — janela, vídeo (filtros), som, controles e laço de 60 Hz (SDL2).
// Uso: kof98.exe [zip ...] [--arcade|--console] [--carregar-estado arq]
// CPU do 68000 fixa em 200% (sem a lentidão do Neo Geo).
//      teste sem janela: --sem-janela --quadros N [--foto x.bmp] [--wav x.wav] [--salvar-estado arq]
//                        [--seg-entradas "ini-fim:p1=bits,..."]   (env KOF_TREINO, KOF_CFG, KOF_DUMP)
// Sem zips na linha de comando procura roms\kof98aemod.zip e roms\neogeo.zip ao lado do EXE
// (e os caminhos do FBNeo/MAME desta máquina).
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "neo.h"
#include "m68k.h"
#include "treino.h"
#include "config.h"
#include "entrada.h"
#include "menu_cfg.h"
#include "ui.h"
#include "hd.h"
#include "musica.h"
#include "efeitos.h"
#include "frontend.h"
#include "ordem.h"
#include "rede.h"
#include "atualiza.h"
#include "versao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <math.h>
#include "stb_image.h"

char cfg_arquivo[512] = "kof98.ini";
static u32 tela[NEO_H][NEO_W];
static u32 tela_ui[NEO_H][NEO_W];   // interface do EXE (treino, F1, tela de ordem): camada transparente desenhada por cima das fotos

// ---------------------------------------------------------------- roteiro de entradas (modo de teste)
typedef struct { u32 ini, fim; u8 *alvo; u8 bits; } Passo;
static Passo roteiro[256];
static int nroteiro;

static void le_roteiro(const char *s)
{
	while (s && *s && nroteiro < 256) {
		unsigned ini, fim, bits;
		char alvo[4] = {0};
		if (sscanf(s, "%u-%u:%2[a-z0-9]=%x", &ini, &fim, alvo, &bits) == 4) {
			Passo *p = &roteiro[nroteiro++];
			p->ini = ini; p->fim = fim; p->bits = (u8)bits;
			p->alvo = !strcmp(alvo, "p1") ? &neo_in_p1 : !strcmp(alvo, "p2") ? &neo_in_p2 :
			          !strcmp(alvo, "st") ? &neo_in_start : &neo_in_coin;
		}
		s = strchr(s, ',');
		if (s) s++;
	}
}

static void aplica_roteiro(void)
{
	for (int i = 0; i < nroteiro; i++)
		if (neo_quadros >= roteiro[i].ini && neo_quadros <= roteiro[i].fim) *roteiro[i].alvo &= (u8)~roteiro[i].bits;
}

static void salva_bmp(const char *nome)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(tela, NEO_W, NEO_H, 32, NEO_W * 4, SDL_PIXELFORMAT_ARGB8888);
	SDL_SaveBMP(s, nome);
	SDL_FreeSurface(s);
}

static int existe(const char *p) { FILE *f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

// ---------------------------------------------------------------- run-ahead
// Emula N quadros à frente com a entrada atual, mostra o último e volta ao estado real: a imagem fica 1-2
// quadros adiantada em relação ao atraso interno do jogo. O som sai do quadro real (já lido antes de chamar).
static void run_ahead(int n)
{
	estado_leve = 1; musica_silenciosa = 1;
	estado_mem_salvar();
	for (int k = 0; k < n; k++) { config_aplica_cenario(); neo_frame(); treino_depois_quadro(); }
	estado_mem_carregar();
	audio_descarta();
	musica_silenciosa = 0; estado_leve = 0;
}

// ---------------------------------------------------------------- rollback online
// Volta ao estado do início do quadro k e re-simula até o quadro atual com as entradas que chegaram de
// verdade (o som desses quadros é descartado: o que já tocou, tocou).
static void resimula_rede(u32 k)
{
	extern int estado_ring_carregar(u32); extern void estado_ring_salvar(u32);
	if (estado_ring_carregar(k)) { fprintf(stderr, "rollback: sem estado do quadro %u\n", k); rede_dessincronizou = 1; return; }
	musica_silenciosa = 1; estado_leve = 1;
	for (u32 q = k; q < rede_quadro(); q++) {
		u8 p1, x1, p2, x2;
		rede_entradas_de(q, &p1, &x1, &p2, &x2);
		neo_in_p1 = p1; neo_in_p2 = p2; neo_in_start = (u8)(0x0f & ~((x1 & REDE_X_START) ? 1 : 0) & ~((x2 & REDE_X_START) ? 4 : 0));
		neo_in_coin = 0x07; neo_in_test = 1;
		estado_ring_salvar(q);
		if (rede_quadro_real(q)) ordem_entradas();
		neo_frame();
		rede_hash_depois(q + 1);
	}
	audio_descarta();
	musica_silenciosa = 0; estado_leve = 0;
}
static int chat_digitando; static char chat_texto[72];

// ---------------------------------------------------------------- ritmo do monitor
// 59,19 Hz numa tela de 60 Hz = um quadro repetido a cada ~70: o scroll engasga. Com a opção ligada o jogo
// roda na taxa do monitor (60, 120 = 2 apresentações por quadro...) com vsync, e o som é reamostrado 1,4% acima.
int ritmo_monitor_ativo;
double ritmo_monitor_hz;
static int ritmo_n = 1, ritmo_falhou;   // falhou = o vsync não segurou a taxa nesta sessão: volta ao ritmo pelo som
static double ritmo_fator = 1.0;
static Uint32 ritmo_aviso_ate;
static void ritmo_calcula(SDL_Window *win, int tem_som)
{
	SDL_DisplayMode m;
	int idx = SDL_GetWindowDisplayIndex(win);
	ritmo_monitor_ativo = 0; ritmo_fator = 1.0; ritmo_n = 1;
	if (cfg.ritmo_monitor && !ritmo_falhou && tem_som && idx >= 0 && SDL_GetCurrentDisplayMode(idx, &m) == 0 && m.refresh_rate > 0) {
		double hz = m.refresh_rate;
		int n = (int)(hz / NEO_HZ + 0.5);
		if (n < 1) n = 1;
		double taxa = hz / n;
		if (fabs(taxa / NEO_HZ - 1.0) < 0.025) { ritmo_monitor_ativo = 1; ritmo_monitor_hz = hz; ritmo_n = n; ritmo_fator = taxa / NEO_HZ; }
	}
	audio_fator_ritmo(ritmo_fator);
}

// ---------------------------------------------------------------- fotos em alta resolução (retratos dos homenageados)
// bin/arte/fotos.bin (tools/retratos.py): u32 n; por foto: u32 tam_png, u16 w, u16 h (tamanho da arte no jogo, px), u8 esp, u8 pad, png.
// A ordem das fotos é a dos alvos de overlay.bin (video_ovl). O renderizador anota onde cada arte foi pintada; aqui a
// foto é desenhada por cima, escalada pro tamanho da janela, então fica nítida mesmo em tela cheia.
typedef struct { SDL_Texture *tex; int w, h, esp, inteira; } Foto;
static Foto fotos[64];
static int nfotos;
static SDL_Renderer *ren;
static void fotos_carrega_bytes(const u8 *d, size_t n)
{
	if (n < 4) return;
	u32 nf; memcpy(&nf, d, 4);
	size_t i = 4;
	for (u32 k = 0; k < nf && k < 64 && i + 10 <= n; k++) {
		u32 tam; u16 w, h; u8 esp;
		memcpy(&tam, d + i, 4); memcpy(&w, d + i + 4, 2); memcpy(&h, d + i + 6, 2); esp = d[i + 8]; i += 10;
		if (i + tam > n) break;
		int iw, ih, ic;
		unsigned char *px = stbi_load_from_memory(d + i, (int)tam, &iw, &ih, &ic, 4);
		i += tam;
		fotos[k] = (Foto){NULL, w, h, esp & 1, (esp >> 1) & 1};
		if (!px) { nfotos = (int)k + 1; continue; }
		SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(px, iw, ih, 32, iw * 4, SDL_PIXELFORMAT_RGBA32);
		if (s) { fotos[k].tex = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
		if (fotos[k].tex) { SDL_SetTextureScaleMode(fotos[k].tex, SDL_ScaleModeLinear); SDL_SetTextureBlendMode(fotos[k].tex, SDL_BLENDMODE_BLEND); }
		stbi_image_free(px);
		nfotos = (int)k + 1;
	}
}
static void fotos_carrega(const char *arq)
{
	FILE *f = fopen(arq, "rb");
	if (!f) return;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *d = malloc((size_t)n);
	size_t lidos = fread(d, 1, (size_t)n, f);
	fclose(f);
	fotos_carrega_bytes(d, lidos);
	free(d);
}
// desenha as fotos cujas artes apareceram neste quadro (ou no quadro congelado da tela de ordem), de cima pra baixo
static void fotos_desenha(const SDL_Rect *r, OvlCaixa (*cx)[2])
{
	typedef struct { int a, l; } It;
	It its[128]; int n = 0;
	for (int a = 0; a < nfotos && a < video_ovl_n; a++)
		for (int l = 0; l < 2; l++)
			if (cx[a][l].n >= 2 && fotos[a].tex && n < 128) its[n++] = (It){a, l};
	for (int i = 1; i < n; i++) {        // ordena por origem y: o de baixo cobre o de cima, como no jogo
		It t = its[i]; int j = i - 1;
		while (j >= 0 && cx[its[j].a][its[j].l].oy > cx[t.a][t.l].oy) { its[j + 1] = its[j]; j--; }
		its[j + 1] = t;
	}
	for (int i = 0; i < n; i++) {
		const OvlCaixa *c = &cx[its[i].a][its[i].l];
		const Foto *f = &fotos[its[i].a];
		int esp = c->esp * 2 > c->n;
		int ox = esp ? c->rx - f->w : c->ox, oy = c->oy;
		if (ox == 9999 || ox == -9999 - f->w || oy == 9999 || c->vx1 < 0) continue;
		// só a parte da arte que ficou visível (o resto o jogo cobriu com outro retrato, caixa de modo...)
		int ix0 = c->vx0 > ox ? c->vx0 : ox, iy0 = c->vy0 > oy ? c->vy0 : oy;
		int ix1 = c->vx1 < ox + f->w - 1 ? c->vx1 : ox + f->w - 1, iy1 = c->vy1 < oy + f->h - 1 ? c->vy1 : oy + f->h - 1;
		if (f->inteira) { ix0 = ox; iy0 = oy; ix1 = ox + f->w - 1; iy1 = oy + f->h - 1; }   // cabeça recortada: a caixa toda
		if (ix1 < ix0 || iy1 < iy0) continue;
		int tw, th; SDL_QueryTexture(f->tex, NULL, NULL, &tw, &th);
		int fl = esp ^ f->esp;
		SDL_Rect src = {(fl ? (ox + f->w - 1 - ix1) : (ix0 - ox)) * tw / f->w, (iy0 - oy) * th / f->h, (ix1 - ix0 + 1) * tw / f->w, (iy1 - iy0 + 1) * th / f->h};
		SDL_Rect dst = {r->x + ix0 * r->w / NEO_W, r->y + iy0 * r->h / NEO_H, (ix1 - ix0 + 1) * r->w / NEO_W, (iy1 - iy0 + 1) * r->h / NEO_H};
		SDL_RenderCopyEx(ren, f->tex, &src, &dst, 0, NULL, fl ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
	}
}
static SDL_Texture *tex_jogo, *tex_fundo, *tex_linhas, *tex_mascara, *tex_ui, *tex_hd;
// menus F1/F6 em 1672x941 (src/hd.c) por cima de tudo, na área 16:9 da janela
static int hd_desenha(int ow, int oh)
{
	if ((!cfgmenu_aberto && !treino_aberto) || !tex_hd) return 0;
	SDL_Rect r = {0, 0, ow, ow * HD_H / HD_W};
	if (r.h > oh) { r.h = oh; r.w = oh * HD_W / HD_H; }
	r.x = (ow - r.w) / 2; r.y = (oh - r.h) / 2;
	SDL_RenderCopy(ren, tex_hd, NULL, &r);
	return 1;
}
static void hd_prepara(void)
{
	if (!cfgmenu_aberto && !treino_aberto) return;
	if (!tex_hd) { tex_hd = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, HD_W, HD_H); SDL_SetTextureBlendMode(tex_hd, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(tex_hd, SDL_ScaleModeLinear); }
	u32 *b = hd_buffer();
	hd_limpa();
	if (treino_aberto && !cfgmenu_aberto) treino_desenha_hd();
	if (cfgmenu_aberto) menu_cfg_desenha_hd();
	SDL_UpdateTexture(tex_hd, NULL, b, HD_W * 4);
}
static int linhas_int = -1, mascara_w, mascara_h, mascara_int = -1;

typedef struct { int linear, linhas, mascara; } Filtro;   // linhas/máscara: 0-255 de escurecimento
static const Filtro FILTROS[NFILTROS] = {
	{0, 0, 0},       // nítido
	{1, 0, 0},       // suave
	{0, 90, 0},      // linhas de TV
	{1, 70, 45},     // tubo CRT
	{0, 120, 60},    // fliperama
	{1, 0, 0},       // xBR 4x na CPU (src/xbr.c), depois suavizado na GPU
};
void xbr_4x(const u32 *src, u32 *dst, int w, int h);
static SDL_Texture *tex_xbr;
static u32 *buf_xbr;

static void prepara_linhas(int intensidade)
{
	if (intensidade == linhas_int) return;
	linhas_int = intensidade;
	if (tex_linhas) SDL_DestroyTexture(tex_linhas);
	tex_linhas = NULL;
	if (!intensidade) return;
	static u32 px[NEO_H * 2];
	for (int y = 0; y < NEO_H * 2; y++) {
		u32 v = (y & 1) ? (u32)(255 - intensidade) : 255;
		px[y] = 0xff000000u | v << 16 | v << 8 | v;
	}
	tex_linhas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, NEO_H * 2);
	SDL_UpdateTexture(tex_linhas, NULL, px, 4);
	SDL_SetTextureBlendMode(tex_linhas, SDL_BLENDMODE_MOD);
	SDL_SetTextureScaleMode(tex_linhas, SDL_ScaleModeNearest);
}

static void prepara_mascara(int w, int h, int intensidade)
{
	if (intensidade == mascara_int && w == mascara_w && h == mascara_h) return;
	mascara_int = intensidade; mascara_w = w; mascara_h = h;
	if (tex_mascara) SDL_DestroyTexture(tex_mascara);
	tex_mascara = NULL;
	if (!intensidade || w <= 0 || h <= 0) return;
	u32 *px = malloc((size_t)w * 4);
	u32 baixo = (u32)(255 - intensidade);
	for (int x = 0; x < w; x++) {            // grade de abertura: listras R, G, B
		u32 r = baixo, g = baixo, b = baixo;
		if (x % 3 == 0) r = 255; else if (x % 3 == 1) g = 255; else b = 255;
		px[x] = 0xff000000u | r << 16 | g << 8 | b;
	}
	tex_mascara = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, 1);
	SDL_UpdateTexture(tex_mascara, NULL, px, w * 4);
	SDL_SetTextureBlendMode(tex_mascara, SDL_BLENDMODE_MOD);
	SDL_SetTextureScaleMode(tex_mascara, SDL_ScaleModeNearest);
	free(px);
}

static SDL_Rect area_jogo(int ow, int oh)
{
	SDL_Rect r = {0, 0, ow, oh};
	if (cfg.proporcao == PROP_4X3) {
		r.h = oh; r.w = oh * 4 / 3;
		if (r.w > ow) { r.w = ow; r.h = ow * 3 / 4; }
	} else if (cfg.proporcao == PROP_INTEIRA) {
		int s = ow / NEO_W < oh / NEO_H ? ow / NEO_W : oh / NEO_H;
		if (s < 1) s = 1;
		r.w = NEO_W * s; r.h = NEO_H * s;
	}
	r.x = (ow - r.w) / 2; r.y = (oh - r.h) / 2;
	return r;
}

static double t_apresenta, t_espera, t_emula, t_neo, t_ra, t_mix;
#define TMS(t0) ((double)(SDL_GetPerformanceCounter() - (t0)) * 1000.0 / (double)SDL_GetPerformanceFrequency())   // diagnóstico KOF_DEBUG_FILA (ms por segundo)
static void apresenta(void)
{
	Uint64 t0 = SDL_GetPerformanceCounter();
	int ow, oh;
	SDL_GetRendererOutputSize(ren, &ow, &oh);
	SDL_Rect r = area_jogo(ow, oh);
	const Filtro *f = &FILTROS[cfg.filtro];
	hd_prepara();
	SDL_Texture *tj = tex_jogo;
	if (cfg.filtro == FILTRO_XBR) {
		if (!tex_xbr) { tex_xbr = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NEO_W * 4, NEO_H * 4); buf_xbr = malloc((size_t)NEO_W * NEO_H * 16 * 4); SDL_SetTextureScaleMode(tex_xbr, SDL_ScaleModeLinear); }
		xbr_4x(&tela[0][0], buf_xbr, NEO_W, NEO_H);
		SDL_UpdateTexture(tex_xbr, NULL, buf_xbr, NEO_W * 4 * 4);
		tj = tex_xbr;
	} else {
		SDL_UpdateTexture(tex_jogo, NULL, tela, NEO_W * 4);
		SDL_SetTextureScaleMode(tex_jogo, f->linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	}
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	if (cfg.fundo_borrado && (r.w < ow || r.h < oh)) {
		SDL_UpdateTexture(tex_fundo, NULL, tela, NEO_W * 4);
		SDL_RenderCopy(ren, tex_fundo, NULL, NULL);
	}
	SDL_RenderCopy(ren, tj, NULL, &r);
	if (cfg.fotos && nfotos && !cfgmenu_aberto) fotos_desenha(&r, ordem_mostra_congelado() ? ordem_ovl : video_ovl);
	if (tex_ui) { SDL_UpdateTexture(tex_ui, NULL, tela_ui, NEO_W * 4); SDL_RenderCopy(ren, tex_ui, NULL, &r); }
	prepara_linhas(f->linhas);
	if (tex_linhas) SDL_RenderCopy(ren, tex_linhas, NULL, &r);
	prepara_mascara(r.w, r.h, f->mascara);
	if (tex_mascara) SDL_RenderCopy(ren, tex_mascara, NULL, &r);
	hd_desenha(ow, oh);
	SDL_RenderPresent(ren);
	for (int k = 1; k < ritmo_n; k++) {      // monitor a 120/240 Hz: o mesmo quadro de novo, no vsync seguinte
		SDL_RenderCopy(ren, tj, NULL, &r);
		if (tex_linhas) SDL_RenderCopy(ren, tex_linhas, NULL, &r);
		if (tex_mascara) SDL_RenderCopy(ren, tex_mascara, NULL, &r);
		hd_desenha(ow, oh);
		SDL_RenderPresent(ren);
	}
	t_apresenta += (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

// ---------------------------------------------------------------- principal
int main(int argc, char **argv)
{
	const char *zips[8];
	int nz = 0;
	u32 quadros_max = 0;
	const char *foto = NULL, *wav = NULL, *est_ini = NULL, *est_fim = NULL;
	int sem_janela = 0, modo_cli = -1;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--quadros") && i + 1 < argc) quadros_max = (u32)atoi(argv[++i]);
		else if (!strcmp(argv[i], "--foto") && i + 1 < argc) foto = argv[++i];
		else if (!strcmp(argv[i], "--sem-janela")) sem_janela = 1;
		else if (!strcmp(argv[i], "--seg-entradas") && i + 1 < argc) le_roteiro(argv[++i]);
		else if (!strcmp(argv[i], "--carregar-estado") && i + 1 < argc) est_ini = argv[++i];
		else if (!strcmp(argv[i], "--salvar-estado") && i + 1 < argc) est_fim = argv[++i];
		else if (!strcmp(argv[i], "--arcade")) modo_cli = 0;
		else if (!strcmp(argv[i], "--console")) modo_cli = 1;
		else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
		else if (nz < 8) zips[nz++] = argv[i];
	}
	if (modo_cli >= 0) neo_aes = modo_cli;

	char *basep = SDL_GetBasePath();
	musica_init(basep);
#ifdef KOF_SOLO
	video_troca_embutida();
	{ extern const unsigned char embutido_overlay[]; extern const size_t embutido_overlay_tam; video_ovl_carrega_bytes(embutido_overlay, embutido_overlay_tam); }
#else
	{
		char t[700];
		snprintf(t, sizeof t, "%sarte/troca_tiles.bin", basep ? basep : "");
		video_troca_carrega(t);
		snprintf(t, sizeof t, "%sarte/overlay.bin", basep ? basep : "");
		video_ovl_carrega(t);
	}
#endif
	efeitos_init(basep);
	if (basep) { snprintf(cfg_arquivo, sizeof cfg_arquivo, "%skof98.ini", basep); SDL_free(basep); }
	if (sem_janela) config_padrao(); else config_carregar(cfg_arquivo);
	if (getenv("KOF_SO_CHIP")) cfg.musica_arquivos = cfg.efeitos_arquivos = 0;   // som 100% do chip (importar_um.py)
	musica_ligada = cfg.musica_arquivos;
	efeitos_ligados = cfg.efeitos_arquivos;
	if (sem_janela && getenv("KOF_CENARIO")) cfg.cenario = atoi(getenv("KOF_CENARIO"));
#ifdef KOF_TESTE
	if (getenv("KOF_LIMITE")) video_limite_linha = atoi(getenv("KOF_LIMITE"));
	if (getenv("KOF_CPU")) cfg.cpu = atoi(getenv("KOF_CPU"));   // só no build de teste (comparar com o MAME)
#endif
	neo_cpu_pct = cfg.cpu;

	static char achados[8][600];
	if (!nz) {                       // todo .zip ao lado do EXE e em roms\ (kof98ae2016.zip + kof98.zip + neogeo.zip)
		char *bp = SDL_GetBasePath();
		const char *pastas[2] = {"roms/", ""};
		for (int pi = 0; pi < 2 && nz < 8; pi++) {
			char busca[700];
			snprintf(busca, sizeof busca, "%s%s*.zip", bp ? bp : "", pastas[pi]);
			wchar_t wb[700];
			MultiByteToWideChar(CP_UTF8, 0, busca, -1, wb, 700);
			WIN32_FIND_DATAW fd;
			HANDLE h = FindFirstFileW(wb, &fd);
			if (h == INVALID_HANDLE_VALUE) continue;
			do {
				char nome[400];
				WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nome, sizeof nome, NULL, NULL);
				if (nz < 8) { snprintf(achados[nz], 600, "%s%s%s", bp ? bp : "", pastas[pi], nome); zips[nz] = achados[nz]; nz++; }
			} while (FindNextFileW(h, &fd));
			FindClose(h);
		}
		SDL_free(bp);
	}
#ifndef KOF_SOLO
	if (!nz) {                       // caminhos desta máquina (desenvolvimento)
		static const char *padrao[] = {
			"roms/kof98aemod.zip", "roms/kof98ae2016.zip", "roms/kof98.zip", "roms/neogeo.zip",
			"D:/Tools/FBNeo/roms/kof98aemod.zip", "D:/Tools/FBNeo/roms/neogeo.zip", "D:/Tools/MAME/roms/neogeo.zip" };
		for (size_t i = 0; i < sizeof padrao / sizeof padrao[0] && nz < 8; i++)
			if (existe(padrao[i])) zips[nz++] = padrao[i];
	}
#endif

	char erro[256];
	if (roms_load(zips, nz, neo_aes, erro, sizeof erro)) {
		fprintf(stderr, "erro: %s\n", erro);
		char msg[900];
		snprintf(msg, sizeof msg, "%s\n\nColoque na mesma pasta do jogo (ou numa pasta roms\\ ao lado dele):\n"
			"  - kof98ae2016.zip  (The King of Fighters '98 Anniversary Edition 2016; o FBNeo chama de kof98ae)\n"
			"  - kof98.zip  (The King of Fighters '98 original: o Anniversary usa os sprites c3/c4 e o som v1-v4 dele)\n"
			"  - neogeo.zip  (BIOS do Neo Geo, com neo-epo.bin, sfix, sm1 e 000-lo)\n\n"
			"Os dois vêm da sua cópia do jogo: nenhum deles acompanha o programa.", erro);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KOF '98 Comunidade Edition", msg, NULL);
		return 1;
	}
	neo_init();
	if (sem_janela && getenv("KOF_RESET")) neo_reset();   // teste: mesmo caminho do menu inicial (reset em vez de ligar)
	if (est_ini && estado_carregar(est_ini)) fprintf(stderr, "erro ao carregar estado %s\n", est_ini);

	// ------------------------------------------------------------ modo de teste sem janela
	if (sem_janela) {
		const char *tr = getenv("KOF_TREINO");
		while (tr && *tr) {
			char nome[16] = {0}; int v = 0;
			if (sscanf(tr, "%15[a-z]=%d", nome, &v) == 2) treino_opcao(nome, v);
			tr = strchr(tr, ','); if (tr) tr++;
		}
		if (getenv("KOF_CFG")) menu_cfg_abre(atoi(getenv("KOF_CFG")) - 1);
#ifdef KOF_TESTE
		if (getenv("KOF_MENU_MUSICA")) musica_modo_menu = 1;
		if (getenv("KOF_ORDEM")) ordem_ligada = atoi(getenv("KOF_ORDEM"));
#endif
#ifdef KOF_TESTE
		{ void frontend_teste_arte(void); if (getenv("KOF_VIGIA_TESTE")) frontend_teste_arte(); }
#endif
		FILE *fw = wav ? fopen(wav, "wb") : NULL;
		u32 total = 0;
		static s16 buf[8192 * 2];
		if (fw) fseek(fw, 44, SEEK_SET);
		const char *so_som = getenv("KOF_SO_SOM");   // "quadro:cmd,..." (hexa): só o Z80+YM, sem o 68k
		if (so_som) { audio_reset(); audio_bios_ou_cart(1); }
		for (u32 q = 0; so_som && q < quadros_max; q++) {
			for (const char *s = so_som; s && *s; s = strchr(s, ','), s = s ? s + 1 : NULL) {
				unsigned sq, sc;
				if (sscanf(s, "%u:%x", &sq, &sc) == 2 && sq == q) audio_comando((u8)sc);
			}
			for (int l = 0; l < NEO_LINHAS; l++) audio_roda_linha();
			int n = audio_pega_amostras(buf, 8192);
			if (fw) { fwrite(buf, 4, (size_t)n, fw); total += (u32)n; }
		}
		for (u32 q = 0; !so_som && q < quadros_max; q++) {
			neo_in_p1 = neo_in_p2 = 0xff; neo_in_start = 0x0f; neo_in_coin = 0x07; neo_in_test = 1;
			aplica_roteiro();
#ifdef KOF_TESTE
			if (getenv("KOF_MENU_MUSICA") && neo_quadros == (u32)atoi(getenv("KOF_MENU_MUSICA"))) musica_menu_fim(1);
#endif
			treino_entradas();
			ordem_entradas();
			config_aplica_cenario();
			int ja_rodou = 0;
#ifdef KOF_TESTE
			if (getenv("KOF_TESTE_ROLLBACK")) {   // determinismo do savestate: salva, roda, volta 8 quadros, re-simula e compara o hash
				extern int estado_ring_carregar(u32); extern void estado_ring_salvar(u32);
				static u8 rp1[64], rp2[64], rs[64]; static u32 rh[64]; static int erros;
				u32 qa = neo_quadros;
				estado_leve = 1; estado_ring_salvar(qa); estado_leve = 0;
				rp1[qa % 64] = neo_in_p1; rp2[qa % 64] = neo_in_p2; rs[qa % 64] = neo_in_start;
				neo_frame();
				{ u32 h = 2166136261u; for (u32 a = 0x100000; a < 0x110000; a += 2) { h ^= (u32)m68k_read_memory_16(a); h *= 16777619u; } rh[(qa + 1) % 64] = h; }
				if (qa > 200 && qa % 30 == 0) {
					u32 q0 = qa - 8;
					if (estado_ring_carregar(q0)) fprintf(stderr, "rollback-teste: sem estado %u\n", q0);
					else {
						musica_silenciosa = 1; estado_leve = 1;
						for (u32 q = q0; q <= qa; q++) {
							neo_in_p1 = rp1[q % 64]; neo_in_p2 = rp2[q % 64]; neo_in_start = rs[q % 64]; neo_in_coin = 0x07; neo_in_test = 1;
							ordem_entradas();
							neo_frame();
							u32 h = 2166136261u; for (u32 a = 0x100000; a < 0x110000; a += 2) { h ^= (u32)m68k_read_memory_16(a); h *= 16777619u; }
							if (h != rh[(q + 1) % 64]) { if (erros++ < 20) fprintf(stderr, "rollback-teste: quadro %u diverge (%08x x %08x) apos voltar pra %u\n", q + 1, h, rh[(q + 1) % 64], q0); break; }
						}
						audio_descarta(); musica_silenciosa = 0; estado_leve = 0;
						if (neo_quadros != qa + 1) fprintf(stderr, "rollback-teste: neo_quadros %u != %u\n", neo_quadros, qa + 1);
					}
				}
				if (qa + 1 >= quadros_max) fprintf(stderr, "rollback-teste: %d divergencias\n", erros);
				ja_rodou = 1;
			}
#endif
#ifdef KOF_TESTE
			{   // KOF_POKE="endereco:valor:ini:fim" (hexa:hexa:dec:dec) — escreve um byte a cada quadro no intervalo
				const char *pk = getenv("KOF_POKE");
				unsigned pa, pv, pi, pf;
				if (pk && sscanf(pk, "%x:%x:%u:%u", &pa, &pv, &pi, &pf) == 4 && neo_quadros >= pi && neo_quadros <= pf)
					m68k_write_memory_8(pa, pv);
			}
#endif
			video_limite_linha = cfg.sprites_sem_limite ? MAX_SPRITES_TELA : 96;
			if (!ja_rodou) neo_frame();
			treino_depois_quadro();
#ifdef KOF_TESTE
			if (getenv("KOF_RUNAHEAD")) {   // teste: o estado final tem que ser idêntico ao sem run-ahead
				static s16 lx[8192 * 2];
				int na = audio_pega_amostras(lx, 8192);
				if (fw) { fwrite(lx, 4, (size_t)na, fw); total += (u32)na; }
				run_ahead(atoi(getenv("KOF_RUNAHEAD")));
			}
			if (getenv("KOF_VIGIA_TESTE")) {   // quando o menu abriria sozinho por cima do MODE SELECT original
				void frontend_teste_zera(void);
				frontend_vigia();
				if (frontend_ativo) { fprintf(stderr, "vigia abriu no quadro %u\n", neo_quadros); frontend_teste_zera(); }
			}
			if (getenv("KOF_HASH_RAM") && neo_quadros % 60 == 0) {   // hash da RAM do 68k a cada 60 quadros (determinismo entre configs)
				u32 h = 2166136261u;
				for (u32 a = 0x100000; a < 0x110000; a += 2) { h ^= (u32)m68k_read_memory_16(a); h *= 16777619u; }
				estado_mem_salvar(); extern size_t estado_mem_tam(void);
				fprintf(stdout, "hash %u %08x estado %u\n", neo_quadros, h, (unsigned)estado_mem_tam());
			}
			if (getenv("KOF_LOG_PRETO")) {   // quadros com a tela quase toda preta (bug do sprite 0 na rua com carros)
				int pretos = 0;
				for (int y = 60; y < 200; y++) for (int x = 40; x < 300; x++) pretos += (neo_fb[y][x] & 0xffffff) < 0x080808;
				if (pretos > 140 * 260 * 6 / 10) fprintf(stderr, "preto %u %d%%\n", neo_quadros, pretos * 100 / (140 * 260));
			}
#endif
			const char *dump = getenv("KOF_DUMP");      // "endereco,tamanho" em hexa: despeja a cada quadro
			if (dump) {
				unsigned da = 0, dn = 0;
				sscanf(dump, "%x,%x", &da, &dn);
				fprintf(stdout, "%u", neo_quadros);
				for (unsigned k = 0; k < dn; k++) fprintf(stdout, " %02x", m68k_read_memory_8(da + k));
				fputc('\n', stdout);
			}
			int n = audio_pega_amostras(buf, 8192);
			if (fw) { fwrite(buf, 4, (size_t)n, fw); total += (u32)n; }
		}
		if (fw) {   // cabeçalho WAV PCM 16 bits estéreo AUDIO_TAXA
			u32 taxa = AUDIO_TAXA, bytes = total * 4, v32;
			u16 v16;
			fseek(fw, 0, SEEK_SET);
			fwrite("RIFF", 1, 4, fw); v32 = 36 + bytes; fwrite(&v32, 4, 1, fw); fwrite("WAVEfmt ", 1, 8, fw);
			v32 = 16; fwrite(&v32, 4, 1, fw); v16 = 1; fwrite(&v16, 2, 1, fw); v16 = 2; fwrite(&v16, 2, 1, fw);
			fwrite(&taxa, 4, 1, fw); v32 = taxa * 4; fwrite(&v32, 4, 1, fw); v16 = 4; fwrite(&v16, 2, 1, fw);
			v16 = 16; fwrite(&v16, 2, 1, fw); fwrite("data", 1, 4, fw); fwrite(&bytes, 4, 1, fw);
			fclose(fw);
		}
		memcpy(tela, neo_fb, sizeof tela);
		ordem_desenha(tela, tela);
		treino_desenha(tela);
#ifdef KOF_TESTE
		{ void video_despeja_fix(const char *); if (getenv("KOF_FIX")) video_despeja_fix(getenv("KOF_FIX")); }
#endif
		if (foto) salva_bmp(foto);
		if (est_fim) estado_salvar(est_fim);
		{ extern u32 audio_cmd_total, audio_cmd_perdidos; fprintf(stderr, "som: %u comandos, %u perdidos\n", audio_cmd_total, audio_cmd_perdidos); }
		fprintf(stderr, "quadros=%u pc=%06x\n", neo_quadros, m68k_get_reg(NULL, M68K_REG_PC));
		return 0;
	}

	// ------------------------------------------------------------ janela
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");                 // sticks DirectInput (arcade por USB) lidos numa thread própria
	SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");               // rawinput some com controle em alguns PCs (SDL 2.30): fica DirectInput/XInput
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	rede_init();
	SDL_Rect util = {0, 0, 1280, 720};
	SDL_GetDisplayUsableBounds(0, &util);
	int escala = (util.h * 85 / 100) / NEO_H;
	if ((util.w * 85 / 100) / NEO_W < escala) escala = (util.w * 85 / 100) / NEO_W;
	if (escala < 1) escala = 1;
	const char *autofoto = getenv("KOF_AUTOFOTO");      // "quadros,arquivo.bmp": captura a saída final e sai
	if (autofoto && !getenv("KOF_SALVA_CFG")) config_somente_leitura = 1;           // o menu salva a cada mudança: teste não pode gravar no kof98.ini do usuário
	if (getenv("KOF_MUDO")) cfg.mudo = 1;
	if (getenv("KOF_VSYNC")) cfg.vsync = atoi(getenv("KOF_VSYNC"));
	if (getenv("KOF_FILTRO")) cfg.filtro = atoi(getenv("KOF_FILTRO")) % NFILTROS;
#ifdef KOF_TESTE
	if (getenv("KOF_CFG")) menu_cfg_abre(atoi(getenv("KOF_CFG")) - 1);
	{   // KOF_TREINO="menu=1,hitbox=1" também com janela (foto dos menus HD)
		const char *tr = getenv("KOF_TREINO");
		while (tr && *tr) {
			char nome[16] = {0}; int v = 0;
			if (sscanf(tr, "%15[a-z]=%d", nome, &v) == 2) treino_opcao(nome, v);
			tr = strchr(tr, ','); if (tr) tr++;
		}
	}
	if (getenv("KOF_RUNAHEAD")) cfg.run_ahead = atoi(getenv("KOF_RUNAHEAD"));
	if (getenv("KOF_RITMO")) cfg.ritmo_monitor = atoi(getenv("KOF_RITMO"));
#endif
	SDL_Window *win = SDL_CreateWindow("KOF '98 UM Comunidade Edition v" KOF_VERSAO, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, NEO_H * escala * 4 / 3, NEO_H * escala, SDL_WINDOW_RESIZABLE | (autofoto && !getenv("KOF_JANELA") ? SDL_WINDOW_HIDDEN : 0));
	SDL_SetWindowSize(win, NEO_H * escala * 4 / 3, NEO_H * escala);
#ifdef KOF_TESTE
	if (getenv("KOF_TAM")) { int tw = 1600, th = 900; sscanf(getenv("KOF_TAM"), "%d,%d", &tw, &th); SDL_SetWindowSize(win, tw, th); }   // janela 16:9 pra foto dos menus
#endif
#ifdef KOF_SOLO
	{   // ícone da janela (arte/icone_64.png embutido)
		extern const unsigned char embutido_icone_png[]; extern const size_t embutido_icone_png_tam;
		int iw, ih, in;
		unsigned char *ip = stbi_load_from_memory(embutido_icone_png, (int)embutido_icone_png_tam, &iw, &ih, &in, 4);
		if (ip) {
			SDL_Surface *is = SDL_CreateRGBSurfaceWithFormatFrom(ip, iw, ih, 32, iw * 4, SDL_PIXELFORMAT_RGBA32);
			if (is) { SDL_SetWindowIcon(win, is); SDL_FreeSurface(is); }
			stbi_image_free(ip);
		}
	}
#endif
	SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
	tex_jogo = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NEO_W, NEO_H);
#ifdef KOF_SOLO
	{ extern const unsigned char embutido_fotos[]; extern const size_t embutido_fotos_tam; fotos_carrega_bytes(embutido_fotos, embutido_fotos_tam); }
#else
	{ char *bp = SDL_GetBasePath(); char t[700]; snprintf(t, sizeof t, "%sarte/fotos.bin", bp ? bp : ""); fotos_carrega(t); SDL_free(bp); }
#endif
	tex_fundo = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NEO_W, NEO_H);
	SDL_SetTextureScaleMode(tex_fundo, SDL_ScaleModeLinear);
	tex_ui = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NEO_W, NEO_H);
	SDL_SetTextureBlendMode(tex_ui, SDL_BLENDMODE_BLEND);
	SDL_SetTextureColorMod(tex_fundo, 60, 60, 70);
	int vsync_atual = -1, cheia_atual = 0, ritmo_cfg_atual = -1;
	if (cfg.tela_cheia) { SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP); cheia_atual = 1; }

	SDL_AudioSpec quero = {0}, tenho;
	quero.freq = AUDIO_TAXA; quero.format = AUDIO_S16SYS; quero.channels = 2; quero.samples = 1024;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &quero, &tenho, 0);
	if (dev) SDL_PauseAudioDevice(dev, 0);
	ritmo_calcula(win, dev != 0);
	entrada_init();

	// menu inicial (Comunidade Edition): o jogo roda por trás dele desde o reset
	{
		char *bp = SDL_GetBasePath();
		hd_init(bp);
		if (frontend_carrega(bp) && !est_ini) frontend_abre();
#ifdef KOF_SOLO
		if (!autofoto || getenv("KOF_ATUALIZA")) atualiza_checa();   // versão nova no GitHub? (só no EXE distribuído)
#else
		if (getenv("KOF_ATUALIZA")) atualiza_checa();
#endif
		SDL_free(bp);
	}
#ifdef KOF_TESTE
	{ void frontend_escolhe_teste(int); if (getenv("KOF_MENU_ESCOLHA")) frontend_escolhe_teste(atoi(getenv("KOF_MENU_ESCOLHA"))); }
	{ void frontend_rede_teste(const char *); if (getenv("KOF_REDE")) frontend_rede_teste(getenv("KOF_REDE")); }
#endif

	// início rápido: roda o boot da BIOS (logo) sem imagem nem som
	if (cfg.inicio_rapido && !est_ini && !frontend_ativo) {
		static s16 lixo[8192 * 2];
		for (int q = 0; q < 330; q++) { config_aplica_entradas(); neo_frame(); audio_pega_amostras(lixo, 8192); }
	}
	if (!cfg.dica_treino) treino_aviso_ate = 0;
	else treino_aviso_ate = neo_quadros + 60 * 8;

	static s16 amostras[8192 * 2];
	Uint64 freq = SDL_GetPerformanceFrequency();
	double passo = (double)freq / NEO_HZ;
	double proximo = (double)SDL_GetPerformanceCounter();
	int rodando = 1, sem_foco = 0;
	Uint32 fps_t = SDL_GetTicks(), fps_n = 0, fps = 0;
	while (rodando) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) rodando = 0;
			entrada_evento(&e);
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) sem_foco = 1;
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) sem_foco = 0;
#ifdef KOF_TESTE
			if (getenv("KOF_DEBUG_ENTRADA") && e.type == SDL_KEYDOWN) fprintf(stderr, "tecla %d quadro %u\n", e.key.keysym.scancode, neo_quadros);
#endif
			if (frontend_ativo) {          // no menu inicial vale F1 (configurações por cima), Esc (sai), F11/Alt+Enter e F12
				if (frontend_evento(&e)) continue;
				if (menu_cfg_evento(&e)) continue;
				if (e.type == SDL_KEYDOWN && !e.key.repeat) {
					SDL_Scancode s = e.key.keysym.scancode;
					if (s == SDL_SCANCODE_ESCAPE && !frontend_voltar()) rodando = 0;
					if (s == SDL_SCANCODE_F11 || (s == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))) { cfg.tela_cheia = !cfg.tela_cheia; config_salvar(cfg_arquivo); }
				}
				continue;
			}
			if (rede_ativa && chat_digitando) {        // digitando no chat: Enter manda, Esc cancela, Backspace apaga
				if (e.type == SDL_TEXTINPUT) { if (strlen(chat_texto) + strlen(e.text.text) < sizeof chat_texto - 1) strcat(chat_texto, e.text.text); continue; }
				if (e.type == SDL_KEYDOWN) {
					SDL_Scancode s = e.key.keysym.scancode;
					if (s == SDL_SCANCODE_RETURN || s == SDL_SCANCODE_KP_ENTER) { rede_chat_envia(chat_texto); chat_digitando = 0; SDL_StopTextInput(); entrada_teclado_bloqueado = 0; }
					else if (s == SDL_SCANCODE_ESCAPE) { chat_digitando = 0; SDL_StopTextInput(); entrada_teclado_bloqueado = 0; }
					else if (s == SDL_SCANCODE_BACKSPACE) { size_t n = strlen(chat_texto); while (n && (chat_texto[n - 1] & 0xc0) == 0x80) n--; if (n) n--; chat_texto[n] = 0; }
				}
				continue;
			}
			if (rede_ativa && !frontend_ativo && e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_T && !cfgmenu_aberto) {
				chat_digitando = 1; chat_texto[0] = 0; entrada_teclado_bloqueado = 1; SDL_StartTextInput(); continue;
			}
			if (!treino_aberto && menu_cfg_evento(&e)) continue;
			if (treino_aberto && !rede_ativa) {      // menu F6: setas, teclas/botões configurados e botões fixos do controle
				static const int T[] = {0, TECLA_CIMA, TECLA_BAIXO, TECLA_ESQ, TECLA_DIR, TECLA_OK, TECLA_ESC, 0, 0};
				int m = entrada_evento_menu(&e);
				if (m && T[m] && treino_tecla(T[m])) continue;
			}
			if (e.type != SDL_KEYDOWN || e.key.repeat) continue;
			SDL_Scancode sc = e.key.keysym.scancode;
			int t = 0;
			switch (sc) {
			case SDL_SCANCODE_F5: t = TECLA_F5; break; case SDL_SCANCODE_F6: t = TECLA_F6; break;
			case SDL_SCANCODE_F7: t = TECLA_F7; break; case SDL_SCANCODE_F8: t = TECLA_F8; break;
			case SDL_SCANCODE_F9: t = TECLA_F9; break; case SDL_SCANCODE_UP: t = TECLA_CIMA; break;
			case SDL_SCANCODE_DOWN: t = TECLA_BAIXO; break; case SDL_SCANCODE_LEFT: t = TECLA_ESQ; break;
			case SDL_SCANCODE_RIGHT: t = TECLA_DIR; break; case SDL_SCANCODE_Z: case SDL_SCANCODE_RETURN: t = TECLA_OK; break;
			case SDL_SCANCODE_ESCAPE: t = TECLA_ESC; break;
			default: break;
			}
			if (t && !rede_ativa && treino_tecla(t)) continue;
			if (sc == SDL_SCANCODE_ESCAPE) { if (rede_ativa) rede_desligar(); if (frontend_tem_arte()) frontend_abre(); else rodando = 0; }   // Esc volta pro menu (e sai da sala)
			if (sc == SDL_SCANCODE_F11 || (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))) { cfg.tela_cheia = !cfg.tela_cheia; config_salvar(cfg_arquivo); }
			if (sc == SDL_SCANCODE_F4) { cfg.filtro = (cfg.filtro + 1) % NFILTROS; config_salvar(cfg_arquivo); }
			if (sc == SDL_SCANCODE_F3 && !rede_ativa) neo_reset();
			if (sc == SDL_SCANCODE_F12) salva_bmp("kof98_foto.bmp");
		}
		if (cfg.tela_cheia != cheia_atual) { cheia_atual = cfg.tela_cheia; SDL_SetWindowFullscreen(win, cheia_atual ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); ritmo_calcula(win, dev != 0); }
		if (cfg.ritmo_monitor != ritmo_cfg_atual || fps_n == 30) { ritmo_cfg_atual = cfg.ritmo_monitor; ritmo_calcula(win, dev != 0); }
		{ int v = cfg.vsync || ritmo_monitor_ativo; if (v != vsync_atual) { vsync_atual = v; SDL_RenderSetVSync(ren, v); } }
		video_limite_linha = cfg.sprites_sem_limite ? MAX_SPRITES_TELA : 96;
		neo_cpu_pct = cfg.cpu;
		musica_ligada = cfg.musica_arquivos;
		efeitos_ligados = cfg.efeitos_arquivos;
		rede_poll();
		rede_atraso_cfg = cfg.atraso_online;
		if (!rede_ativa || rede_anfitriao) rede_ordem = cfg.ordem_botao;
		ordem_ligada = rede_ativa ? rede_ordem : cfg.ordem_botao;   // na partida online vale a config do anfitrião nos dois lados

		entrada_le();
		if (rede_ativa) {                              // rollback: a entrada real do outro lado chegou diferente da prevista
			int k = rede_rollback();
			if (k >= 0) resimula_rede((u32)k);
			rede_checa_crc();
			rede_previsao = !frontend_ativo && !frontend_piloto() && !ordem_ativa() && !ordem_na_tela() && !cfgmenu_aberto;   // previsão só na luta
		} else { rede_previsao = 0; if (chat_digitando) { chat_digitando = 0; SDL_StopTextInput(); entrada_teclado_bloqueado = 0; } }
		int pausado = !frontend_ativo && !rede_ativa && (cfgmenu_aberto || treino_aberto || (sem_foco && cfg.pausa_sem_foco));   // online nunca pausa (o outro lado esperaria)
		if (frontend_ativo && !cfgmenu_aberto) frontend_controles(&rodando);
		int passos = pausado ? 0 : frontend_passos();   // >1 = avanço rápido escondido atrás do menu
		int rede_parado = 0;
		if (rede_ativa) { int d = rede_pode(); if (passos > d) passos = d; rede_parado = d == 0; rede_conta_espera(rede_parado); }   // lockstep/rollback: quantos quadros dá pra andar
#ifdef KOF_TESTE
		{ static u32 ult; if (getenv("KOF_DEBUG_REDE") && SDL_GetTicks() - ult >= 1000) { ult = SDL_GetTicks(); fprintf(stderr, "rede: estado %d ativa %d quadro %u pode %d ping %d atraso %d espera %d dessinc %d neo %u menu %d | %s\n", rede_estado, rede_ativa, rede_quadro(), rede_pode(), rede_ping, rede_atraso, rede_esperando(), rede_dessincronizou, neo_quadros, frontend_ativo, rede_status()); } }
#endif
		int rapido = passos > 1;
		if (!rapido && !pausado && ritmo_monitor_ativo && dev) {   // vsync dita o ritmo; a fila de som só corrige a deriva
			double fila_q = SDL_GetQueuedAudioSize(dev) / 4.0 / ((double)AUDIO_TAXA / NEO_HZ);
			passos = fila_q < 1.5 ? 2 : fila_q > 6.0 ? 0 : 1;
			double aj = (fila_q - 3.0) * 0.004;
			if (aj > 0.015) aj = 0.015; if (aj < -0.015) aj = -0.015;
			audio_fator_ritmo(ritmo_fator * (1.0 + aj));
			if (rede_ativa) { int d = rede_pode(); if (passos > d) passos = d; }   // (bug até 19/10: isto passava por cima do lockstep e rodava um quadro sem a entrada do outro = dessincronia)
		}
		Uint64 t_em0 = SDL_GetPerformanceCounter();
		for (int passo_i = 0; passo_i < passos; passo_i++) {
			int piloto = frontend_piloto();
			if (rede_ativa) {                     // online: minha entrada vai pro quadro atual+atraso; as dos dois lados vêm do lockstep
				u8 pp1 = neo_in_p1, pp2 = neo_in_p2, ps = neo_in_start;
				config_aplica_entradas();
				u8 mp = (u8)(neo_in_p1 & neo_in_p2), mx = (neo_in_start & 5) != 5 ? REDE_X_START : 0;
				if (cfgmenu_aberto || frontend_ativo) { mp = 0xff; mx = 0; }
#ifdef KOF_TESTE
				if (getenv("KOF_REDE_RANDOM") && rede_previsao) mp = (u8)(rand() & 0xff);   // lab: entrada aleatória força rollbacks
#endif
				u8 p1, x1, p2, x2;
				{ extern void estado_ring_salvar(u32); estado_leve = 1; estado_ring_salvar(rede_quadro()); estado_leve = 0; }   // estado do início deste quadro (rollback volta aqui)
				rede_passo(mp, mx, &p1, &x1, &p2, &x2);
				if (x1 & REDE_X_MENU) frontend_escolhe_item(x1 >> 4);
				if (piloto) { neo_in_p1 = pp1; neo_in_p2 = pp2; neo_in_start = ps; }
				else { neo_in_p1 = p1; neo_in_p2 = p2; neo_in_start = (u8)(0x0f & ~((x1 & REDE_X_START) ? 1 : 0) & ~((x2 & REDE_X_START) ? 4 : 0)); }
				neo_in_coin = 0x07; neo_in_test = 1;
			} else if (!piloto) config_aplica_entradas();
			if (!piloto && nroteiro) aplica_roteiro();
#ifdef KOF_TESTE
			if (getenv("KOF_DEBUG_ENTRADA") && neo_in_start != 0x0f) fprintf(stderr, "start %x quadro %u\n", neo_in_start, neo_quadros);
#endif
			if (!piloto && SDL_GetKeyboardState(NULL)[SDL_SCANCODE_F2]) neo_in_test = 0;
			if (!piloto && !rede_ativa) treino_entradas();
			if (!piloto && rede_quadro_real(rede_quadro() - 1)) ordem_entradas();   // ORDER SELECT só em quadro confirmado (rollback não desfaz os estados do módulo)
			if (!rede_ativa) config_aplica_cenario();
			Uint64 tq = SDL_GetPerformanceCounter();
			neo_frame();
			t_neo += TMS(tq); tq = SDL_GetPerformanceCounter();
			if (!rede_ativa) treino_depois_quadro();
			rede_depois_quadro();
			frontend_vigia();
			int n = audio_pega_amostras(amostras, 8192);
			t_mix += TMS(tq); tq = SDL_GetPerformanceCounter();
			if (rapido) continue;                  // avanço rápido: som descartado
			if (!piloto && cfg.run_ahead > 0 && !frontend_ativo && !rede_ativa) run_ahead(cfg.run_ahead);
			t_ra += TMS(tq);
			int vol = cfg.mudo ? 0 : cfg.volume;
			if (vol != 100) for (int i = 0; i < n * 2; i++) amostras[i] = (s16)(amostras[i] * vol / 100);
			if (dev) {
				Uint32 fila = SDL_GetQueuedAudioSize(dev);
				t_emula += (double)(SDL_GetPerformanceCounter() - t_em0) * 1000.0 / (double)SDL_GetPerformanceFrequency(); t_em0 = SDL_GetPerformanceCounter();
				if (getenv("KOF_DEBUG_FILA")) {      // diagnóstico: fila de áudio antes de enfileirar
					static Uint32 t0, mn = ~0u, mx, vazias, quadros;
					if (!t0) t0 = SDL_GetTicks();
					if (fila < mn) mn = fila;
					if (fila > mx) mx = fila;
					if (fila == 0) vazias++;
					quadros++;
					if (SDL_GetTicks() - t0 >= 1000) {
						fprintf(stderr, "fila: min %u max %u vazias %u quadros %u | ms/s: emula %.0f (neo %.0f mix %.0f ra %.0f) apresenta %.0f espera %.0f | ritmo %d (%.0f Hz x%d) vsync %d\n", mn / 4, mx / 4, vazias, quadros, t_emula, t_neo, t_mix, t_ra, t_apresenta, t_espera, ritmo_monitor_ativo, ritmo_monitor_hz, ritmo_n, vsync_atual);
						t_emula = t_apresenta = t_espera = t_neo = t_mix = t_ra = 0;
						t0 = SDL_GetTicks(); mn = ~0u; mx = 0; vazias = 0; quadros = 0;
					}
				}
				SDL_QueueAudio(dev, amostras, (Uint32)n * 4);
			}
		}
		if (frontend_ativo && cfgmenu_aberto) {
			for (int y = 0; y < NEO_H; y++) for (int x = 0; x < NEO_W; x++) tela[y][x] = 0xff05070cu;
			apresenta();
		} else if (frontend_ativo) frontend_desenha(ren);
		else {
		memcpy(tela, neo_fb, sizeof tela);
		memset(tela_ui, 0, sizeof tela_ui);
		ordem_desenha(tela, tela_ui);
		treino_desenha(tela_ui);
		if (rede_ativa) {
			char b[48];
			ui_alvo(tela_ui);
			snprintf(b, sizeof b, "online %dP  ping %d ms  atraso %d  rollback %d", rede_anfitriao ? 1 : 2, rede_ping, rede_atraso, rede_rollbacks);
			ui_texto(4, NEO_H - 12, b, 0xff60ff90u);
			if (rede_esperando() > 15) ui_texto_centro(NEO_H / 2 - 4, "Aguardando o adversário...", 0xffffe060u);
			if (rede_dessincronizou) ui_texto_centro(NEO_H / 2 + 10, "DESSINCRONIZOU: ESC e crie a sala de novo", 0xffff6060u);
			{   // chat: mensagens dos últimos 10 s no canto inferior esquerdo + campo de digitação (T)
				int y = NEO_H - 24;
				if (chat_digitando) {
					char l[96]; snprintf(l, sizeof l, "> %s_", chat_texto);
					ui_caixa(2, y - 2, 2 + ui_largura(l) + 4, y + 9, 0xff101420u, 230); ui_texto(4, y, l, 0xffffffffu); y -= 11;
				} else if (!rede_chat_n || SDL_GetTicks() - rede_chat[rede_chat_n - 1].t > 10000) { ui_texto(4, y, "T: chat", 0xff8890a0u); y -= 11; }
				for (int i = rede_chat_n - 1; i >= 0 && y > 0; i--) {
					if (SDL_GetTicks() - rede_chat[i].t > 10000) break;
					char l[96]; snprintf(l, sizeof l, "%s: %s", rede_chat[i].de_mim == (rede_anfitriao ? 1 : 0) ? "1P" : "2P", rede_chat[i].texto);
					if ((int)strlen(l) > 38) l[38] = 0;
					ui_caixa(2, y - 2, 2 + ui_largura(l) + 4, y + 9, 0xff101420u, 200); ui_texto(4, y, l, rede_chat[i].de_mim ? 0xffffe060u : 0xff80e0ffu); y -= 11;
				}
			}
		}
		fps_n++;
		if (SDL_GetTicks() - fps_t >= 1000) {
			fps = fps_n; fps_n = 0; fps_t = SDL_GetTicks();
			static int ruim;      // ritmo do monitor: se o vsync não segura ~60 fps por 2 s seguidos, volta ao ritmo pelo som
			if (ritmo_monitor_ativo && !pausado && fps < (Uint32)(NEO_HZ - 5)) { if (++ruim >= 2) { ritmo_falhou = 1; ritmo_calcula(win, dev != 0); ritmo_aviso_ate = SDL_GetTicks() + 6000; if (getenv("KOF_DEBUG_FILA")) fprintf(stderr, "ritmo do monitor desligado: %u fps\n", fps); } }
			else ruim = 0;
		}
		if (SDL_GetTicks() < ritmo_aviso_ate) { ui_alvo(tela_ui); ui_texto_centro(NEO_H - 24, "Ritmo do monitor desligado: o vsync não segurou 60 fps", 0xffffe060u); }
		if (cfg.mostra_fps) {
			char b[16];
			snprintf(b, sizeof b, "%u fps", fps);
			ui_alvo(tela_ui);
			ui_texto(NEO_W - ui_largura(b) - 4, NEO_H - 12, b, 0xff60ff90u);
		}
		apresenta();
		}
		if (autofoto && neo_quadros >= (u32)atoi(autofoto)) {
			int ow, oh;
			SDL_GetRendererOutputSize(ren, &ow, &oh);
			SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
			SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
			SDL_SaveBMP(sf, strchr(autofoto, ',') ? strchr(autofoto, ',') + 1 : "saida.bmp");
			SDL_FreeSurface(sf);
			rodando = 0;
		}

		// ritmo: com som, a fila de áudio manda (nunca descarta nem falta amostra); sem som, relógio.
		// No ritmo do monitor o vsync já segurou o laço (passos acima corrige a fila).
		if (dev && !pausado && ritmo_monitor_ativo && !rapido) {
			proximo = (double)SDL_GetPerformanceCounter();
		} else if (dev && !pausado) {
			const Uint32 alvo = 4u * (Uint32)((double)AUDIO_TAXA / NEO_HZ * 2);   // ~2 quadros de som na fila
			Uint64 t0 = SDL_GetPerformanceCounter();
			while (SDL_GetQueuedAudioSize(dev) > alvo) SDL_Delay(1);
			if (rede_parado) SDL_Delay(1);      // esperando o outro lado: sem fila de som pra segurar o laço
			t_espera += (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
			proximo = (double)SDL_GetPerformanceCounter();
		} else {
			proximo += passo;
			double agora = (double)SDL_GetPerformanceCounter();
			if (proximo > agora) SDL_Delay((Uint32)((proximo - agora) * 1000.0 / freq));
			else if (agora - proximo > passo * 5) proximo = agora;
		}
	}
	if (!autofoto || getenv("KOF_SALVA_CFG")) config_salvar(cfg_arquivo);   // testes automáticos não mexem na config do usuário
	rede_fim();
	SDL_Quit();
	return 0;
}
