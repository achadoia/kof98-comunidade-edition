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
#include "musica.h"
#include "efeitos.h"
#include "frontend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#ifdef KOF_SOLO
#include "stb_image.h"
#endif

char cfg_arquivo[512] = "kof98.ini";
static u32 tela[NEO_H][NEO_W];

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

// ---------------------------------------------------------------- vídeo
static SDL_Renderer *ren;
static SDL_Texture *tex_jogo, *tex_fundo, *tex_linhas, *tex_mascara;
static int linhas_int = -1, mascara_w, mascara_h, mascara_int = -1;

typedef struct { int linear, linhas, mascara; } Filtro;   // linhas/máscara: 0-255 de escurecimento
static const Filtro FILTROS[NFILTROS] = {
	{0, 0, 0},       // nítido
	{1, 0, 0},       // suave
	{0, 90, 0},      // linhas de TV
	{1, 70, 45},     // tubo CRT
	{0, 120, 60},    // fliperama
};

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

static void apresenta(void)
{
	int ow, oh;
	SDL_GetRendererOutputSize(ren, &ow, &oh);
	SDL_Rect r = area_jogo(ow, oh);
	const Filtro *f = &FILTROS[cfg.filtro];
	SDL_UpdateTexture(tex_jogo, NULL, tela, NEO_W * 4);
	SDL_SetTextureScaleMode(tex_jogo, f->linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	if (cfg.fundo_borrado && (r.w < ow || r.h < oh)) {
		SDL_UpdateTexture(tex_fundo, NULL, tela, NEO_W * 4);
		SDL_RenderCopy(ren, tex_fundo, NULL, NULL);
	}
	SDL_RenderCopy(ren, tex_jogo, NULL, &r);
	prepara_linhas(f->linhas);
	if (tex_linhas) SDL_RenderCopy(ren, tex_linhas, NULL, &r);
	prepara_mascara(r.w, r.h, f->mascara);
	if (tex_mascara) SDL_RenderCopy(ren, tex_mascara, NULL, &r);
	SDL_RenderPresent(ren);
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
#else
	{
		char t[700];
		snprintf(t, sizeof t, "%sarte/troca_tiles.bin", basep ? basep : "");
		video_troca_carrega(t);
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
	if (!nz) {                       // todo .zip ao lado do EXE e em roms\ (kof98ae2016.zip + neogeo.zip)
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
			treino_entradas();
			config_aplica_cenario();
#ifdef KOF_TESTE
			{   // KOF_POKE="endereco:valor:ini:fim" (hexa:hexa:dec:dec) — escreve um byte a cada quadro no intervalo
				const char *pk = getenv("KOF_POKE");
				unsigned pa, pv, pi, pf;
				if (pk && sscanf(pk, "%x:%x:%u:%u", &pa, &pv, &pi, &pf) == 4 && neo_quadros >= pi && neo_quadros <= pf)
					m68k_write_memory_8(pa, pv);
			}
#endif
			neo_frame();
			treino_depois_quadro();
#ifdef KOF_TESTE
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
		treino_desenha(tela);
		menu_cfg_desenha(tela);
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
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_Rect util = {0, 0, 1280, 720};
	SDL_GetDisplayUsableBounds(0, &util);
	int escala = (util.h * 85 / 100) / NEO_H;
	if ((util.w * 85 / 100) / NEO_W < escala) escala = (util.w * 85 / 100) / NEO_W;
	if (escala < 1) escala = 1;
	const char *autofoto = getenv("KOF_AUTOFOTO");      // "quadros,arquivo.bmp": captura a saída final e sai
	if (getenv("KOF_MUDO")) cfg.mudo = 1;
	if (getenv("KOF_VSYNC")) cfg.vsync = atoi(getenv("KOF_VSYNC"));
	if (getenv("KOF_FILTRO")) cfg.filtro = atoi(getenv("KOF_FILTRO")) % NFILTROS;
	SDL_Window *win = SDL_CreateWindow("KOF98 Anniversary - port nativo (kof98-recomp)", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, NEO_H * escala * 4 / 3, NEO_H * escala, SDL_WINDOW_RESIZABLE | (autofoto ? SDL_WINDOW_HIDDEN : 0));
	SDL_SetWindowSize(win, NEO_H * escala * 4 / 3, NEO_H * escala);
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
	tex_fundo = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NEO_W, NEO_H);
	SDL_SetTextureScaleMode(tex_fundo, SDL_ScaleModeLinear);
	SDL_SetTextureColorMod(tex_fundo, 60, 60, 70);
	int vsync_atual = cfg.vsync, cheia_atual = 0;
	if (cfg.tela_cheia) { SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP); cheia_atual = 1; }

	SDL_AudioSpec quero = {0}, tenho;
	quero.freq = AUDIO_TAXA; quero.format = AUDIO_S16SYS; quero.channels = 2; quero.samples = 1024;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &quero, &tenho, 0);
	if (dev) SDL_PauseAudioDevice(dev, 0);
	entrada_init();

	// menu inicial (Comunidade Edition): o jogo roda por trás dele desde o reset
	{
		char *bp = SDL_GetBasePath();
		if (frontend_carrega(bp) && !est_ini) frontend_abre();
		SDL_free(bp);
	}
#ifdef KOF_TESTE
	{ void frontend_escolhe_teste(int); if (getenv("KOF_MENU_ESCOLHA")) frontend_escolhe_teste(atoi(getenv("KOF_MENU_ESCOLHA"))); }
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
			if (frontend_ativo) {          // no menu inicial só vale Esc (sai), F11/Alt+Enter e F12
				if (e.type == SDL_KEYDOWN && !e.key.repeat) {
					SDL_Scancode s = e.key.keysym.scancode;
					if (s == SDL_SCANCODE_ESCAPE) rodando = 0;
					if (s == SDL_SCANCODE_F11 || (s == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))) { cfg.tela_cheia = !cfg.tela_cheia; config_salvar(cfg_arquivo); }
				}
				continue;
			}
			if (!treino_aberto && menu_cfg_evento(&e)) continue;
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
			if (t && treino_tecla(t)) continue;
			if (sc == SDL_SCANCODE_ESCAPE) { if (frontend_tem_arte()) frontend_abre(); else rodando = 0; }   // Esc volta pro menu
			if (sc == SDL_SCANCODE_F11 || (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))) { cfg.tela_cheia = !cfg.tela_cheia; config_salvar(cfg_arquivo); }
			if (sc == SDL_SCANCODE_F4) { cfg.filtro = (cfg.filtro + 1) % NFILTROS; config_salvar(cfg_arquivo); }
			if (sc == SDL_SCANCODE_F3) neo_reset();
			if (sc == SDL_SCANCODE_F12) salva_bmp("kof98_foto.bmp");
		}
		if (cfg.tela_cheia != cheia_atual) { cheia_atual = cfg.tela_cheia; SDL_SetWindowFullscreen(win, cheia_atual ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
		if (cfg.vsync != vsync_atual) { vsync_atual = cfg.vsync; SDL_RenderSetVSync(ren, vsync_atual); }
		neo_cpu_pct = cfg.cpu;
		musica_ligada = cfg.musica_arquivos;
		efeitos_ligados = cfg.efeitos_arquivos;

		entrada_le();
		int pausado = !frontend_ativo && (cfgmenu_aberto || treino_aberto || (sem_foco && cfg.pausa_sem_foco));
		if (frontend_ativo) frontend_controles(&rodando);
		int passos = pausado ? 0 : frontend_passos();   // >1 = avanço rápido escondido atrás do menu
		for (int passo_i = 0; passo_i < passos; passo_i++) {
			int piloto = frontend_piloto();
			if (!piloto) config_aplica_entradas();
#ifdef KOF_TESTE
			if (getenv("KOF_DEBUG_ENTRADA") && neo_in_start != 0x0f) fprintf(stderr, "start %x quadro %u\n", neo_in_start, neo_quadros);
#endif
			if (!piloto && SDL_GetKeyboardState(NULL)[SDL_SCANCODE_F2]) neo_in_test = 0;
			if (!piloto) treino_entradas();
			config_aplica_cenario();
			neo_frame();
			treino_depois_quadro();
			int n = audio_pega_amostras(amostras, 8192);
			if (passos > 1) continue;              // avanço rápido: som descartado
			int vol = cfg.mudo ? 0 : cfg.volume;
			if (vol != 100) for (int i = 0; i < n * 2; i++) amostras[i] = (s16)(amostras[i] * vol / 100);
			if (dev) {
				Uint32 fila = SDL_GetQueuedAudioSize(dev);
				if (getenv("KOF_DEBUG_FILA")) {      // diagnóstico: fila de áudio antes de enfileirar
					static Uint32 t0, mn = ~0u, mx, vazias, quadros;
					if (!t0) t0 = SDL_GetTicks();
					if (fila < mn) mn = fila;
					if (fila > mx) mx = fila;
					if (fila == 0) vazias++;
					quadros++;
					if (SDL_GetTicks() - t0 >= 1000) {
						fprintf(stderr, "fila: min %u max %u vazias %u quadros %u\n", mn / 4, mx / 4, vazias, quadros);
						t0 = SDL_GetTicks(); mn = ~0u; mx = 0; vazias = 0; quadros = 0;
					}
				}
				SDL_QueueAudio(dev, amostras, (Uint32)n * 4);
			}
		}
		if (frontend_ativo) frontend_desenha(ren);
		else {
		memcpy(tela, neo_fb, sizeof tela);
		treino_desenha(tela);
		menu_cfg_desenha(tela);
		fps_n++;
		if (SDL_GetTicks() - fps_t >= 1000) { fps = fps_n; fps_n = 0; fps_t = SDL_GetTicks(); }
		if (cfg.mostra_fps) {
			char b[16];
			snprintf(b, sizeof b, "%u fps", fps);
			ui_alvo(tela);
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

		// ritmo: com som, a fila de áudio manda (nunca descarta nem falta amostra); sem som, relógio
		if (dev && !pausado) {
			const Uint32 alvo = 4u * (Uint32)((double)AUDIO_TAXA / NEO_HZ * 3);   // ~3 quadros de som na fila
			while (SDL_GetQueuedAudioSize(dev) > alvo) SDL_Delay(1);
			proximo = (double)SDL_GetPerformanceCounter();
		} else {
			proximo += passo;
			double agora = (double)SDL_GetPerformanceCounter();
			if (proximo > agora) SDL_Delay((Uint32)((proximo - agora) * 1000.0 / freq));
			else if (agora - proximo > passo * 5) proximo = agora;
		}
	}
	if (!autofoto) config_salvar(cfg_arquivo);   // testes automáticos não mexem na config do usuário
	SDL_Quit();
	return 0;
}
