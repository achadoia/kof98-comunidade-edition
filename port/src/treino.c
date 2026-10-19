// Treino avançado (F6): menu desenhado pelo EXE por cima do jogo, hitboxes, boneco P2, gravação,
// histórico de comandos, vida/barra infinitas e savestate. Nada disso altera a ROM.
#include "treino.h"
#include "neo.h"
#include "ui.h"
#include "hd.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

#define P1 0x108100u
#define P2 0x108300u
#define VIDA_OFF 0x150      // vida no Anniversary (no kof98 original é 0x138)
#define VIDA_MAX 0x67
#define BARRA_OFF 0xE8
#define ESTOQUE_OFF 0x1E3

int treino_aberto;
static int hitboxes = 0, boneco = 0, repetir = 1, vida_inf = 0, barra_inf = 0, mostra_cmd = 0;
static int item;
u32 treino_aviso_ate = 60 * 8;        // dica "F6" nos primeiros segundos
static char msg[64];
static u32 msg_ate;

// gravação do boneco: entradas relativas (bit2 = trás, bit3 = frente)
#define GRAV_MAX (60 * 30)
static u8 grav[GRAV_MAX];
static int grav_n, grav_pos, gravando, reproduzindo;

// histórico de comandos do P1
typedef struct { u8 bits; u16 quadros; } Hist;
static Hist hist[16];
static int nhist;

static const char *BONECO[] = {"livre (jogo)", "em pé", "agachado", "defende em pé", "defende agachado", "pula", "pula pra trás"};
#define NBONECO 7

static u8 rd8(u32 a) { return (u8)m68k_read_memory_8(a); }
static u16 rd16(u32 a) { return (u16)m68k_read_memory_16(a); }

static void aviso(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_ate = neo_quadros + 120; }

// lado: bit0 de +0x31 = virado pra direita
static int olha_direita(u32 p) { return rd8(p + 0x31) & 1; }

static u8 para_relativo(u8 bits, int direita)
{
	u8 r = bits & 0xf3;                  // mantém cima/baixo e botões (ativos em 1 aqui)
	int esq = bits & 4, dir = bits & 8;
	if (direita) { if (esq) r |= 4; if (dir) r |= 8; }
	else { if (dir) r |= 4; if (esq) r |= 8; }
	return r;                            // bit2 = trás, bit3 = frente
}

static u8 para_absoluto(u8 rel, int direita)
{
	u8 r = rel & 0xf3;
	int tras = rel & 4, frente = rel & 8;
	if (direita) { if (tras) r |= 4; if (frente) r |= 8; }
	else { if (tras) r |= 8; if (frente) r |= 4; }
	return r;
}

// ---------------------------------------------------------------- antes de cada quadro do jogo
void treino_entradas(void)
{
	u8 p1 = (u8)~neo_in_p1;              // ativos em 1
	// histórico
	if (nhist && hist[0].bits == p1) { if (hist[0].quadros < 999) hist[0].quadros++; }
	else {
		memmove(&hist[1], &hist[0], sizeof(Hist) * 15);
		hist[0].bits = p1; hist[0].quadros = 1;
		if (nhist < 16) nhist++;
	}

	int dir2 = olha_direita(P2);
	if (gravando) {                      // você controla o boneco; o P1 fica parado
		if (grav_n < GRAV_MAX) grav[grav_n++] = para_relativo(p1, dir2);
		else { gravando = 0; aviso("gravação cheia (30 s)"); }
		neo_in_p2 = (u8)~p1;
		neo_in_p1 = 0xff;
		return;
	}
	if (reproduzindo && grav_n) {
		neo_in_p2 = (u8)~para_absoluto(grav[grav_pos], dir2);
		if (++grav_pos >= grav_n) { grav_pos = 0; if (!repetir) reproduzindo = 0; }
		return;
	}
	if (boneco) {
		u8 b = 0;
		switch (boneco) {
		case 2: b = 0x02; break;                              // baixo
		case 3: b = 0x04; break;                              // trás
		case 4: b = 0x06; break;                              // baixo + trás
		case 5: b = 0x01; break;                              // cima
		case 6: b = 0x05; break;                              // cima + trás
		}
		neo_in_p2 = (u8)~para_absoluto(b, dir2);
	}
}

