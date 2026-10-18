// ORDER SELECT em duas etapas (18/10, pedido do usuário):
//   1) "ESCOLHA O MODO": cursor visível nos 3 retratos, esquerda/direita troca ADVANCED/EXTRA/SPECIAL de cada
//      um (seleção livre, todo mundo vê), A confirma.
//   2) "ESCOLHA A ORDEM": uma tecla só, com o botão desenhado ao lado de cada retrato: A = o de cima, B = o do
//      meio, C = o de baixo (o grande); os outros seguem na ordem da escolha. Secreta: as duas colunas ficam congeladas na imagem de antes de qualquer
//      escolha até os dois jogadores terminarem.
// O EXE traduz tudo na sequência original do jogo. Mecânica do Anniversary (lab 18/10): cima/baixo giram a
// coluna (o de baixo, grande, é o candidato), A confirma o candidato na próxima posição; a caixa de modo
// (esquerda = SPECIAL, 2x esquerda = EXTRA; direita = EXTRA, 2x = SPECIAL) é POR PERSONAGEM: cada confirmação
// grava o modo da caixa naquela posição e a caixa volta pra ADVANCED; a 3ª confirmação (final) grava o modo
// do 3º. Confirmação só vale ~40 quadros depois da anterior. Candidato depois de confirmar o índice i:
// i=0 -> 1, i=1 -> 2, i=2 -> 0 (ordem cíclica); "cima" volta um.
//
// RAM: $1005C7 == 0x10 só nesta tela; $1005CE fase (5 = aceita entrada); $10A84D/$10A85E confirmações P1/P2
// (2 = ordem feita, 3 = tudo confirmado); $10A854-56/$10A865-67 ordem final (índice na escolha).
#include "ordem.h"
#include "ui.h"
#include <string.h>
#include <stdio.h>

#define RAM_TELA_ORD 0x1005C7
#define RAM_FASE_ORD 0x1005CE
#define RAM_CONT_P1 0x10A84D
#define RAM_CONT_P2 0x10A85E

int ordem_ligada = 1;
static int ativo, congelado_ok, quadros_ativo;
static u32 congelado[NEO_H][NEO_W];
static u8 antes[2];

enum { ET_MODO, ET_ORDEM, ET_SEQ, ET_PRONTO };
static int etapa[2], cursor[2], modo[2][3];     // modo por índice de escolha (0 = 1º escolhido): 0 ADV, 1 EXTRA, 2 SPECIAL
static const char *NOME_MODO[3] = {"ADVANCED", "EXTRA", "SPECIAL"};
static const u32 COR_MODO[3] = {0xffff7040u, 0xff60e060u, 0xff60a0ffu};
// posição na coluna (0 topo, 1 meio, 2 embaixo/grande) -> índice de escolha: topo = 3º escolhido, grande = 1º
static const int IDX_POS[3] = {2, 1, 0};

typedef struct { u16 q; u8 bits; } Passo;        // bits ativos em 1: 1 cima, 2 baixo, 4 esq, 8 dir, 0x10 A
#define MAXSEQ 40
static Passo seq[2][MAXSEQ];
static int seq_n[2], seq_q[2];

static u8 rd(u32 a) { return (u8)m68k_read_memory_8(a); }

static void passo(int j, int *q, u8 bits, int dura, int depois)
{
	for (int k = 0; k < dura && seq_n[j] < MAXSEQ; k++) seq[j][seq_n[j]++] = (Passo){(u16)(*q + k), bits};
	*q += dura + depois;
}

