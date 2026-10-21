// Menu inicial "KOF '98 Ultimate Match - Comunidade Edition": arte em 1672x941 (arte/menu_fundo.bmp, gerada
// pelo usuário) com as opções desenhadas por cima. O jogo continua rodando por trás: um piloto aperta Start
// na tela de título e espera no MODE SELECT do console; ao escolher, ele desce até o item e confirma com A
// (em avanço rápido, escondido atrás do menu) e só então mostra o jogo. Esc no jogo volta pra cá.
//
// MODE SELECT do Anniversary (AES): 0 Team Play, 1 Team VS, 2 Single Play, 3 Single VS, 4 Survivor,
// 5 Practice, 6 Option. Tempos do piloto = bin/roteiro_pratica.txt (Start no quadro 700, menu pronto no 960).
#include "frontend.h"
#include "config.h"
#include "musica.h"
#include "rede.h"
#include "atualiza.h"
#include "versao.h"
extern char font8x8_basic[128][8];   // definida em ui.c (font8x8_basic.h)
extern char font8x8_ext_latin[96][8];
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

// itens: linha, coluna, texto, ação (item do MODE SELECT; -1 = assistir, -2 = sair, -3 = em breve, -4 = online)
typedef struct { int linha, col; const char *txt; int modo; } Item;
static const Item ITENS[] = {
	{0, 0, "ARCADE PLAY", 2}, {1, 0, "TEAM PLAY", 0}, {1, 1, "2P VS", 1}, {2, 0, "SURVIVAL", 4},
	{2, 1, "ONLINE", -4}, {3, 0, "PRACTICE", 5}, {3, 1, "OPTIONS", 6}, {4, 0, "WATCH", -1}, {4, 1, "EXIT", -2},
};
#define NITENS (int)(sizeof ITENS / sizeof ITENS[0])
static const int LINHA_Y[5] = {200, 272, 345, 420, 493};   // centro de cada linha (mockup)

// piloto: 0 desligado, 1 indo pro MODE SELECT, 2 escolhendo, 3 assistindo (sem Start)
static int piloto, alvo;
static int rede_escolha_agora;   // 1 = escolha vinda dos quadros da rede (executa de verdade nos dois lados)
static int cursor_ini;        // item onde o cursor do MODE SELECT estava ao escolher (voltando do OPTION fica no 6)
static int vigia_n;           // quadros seguidos com o MODE SELECT original na tela
static u32 q_rel, q_escolha, q_som, q_fim;   // quadros desde que o menu abriu (neo_reset não zera neo_quadros); q_som = confirmação
// telas do jogo online: 0 nenhuma, 1 submenu, 2 anfitrião (sala criada), 3 convidado digitando, 4 convidado procurando
static int tela_rede, sub_sel;
static char digitado[40];
static u32 t_copiado;
#define ITEM_2PVS 2

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
		if (ch >= 0xc0 && (s[1] & 0xc0) == 0x80) { ch = ((ch & 0x1f) << 6) | (s[1] & 0x3f); s++; }   // UTF-8 de 2 bytes (acentos)
		const char *g = ch < 128 ? font8x8_basic[ch] : (ch >= 0xa0 && ch <= 0xff) ? font8x8_ext_latin[ch - 0xa0] : font8x8_basic['?'];
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