void treino_depois_quadro(void)
{
	if (vida_inf) {
		m68k_write_memory_16(P1 + VIDA_OFF, VIDA_MAX);
		m68k_write_memory_16(P2 + VIDA_OFF, VIDA_MAX);
	}
	if (barra_inf) {
		m68k_write_memory_16(P1 + BARRA_OFF, 0x7f00);
		m68k_write_memory_16(P2 + BARRA_OFF, 0x7f00);
		m68k_write_memory_8(P1 + ESTOQUE_OFF, 3);
		m68k_write_memory_8(P2 + ESTOQUE_OFF, 3);
	}
}

// ---------------------------------------------------------------- desenho
static void hitboxes_de(u32 p)
{
	int sx = (s16)rd16(p + 0x24), sy = (s16)rd16(p + 0x26) - 16;
	int dir = olha_direita(p);
	u8 ativas = rd8(p + 0x7c);
	for (int k = 0; k < 4; k++) {
		if (!((ativas >> k) & 1)) continue;
		u32 b = p + 0x90 + 5 * k;
		int xo = (int8_t)rd8(b + 1), yo = (int8_t)rd8(b + 2), w = rd8(b + 3), h = rd8(b + 4);
		if (!w || !h) continue;
		int cx = sx + (dir ? -xo : xo), cy = sy + yo;
		u32 cor = k == 0 ? 0xffff3030u : 0xff3080ffu;            // vermelho = ataque, azul = corpo
		ui_retangulo(cx - w, cy - h, cx + w, cy + h, cor, 64);
	}
	ui_retangulo(sx - 1, sy - 1, sx + 1, sy + 1, 0xffffffffu, 0);      // ponto de referência no chão
}

static void desenha_cmd(void)
{
	for (int i = 0; i < nhist && i < 14; i++) {
		u8 b = hist[i].bits;
		char s[16];
		int v = (b & 1) ? 2 : (b & 2) ? 0 : 1;          // linha do teclado numérico (7-9 / 4-6 / 1-3)
		int h = (b & 4) ? 0 : (b & 8) ? 2 : 1;
		char d = (char)('1' + v * 3 + h);
		snprintf(s, sizeof s, "%3u %c%s%s%s%s", hist[i].quadros, d, b & 0x10 ? "A" : "", b & 0x20 ? "B" : "",
			b & 0x40 ? "C" : "", b & 0x80 ? "D" : "");
		ui_texto(4, 40 + i * 10, s, i == 0 ? 0xffffff60u : 0xffe0e0e0u);
	}
}

static const char *liga(int v) { return v ? "LIGADO" : "DESLIGADO"; }

typedef struct { const char *nome; char valor[40]; } Linha;
#define NITENS 10

static void monta_itens(Linha *L)
{
	snprintf(L[0].valor, 40, "%s", liga(hitboxes)); L[0].nome = "Hitboxes";
	snprintf(L[1].valor, 40, "%s", BONECO[boneco]); L[1].nome = "Boneco (P2)";
	snprintf(L[9].valor, 40, "%s", config_nome_cenario(cfg.cenario)); L[9].nome = "Cenário";
	snprintf(L[2].valor, 40, "%s", gravando ? "GRAVANDO" : reproduzindo ? "TOCANDO" : grav_n ? "pronta" : "vazia");
	L[2].nome = "Gravação (F7/F8)";
	snprintf(L[3].valor, 40, "%s", liga(repetir)); L[3].nome = "Repetir gravação";
	snprintf(L[4].valor, 40, "%s", liga(mostra_cmd)); L[4].nome = "Mostrar comandos";
	snprintf(L[5].valor, 40, "%s", liga(vida_inf)); L[5].nome = "Vida infinita";
	snprintf(L[6].valor, 40, "%s", liga(barra_inf)); L[6].nome = "Barra infinita";
	snprintf(L[7].valor, 40, "F5 / F9"); L[7].nome = "Salvar / carregar estado";
	snprintf(L[8].valor, 40, " "); L[8].nome = "Voltar ao jogo";
}