// sequência completa pra "líder = índice de escolha l" com os modos escolhidos na etapa 1
static void monta_seq(int j, int l)
{
	int q = 0;
	seq_n[j] = 0;
	int pos[3] = {l, l == 0 ? 1 : 0, l == 2 ? 1 : 2};              // ordem final: líder, depois os outros na ordem da escolha
	// 1ª confirmação: candidato inicial é o índice 0
	if (l == 1) passo(j, &q, 1, 3, 3);
	if (l == 2) passo(j, &q, 2, 3, 3);
	for (int k = 0; k < modo[j][pos[0]] ; k++) passo(j, &q, 4, 3, 3);   // esquerda: 1x SPECIAL... (2 = SPECIAL, 1 = EXTRA)
	passo(j, &q, 0x10, 3, 40);
	// 2ª: candidato padrão depois de l: l=0 -> 1, l=1 -> 2, l=2 -> 0
	int padrao = l == 0 ? 1 : l == 1 ? 2 : 0;
	if (padrao != pos[1]) passo(j, &q, 1, 3, 3);                    // "cima" volta um (só acontece com l = 1)
	for (int k = 0; k < modo[j][pos[1]]; k++) passo(j, &q, 4, 3, 3);
	passo(j, &q, 0x10, 3, 40);
	// 3ª (final): o que sobrou
	for (int k = 0; k < modo[j][pos[2]]; k++) passo(j, &q, 4, 3, 3);
	passo(j, &q, 0x10, 3, 0);
	seq_q[j] = 0;
}

// esquerda cicla ADV -> SPECIAL -> EXTRA: SPECIAL = 1 toque, EXTRA = 2 toques; na tabela modo 1 = EXTRA, 2 = SPECIAL
static int toques_modo(int m) { return m == 2 ? 1 : m == 1 ? 2 : 0; }

int ordem_ativa(void) { return ativo; }

void ordem_entradas(void)
{
	if (!ordem_ligada || rd(RAM_TELA_ORD) != 0x10) { ativo = 0; congelado_ok = 0; return; }
	if (!ativo) {
		ativo = 1; congelado_ok = 0; quadros_ativo = 0;
		antes[0] = antes[1] = 0xff;
		for (int j = 0; j < 2; j++) { etapa[j] = ET_MODO; cursor[j] = 2; seq_n[j] = 0; memset(modo[j], 0, sizeof modo[j]); }
	}
	quadros_ativo++;
	int pronta = rd(RAM_FASE_ORD) >= 5;
	if (pronta && !congelado_ok && quadros_ativo > 2) { memcpy(congelado, neo_fb, sizeof congelado); congelado_ok = 1; }
	for (int j = 0; j < 2; j++) {
		u8 *in = j ? &neo_in_p2 : &neo_in_p1;
		u8 ativos = (u8)~*in;
		u8 novo = ativos & (u8)~antes[j];
		antes[j] = ativos;
		int cont = rd(j ? RAM_CONT_P2 : RAM_CONT_P1);
		if (cont >= 3) etapa[j] = ET_PRONTO;
		*in = 0xff;                                      // nada do jogador chega ao jogo nesta tela
		switch (etapa[j]) {
		case ET_MODO:
			if (!pronta) break;
			if (novo & 1) cursor[j] = (cursor[j] + 2) % 3;
			if (novo & 2) cursor[j] = (cursor[j] + 1) % 3;
			if (novo & 4) modo[j][IDX_POS[cursor[j]]] = (modo[j][IDX_POS[cursor[j]]] + 2) % 3;
			if (novo & 8) modo[j][IDX_POS[cursor[j]]] = (modo[j][IDX_POS[cursor[j]]] + 1) % 3;
			if (novo & 0x10) etapa[j] = ET_ORDEM;
			break;
		case ET_ORDEM: {
			int l = (novo & 0x10) ? IDX_POS[0] : (novo & 0x20) ? IDX_POS[1] : (novo & 0x40) ? IDX_POS[2] : -1;   // A topo, B meio, C baixo
			if (l < 0 || !pronta) break;
			int m[3] = {modo[j][0], modo[j][1], modo[j][2]};
			for (int k = 0; k < 3; k++) modo[j][k] = toques_modo(m[k]);   // vira "toques de esquerda" pra montar
			monta_seq(j, l);
			for (int k = 0; k < 3; k++) modo[j][k] = m[k];
			etapa[j] = ET_SEQ;
		}   /* fallthrough */
		case ET_SEQ: {
			u8 b = 0;
			for (int k = 0; k < seq_n[j]; k++) if (seq[j][k].q == seq_q[j]) b |= seq[j][k].bits;
			*in = (u8)~b;
			if (++seq_q[j] > seq[j][seq_n[j] - 1].q + 2) etapa[j] = ET_PRONTO;
			break;
		}
		default: break;
		}
	}
}