// ---------------------------------------------------------------- telas do jogo online
static const char *SUB[] = {"CRIAR SALA", "ENTRAR COM CÓDIGO", "VOLTAR"};
static void compoe_rede(void)
{
	faixa(80, AW - 80, 470, 700, 0xff000000, 215, 215);
	u32 BR = 0xffffffffu, LAR = 0xffff5a1eu, CZ = 0xffb8b0a8u, VD = 0xff60ff90u, VM = 0xffff6060u;
	const char *st = rede_status();
	if (tela_rede == 1) {
		texto(150, 170, "JOGO ONLINE", LAR, 5);
		texto(150, 230, "Direto entre os dois PCs, sem servidor. Um cria a sala e manda o código", CZ, 2);
		texto(150, 254, "pro outro (WhatsApp); o outro entra com o código. Quem cria joga de 1P, quem entra de 2P.", CZ, 2);
		texto(150, 290, "Cada um usa o controle configurado como JOGADOR 1 no seu F1 - o jogo troca por baixo.", VD, 2);
		for (int i = 0; i < 3; i++) {
			int y = 340 + i * 90, e = i == sub_sel;
			if (e) faixa(110, 900, y, 56, 0xffff4b14, 245, 30);
			texto(152, y - 16, SUB[i], e ? BR : 0xfff0e6dcu, 4);
			if (e) { seta(130, y, -1, BR); seta(152 + largura(SUB[i], 4) + 22, y, 1, BR); }
		}
		if (rede_estado == REDE_ERRO && rede_msg[0]) texto(150, 640, rede_msg, VM, 2);
		texto(150, 700, "Pra Radmin VPN / ZeroTier / rede local também dá pra digitar ip:porta no lugar do código.", CZ, 2);
	} else if (tela_rede == 2) {
		texto(150, 150, "SALA CRIADA", LAR, 5);
		if (rede_codigo[0]) {
			texto(150, 230, "CÓDIGO DA SALA (manda pro seu amigo):", CZ, 2);
			texto(150, 270, rede_codigo, BR, 12);
			if (SDL_GetTicks() - t_copiado < 4000) texto(150, 390, "copiado pra área de transferência", VD, 2);
		}
		texto(150, 440, st, rede_estado == REDE_ERRO ? VM : CZ, 2);
		texto(150, 500, "A sala vale por 2 horas. O amigo escolhe ONLINE > ENTRAR COM CÓDIGO e digita.", CZ, 2);
		texto(150, 660, "ESC cancela a sala", CZ, 2);
	} else if (tela_rede == 3 || tela_rede == 4) {
		texto(150, 150, "ENTRAR NA SALA", LAR, 5);
		texto(150, 230, tela_rede == 3 ? "Digite o código da sala (3 números + 3 letras):" : "Código:", CZ, 2);
		char b[48]; snprintf(b, sizeof b, "%s%s", digitado, tela_rede == 3 && (pisca / 20) & 1 ? "_" : "");
		texto(150, 270, b, BR, 10);
		if (tela_rede == 3) texto(150, 400, "ENTER conecta   ESC volta   (ou ip:porta pra Radmin VPN / rede local)", CZ, 2);
		else {
			texto(150, 440, st, rede_estado == REDE_ERRO ? VM : CZ, 2);
			texto(150, 660, "ESC cancela", CZ, 2);
		}
	}
}

static int atualiza_na_tela(void)
{
	return !atualiza_adiada && (atualiza_estado == ATU_NOVA || atualiza_estado == ATU_BAIXANDO || atualiza_estado == ATU_PRONTA || (atualiza_estado == ATU_ERRO && atualiza_msg[0] && atualiza_versao[0]));
}
static void compoe_atualiza(void)
{
	u32 BR = 0xffffffffu, LAR = 0xffff5a1eu, CZ = 0xffb8b0a8u, VM = 0xffff6060u;
	faixa(80, AW - 80, 470, 700, 0xff000000, 225, 225);
	char b[200];
	snprintf(b, sizeof b, "NOVA VERSÃO %s", atualiza_versao);
	texto(150, 200, b, LAR, 5);
	snprintf(b, sizeof b, "Você está na %s.", KOF_VERSAO);
	texto(150, 260, b, CZ, 2);
	if (atualiza_estado == ATU_NOVA) {
		texto(150, 340, "Atualizando automaticamente...", BR, 4);
		texto(150, 385, "(baixa, troca o EXE e reabre o jogo sozinho)", CZ, 2);
		texto(150, 500, "Pra jogar ONLINE os dois precisam estar na mesma versão.", CZ, 2);
	} else if (atualiza_estado == ATU_BAIXANDO) {
		unsigned t = atualiza_total, n = atualiza_bytes;
		snprintf(b, sizeof b, "Baixando...  %.1f MB%s", n / 1048576.0, t ? "" : "");
		if (t) snprintf(b + strlen(b), sizeof b - strlen(b), " de %.1f MB", t / 1048576.0);
		texto(150, 340, b, BR, 3);
		int larg = 1300, feito = t ? (int)((long long)larg * n / t) : 0;
		faixa(150, 150 + larg, 420, 30, 0xff333333, 255, 255);
		if (feito > 0) faixa(150, 150 + feito, 420, 30, 0xffff4b14, 255, 255);
	} else if (atualiza_estado == ATU_PRONTA) {
		texto(150, 340, "Pronto! Reiniciando com a versão nova...", BR, 3);
	} else {
		texto(150, 340, "Não deu pra atualizar:", VM, 3);
		texto(150, 390, atualiza_msg, VM, 2);
		texto(150, 460, "ESC continua na versão atual. Baixe à mão em github.com/achadoia/kof98-comunidade-edition", CZ, 2);
	}
}