void treino_desenha(u32 tela[NEO_H][NEO_W])
{
	ui_alvo(tela);
	if (hitboxes) { hitboxes_de(P1); hitboxes_de(P2); }
	if (mostra_cmd) desenha_cmd();
	if (gravando) ui_texto(NEO_W - 8 * 8 - 4, 4, "GRAVANDO", 0xffff4040u);
	else if (reproduzindo) ui_texto(NEO_W - 7 * 8 - 4, 4, "TOCANDO", 0xff40ff60u);
	if (neo_quadros < msg_ate) {
		int w = ui_largura(msg) + 16;
		ui_caixa((NEO_W - w) / 2, 186, (NEO_W + w) / 2, 200, 0xff101420u, 220);
		ui_texto((NEO_W - ui_largura(msg)) / 2, 190, msg, 0xffffffffu);
	}
	if (!treino_aberto && neo_quadros < treino_aviso_ate) {
		const char *s = "F6: treino avançado";
		int w = ui_largura(s) + 16;
		ui_caixa((NEO_W - w) / 2, 204, (NEO_W + w) / 2, 218, 0xff101420u, 200);
		ui_retangulo((NEO_W - w) / 2, 204, (NEO_W + w) / 2, 218, 0xff4dd0e0u, 0);
		ui_texto((NEO_W - ui_largura(s)) / 2, 208, s, 0xffffffffu);
	}
	if (!treino_aberto) return;
}

// ---------------------------------------------------------------- menu em alta resolução (1672x941)
void treino_desenha_hd(void)
{
	if (!treino_aberto) return;
	hd_buffer();
	Linha L[NITENS];
	monta_itens(L);
	static const int ORDEM[NITENS] = {0, 1, 9, 2, 3, 4, 5, 6, 7, 8};
	const u32 BRANCO = 0xf4f4f4u, AMARELO = 0xffd020u, LARANJA = 0xe8641eu;
	int x0 = 75, y0 = 188, x1 = 1600, y1 = 812;
	hd_caixa(x0, y0, x1, y1, 0x04050bu, 228);                       // painel por cima do jogo
	hd_borda(x0, y0, x1, y1, LARANJA, 3);
	hd_borda(x0 + 6, y0 + 6, x1 - 6, y1 - 6, 0x8a3018u, 1);
	if (hd_treino_titulo.px) hd_imagem((HD_W - hd_treino_titulo.w) / 2, 14, &hd_treino_titulo);
	else hd_texto_centro(HD_W / 2, 60, "TREINO AVANÇADO", LARANJA, 7);
	char val[64];
	for (int k = 0; k < NITENS; k++) {
		int i = ORDEM[k], sel = i == item;
		int yc = 232 + k * 58;
		if (sel) hd_degrade(x0 + 20, yc - 25, x1 - 20, yc + 25, 0xc81e14u, 0xf06a1eu, 255, 0);
		else hd_caixa(x0 + 35, yc + 28, x1 - 35, yc + 30, LARANJA, 150);
		hd_maiusc(val, L[i].valor, sizeof val);
		hd_texto(150, yc - 16, L[i].nome, BRANCO, 4);
		if (val[0] && val[0] != ' ') {
			hd_texto_centro(1255, yc - 16, val, sel ? AMARELO : BRANCO, 4);
			hd_seta(1000, yc, -1, 13, sel ? BRANCO : LARANJA);
			hd_seta(1510, yc, 1, 13, sel ? BRANCO : LARANJA);
		} else {
			hd_texto_centro(1255, yc - 16, "-", sel ? AMARELO : BRANCO, 4);
			hd_seta(1000, yc, -1, 13, LARANJA);
			hd_seta(1510, yc, 1, 13, LARANJA);
		}
	}
	// rodapé: direcional seleciona, B volta
	int rx0 = 400, rx1 = 1272, ry0 = 822, ry1 = 892;
	hd_caixa(rx0, ry0, rx1, ry1, 0x04050bu, 235);
	hd_borda(rx0, ry0, rx1, ry1, LARANJA, 2);
	int x = 530;
	hd_imagem(x, ry0 + 5, &hd_dpad); x += hd_dpad.w + 14;
	x = hd_texto(x, ry0 + 22, "SELECIONAR", BRANCO, 3) + 70;
	hd_imagem(x, ry0 + 7, &hd_botao_b); x += hd_botao_b.w + 14;
	hd_texto(x, ry0 + 22, "VOLTAR", BRANCO, 3);
}