static void rotulo(int x, int y, const char *s, u32 cor, int sel)
{
	int w = ui_largura(s) + 8;
	if (x >= 160) x = NEO_W - 4 - w;                  // lado do P2: encostado na borda direita, como os selos do jogo
	ui_caixa(x, y - 2, x + w, y + 9, sel ? 0xff202838u : 0xff0a0d14u, 220);
	ui_retangulo(x, y - 2, x + w, y + 9, sel ? 0xffffe060u : cor, 0);
	ui_texto(x + 4, y, s, cor);
}

// botão redondo como os da tela CONTROLS do jogo: A vermelho, B amarelo, C verde, letra branca, sombra embaixo
static void botao(u32 tela[NEO_H][NEO_W], int cx, int cy, int qual)
{
	static const u32 COR[3] = {0xffd82828u, 0xffe8b418u, 0xff28b060u}, CLARO[3] = {0xffff7070u, 0xffffe070u, 0xff70e8a0u};
	static const char *L[3] = {"A", "B", "C"};
	const int r = 10;
	for (int dy = -r - 2; dy <= r + 3; dy++)
		for (int dx = -r - 2; dx <= r + 2; dx++) {
			int x = cx + dx, y = cy + dy;
			if (x < 0 || x >= NEO_W || y < 0 || y >= NEO_H) continue;
			int d2 = dx * dx + dy * dy, d2s = dx * dx + (dy - 3) * (dy - 3);
			if (d2 <= r * r) tela[y][x] = (dx + dy < -r + 3) ? CLARO[qual] : COR[qual];     // face com brilho em cima
			else if (d2s <= r * r) tela[y][x] = 0xff303030u;                                  // sombra/lateral
			else if (d2 <= (r + 2) * (r + 2) && dy > 0) tela[y][x] = 0xff101010u;              // contorno embaixo
		}
	ui_alvo(tela);
	ui_texto(cx - 3, cy - 4, L[qual], 0xffffffffu);
}

// colunas congeladas + etapa de cada jogador, por cima do quadro
void ordem_desenha(u32 tela[NEO_H][NEO_W])
{
	if (!ativo || !congelado_ok || rd(RAM_FASE_ORD) < 5) return;
	int c1 = rd(RAM_CONT_P1), c2 = rd(RAM_CONT_P2);
	if (c1 >= 3 && c2 >= 3) return;                   // os dois terminaram: pode mostrar
	for (int y = 0; y < 184; y++) {
		memcpy(&tela[y][0], &congelado[y][0], 140 * sizeof(u32));
		memcpy(&tela[y][180], &congelado[y][180], 140 * sizeof(u32));
	}
	ui_alvo(tela);
	static const int Y_POS[3] = {28, 76, 136};        // topo, meio, grande
	for (int j = 0; j < 2; j++) {
		int x0 = j ? 180 : 0;
		int et = etapa[j];
		if (et == ET_MODO) {
			for (int p = 0; p < 3; p++) {
				int m = modo[j][IDX_POS[p]], sel = p == cursor[j];
				char s[16];
				snprintf(s, sizeof s, "%s%s", sel ? "> " : "", NOME_MODO[m]);
				rotulo(x0 + 4, Y_POS[p], s, COR_MODO[m], sel);
			}
		} else if (et == ET_ORDEM) {
			for (int p = 0; p < 3; p++) botao(tela, j ? NEO_W - 18 : 18, Y_POS[p] + 4, p);   // A topo, B meio, C baixo
		}
		ui_alvo(tela);
		const char *l1 = et == ET_MODO ? "ESCOLHA O MODO" : et == ET_ORDEM ? "ESCOLHA A ORDEM" : "PRONTO";
		const char *l2 = et == ET_MODO ? "^v quem  <> modo  A ok" : et == ET_ORDEM ? "quem começa?" : "esperando o outro";
		int w = ui_largura(l1) > ui_largura(l2) ? ui_largura(l1) : ui_largura(l2);
		ui_caixa(x0 + 70 - w / 2 - 6, 158, x0 + 70 + w / 2 + 6, 184, 0xff0a0d14u, 225);
		ui_texto(x0 + 70 - ui_largura(l1) / 2, 162, l1, et == ET_PRONTO ? 0xff60ff90u : 0xffffe060u);
		ui_texto(x0 + 70 - ui_largura(l2) / 2, 173, l2, 0xffe0e0e0u);
	}
}