static void compoe(void)
{
	memcpy(tela, fundo, AW * AH * 4);
	if (atualiza_na_tela()) { compoe_atualiza(); return; }
	if (tela_rede) { compoe_rede(); return; }
	for (int l = 1; l < 5; l++) {                     // linhas com 2 opções: faixa escura + traço laranja
		faixa(120, 850, LINHA_Y[l], 56, 0xff07080a, 200, 120);
		for (int x = 130; x < 850; x++) for (int t = 0; t < 3; t++) mistura(x + 4, LINHA_Y[l] + 30 + t, 0xffff5a1e, 200);
	}
	for (int i = 0; i < NITENS; i++) {
		const Item *it = &ITENS[i];
		int y = LINHA_Y[it->linha], escolhido = i == sel, emb = it->modo == -3 || (rede_ativa && i != ITEM_2PVS && it->modo != -2);
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
	if (rede_ativa) {
		char b[160];
		snprintf(b, sizeof b, rede_anfitriao ? "ONLINE: você é o 1P (ping %d ms). Escolha 2P VS pra lutar; EXIT encerra." : "ONLINE: você é o 2P (ping %d ms). O anfitrião escolhe. ESC sai da sala.", rede_ping);
		texto(60, 40, b, 0xff60ff90u, 2);
	} else texto(60, 40, "SETAS escolhem   A / ENTER confirma   ESC no jogo volta aqui", 0xffb8b0a8u, 2);
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
	musica_modo_menu = 1;        // o tema de abertura segue (a 50%) até o jogador escolher
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
	if (it->modo == -4) { tela_rede = 1; sub_sel = 0; sujo = 1; return; }
	if (rede_ativa && !rede_anfitriao && !rede_escolha_agora) return;        // convidado: só o anfitrião escolhe
	if (rede_ativa && it->modo == -2) { rede_desligar(); sujo = 1; return; }   // EXIT na sala = sair da sala
	if (rede_ativa && it->modo != 1) return;
	if (rede_ativa && !rede_escolha_agora) { if (rede_escolha_pendente < 0) rede_escolha_pendente = sel; return; }   // vai pelos quadros (os dois lados juntos)
	if (it->modo == -2) { *rodando = 0; return; }
	if (it->modo == -1) { musica_menu_fim(0); neo_reset(); piloto = 3; frontend_ativo = 0; return; }
	alvo = it->modo;
	cursor_ini = (int)m68k_read_memory_8(RAM_CURSOR);
	if (cursor_ini < 0 || cursor_ini > 6 || m68k_read_memory_8(RAM_TELA) != 1) cursor_ini = 0;
	piloto = 2;
	q_escolha = 0;
	q_fim = 0;
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
	if (atualiza_na_tela()) {                          // aviso de versão nova por cima de tudo
		sujo = 1;
		if (atualiza_estado == ATU_PRONTA) { if (atualiza_aplicar()) *rodando = 0; return; }   // reinicia sozinho
		// 1.8.2+: atualiza SOZINHO (pedido do Matheus: achar a versão atual ao abrir e atualizar automaticamente).
		// KOF_ATUALIZA_MANUAL=1 volta a esperar ENTER (dev/teste).
		if (atualiza_estado == ATU_NOVA && (!getenv("KOF_ATUALIZA_MANUAL") || novo[4])) { audio_comando(0x71); atualiza_baixar(); }
		return;
	}
	if (tela_rede == 1) {
		int s = sub_sel;
		if (novo[0]) s = (s + 2) % 3;
		if (novo[1]) s = (s + 1) % 3;
		if (s != sub_sel) { sub_sel = s; sujo = 1; audio_comando(0x70); }
		if (novo[4]) {
			audio_comando(0x71);
			if (sub_sel == 2) { tela_rede = 0; rede_desligar(); }
			else if (sub_sel == 0) { digitado[0] = 0; tela_rede = 2; rede_criar(); }
			else { digitado[0] = 0; tela_rede = 3; SDL_StartTextInput(); }
			sujo = 1;
		}
		return;
	}
	if (tela_rede) { sujo = 1; return; }               // telas de digitação: eventos em frontend_evento
	if (piloto == 2) return;                           // já escolhido: esperando o jogo chegar lá
	if (rede_ativa && !rede_anfitriao) return;         // convidado só assiste o menu
	int s = sel;
	if (novo[0]) s = mover(-1, 0);
	if (novo[1]) s = mover(1, 0);
	if (novo[2] || novo[3]) s = mover(0, 1);
	if (s != sel) { sel = s; sujo = 1; audio_comando(0x70); }   // som de cursor do próprio MODE SELECT
	if (novo[4]) escolhe(rodando);
}

int frontend_tem_arte(void) { return fundo != NULL; }

// teclado nas telas de código (texto digitado, colar, apagar, Enter); 1 = consumiu
static void digita(const char *p)
{
	for (; *p; p++) {
		char c = (char)toupper((unsigned char)*p);
		if ((isalnum((unsigned char)c) || c == '-' || c == '.' || c == ':') && strlen(digitado) < sizeof digitado - 1) { size_t n = strlen(digitado); digitado[n] = c; digitado[n + 1] = 0; }
	}
}
int frontend_evento(const SDL_Event *e)
{
	if (!frontend_ativo || tela_rede != 3) return 0;
	int cab = 0;
	if (e->type == SDL_TEXTINPUT) { digita(e->text.text); cab = 1; }
	else if (e->type == SDL_KEYDOWN) {
		SDL_Scancode s = e->key.keysym.scancode;
		if (s == SDL_SCANCODE_BACKSPACE) { size_t n = strlen(digitado); if (n) digitado[n - 1] = 0; cab = 1; }
		else if (s == SDL_SCANCODE_V && (e->key.keysym.mod & KMOD_CTRL)) { char *t = SDL_GetClipboardText(); if (t) { digita(t); SDL_free(t); } cab = 1; }
		else if (s == SDL_SCANCODE_RETURN || s == SDL_SCANCODE_KP_ENTER) {
			if (digitado[0]) { tela_rede = 4; rede_entrar(digitado); cab = 1; }
		}
	}
	if (cab) sujo = 1;
	return cab;
}

// Esc no menu: fecha a tela online que estiver aberta (e a sala); 0 = nada aberto (quem chamou decide sair)
int frontend_voltar(void)
{
	if (!frontend_ativo) return 0;
	if (atualiza_na_tela()) { if (atualiza_estado != ATU_BAIXANDO && atualiza_estado != ATU_PRONTA) { atualiza_adiada = 1; sujo = 1; } return 1; }
	if (rede_ativa) { rede_desligar(); sujo = 1; return 1; }
	if (!tela_rede) return 0;
	if (tela_rede == 1) tela_rede = 0; else { rede_desligar(); tela_rede = 1; }
	sujo = 1;
	return 1;
}

// chamado pela rede quando os dois lados conectaram: os dois reiniciam o jogo e vão pro 2P VS no mesmo quadro
void frontend_online_comeca(void)
{
	frontend_abre();
	tela_rede = 0;
	sel = ITEM_2PVS;
	rede_escolha_agora = 1; int r = 1; escolhe(&r); rede_escolha_agora = 0;
}
void frontend_online_caiu(void)       // o outro lado saiu / conexão perdida
{
	if (!frontend_ativo) frontend_abre();
	piloto = 0; tela_rede = 1; sujo = 1;
}
void frontend_escolhe_item(int item)  // escolha do anfitrião chegando pelos quadros (executa nos dois lados)
{
	if (item < 0 || item >= NITENS || !frontend_ativo) return;
	sel = item; sujo = 1;
	rede_escolha_agora = 1; int r = 1; escolhe(&r); rede_escolha_agora = 0;
}
void frontend_copiou(void) { t_copiado = SDL_GetTicks(); }

#ifdef KOF_TESTE
void frontend_rede_teste(const char *s)   // KOF_REDE=criar | entrar:<código ou ip:porta>
{
	if (!frontend_ativo) frontend_abre();
	if (!strcmp(s, "criar")) { tela_rede = 2; digitado[0] = 0; rede_criar(); }
	else if (!strncmp(s, "entrar:", 7)) { tela_rede = 4; snprintf(digitado, sizeof digitado, "%s", s + 7); rede_entrar(digitado); }
	sujo = 1;
}
#endif

// O MODE SELECT original nunca fica na tela: sempre que o jogo chega nele por conta própria (Start no título,
// volta do OPTION, fim de jogo), o nosso menu abre por cima, sem reset, já pronto pra escolher.
void frontend_vigia(void)
{
	if (!fundo || frontend_ativo || piloto) { vigia_n = 0; return; }
	if (!rede_quadro_real(rede_quadro() - 1)) { vigia_n = 0; return; }   // online: só em quadro confirmado (um quadro previsto pode ser desfeito pelo rollback)
	int ms = m68k_read_memory_8(RAM_TELA) == 1 && !m68k_read_memory_8(RAM_FASE) && !m68k_read_memory_8(RAM_OPTION);
	if (!ms) { vigia_n = 0; return; }
	if (++vigia_n < 6) return;
	vigia_n = 0;
	musica_modo_menu = 1;
	frontend_ativo = 1;
	piloto = 1;
	q_rel = Q_MENU;              // já está no MODE SELECT: nada de Start
	sujo = 1;
	memset(antes, 1, sizeof antes);
}
int frontend_passos(void) { return piloto == 2 && q_rel - q_som >= 30 ? (rede_ativa ? 4 : 12) : 1; }   // 30 quadros normais: o som de confirmar toca inteiro

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
#ifdef KOF_TESTE
		if (getenv("KOF_DEBUG_PILOTO") && r < 130 && r % 4 == 0) fprintf(stderr, "piloto2 q=%u r=%u cursor=%02x tela=%d fase=%02x\n", q, r, m68k_read_memory_8(RAM_CURSOR), m68k_read_memory_8(RAM_TELA), m68k_read_memory_8(RAM_FASE));
#endif
		int toques = alvo - cursor_ini, bit = 2;                 // baixo; voltando do OPTION o cursor está no 6: sobe
		if (toques < 0) { toques = -toques; bit = 1; }
		for (int i = 0; i < toques; i++) if (r >= (u32)(i * PASSO) && r < (u32)(i * PASSO + 4)) neo_in_p1 &= (u8)~bit;
		u32 qa = (u32)(toques * PASSO + 8);
		if (r >= qa && r < qa + 5) neo_in_p1 &= (u8)~0x10;                                                           // A
		if (alvo == 6) { if (r >= qa + 30) { piloto = 0; frontend_ativo = 0; musica_menu_fim(1); } }
		else if (r >= qa + 30) {
			int tela = m68k_read_memory_8(RAM_TELA);
#ifdef KOF_TESTE
			if (getenv("KOF_DEBUG_PILOTO") && r % 30 == 0) fprintf(stderr, "piloto q=%u r=%u tela=%d q_fim=%u cursor=%02x option=%02x fase=%02x alvo=%d toques=%d\n", q, r, tela, q_fim, m68k_read_memory_8(RAM_CURSOR), m68k_read_memory_8(RAM_OPTION), m68k_read_memory_8(RAM_FASE), alvo, toques);
#endif
			if (tela == 1) { if ((r - qa) % 24 < 4) neo_in_p1 &= (u8)~0x10; }   // CONTROLS e afins: pula com A
			else if (tela == 2) { if (!q_fim) q_fim = r; if (r >= q_fim + 40) { piloto = 0; frontend_ativo = 0; musica_menu_fim(1); } }
			if (r > qa + 1200) { piloto = 0; frontend_ativo = 0; musica_menu_fim(1); }            // segurança: nunca prende o jogador
		}
	}
	return 1;
}

void frontend_desenha(SDL_Renderer *ren)
{
	pisca++;
	if (!tex) tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, AW, AH);
	if (tela_rede == 2 && rede_codigo[0] && !t_copiado) { SDL_SetClipboardText(rede_codigo); t_copiado = SDL_GetTicks(); sujo = 1; }
	if (tela_rede != 2) t_copiado = 0;
	if (sujo || pisca % 20 == 0 || piloto == 2 || tela_rede || atualiza_na_tela()) { compoe(); SDL_UpdateTexture(tex, NULL, tela, AW * 4); sujo = 0; }
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