// ---------------------------------------------------------------- teclas
static void muda(int d)
{
	switch (item) {
	case 0: hitboxes = !hitboxes; break;
	case 1: boneco = (boneco + d + NBONECO) % NBONECO; break;
	case 2: if (gravando) { gravando = 0; } else if (grav_n) { reproduzindo = !reproduzindo; grav_pos = 0; } break;
	case 3: repetir = !repetir; break;
	case 4: mostra_cmd = !mostra_cmd; break;
	case 5: vida_inf = !vida_inf; break;
	case 6: barra_inf = !barra_inf; break;
	case 9: cfg.cenario += d; if (cfg.cenario < -1) cfg.cenario = NCENARIOS - 1; if (cfg.cenario >= NCENARIOS) cfg.cenario = -1; break;
	}
}

void treino_opcao(const char *nome, int v)
{
	if (!strcmp(nome, "hitbox")) hitboxes = v;
	else if (!strcmp(nome, "cmd")) mostra_cmd = v;
	else if (!strcmp(nome, "menu")) treino_aberto = v;
	else if (!strcmp(nome, "boneco")) boneco = v;
	treino_aviso_ate = 0;
}

int treino_tecla(int tecla)
{
	switch (tecla) {
	case TECLA_F6: treino_aberto = !treino_aberto; treino_aviso_ate = 0; return 1;
	case TECLA_F5: aviso(estado_salvar("estado.sta") ? "erro ao salvar estado" : "estado salvo (F9 carrega)"); return 1;
	case TECLA_F9: aviso(estado_carregar("estado.sta") ? "nenhum estado salvo" : "estado carregado"); return 1;
	case TECLA_F7:
		if (gravando) { gravando = 0; aviso("gravação pronta: F8 toca"); }
		else { gravando = 1; reproduzindo = 0; grav_n = 0; aviso("gravando: controle o P2 (F7 para)"); }
		return 1;
	case TECLA_F8:
		if (!grav_n) { aviso("nada gravado (F7 grava)"); return 1; }
		gravando = 0; reproduzindo = !reproduzindo; grav_pos = 0;
		aviso(reproduzindo ? "tocando gravação" : "gravação parada");
		return 1;
	}
	if (!treino_aberto) return 0;
	switch (tecla) {
	case TECLA_CIMA: case TECLA_BAIXO: {
		static const int ORDEM[NITENS] = {0, 1, 9, 2, 3, 4, 5, 6, 7, 8};
		int k = 0;
		while (ORDEM[k] != item) k++;
		k = (k + (tecla == TECLA_CIMA ? NITENS - 1 : 1)) % NITENS;
		item = ORDEM[k];
		return 1;
	}
	case TECLA_ESQ: muda(-1); return 1;
	case TECLA_DIR: muda(1); return 1;
	case TECLA_OK:
		if (item == 8) treino_aberto = 0;
		else if (item == 7) treino_tecla(TECLA_F5);
		else muda(1);
		return 1;
	case TECLA_ESC: treino_aberto = 0; return 1;
	}
	return 1;   // menu aberto engole as outras teclas
}
