// ORDER SELECT por botão (18/10): em vez de girar a coluna com cima/baixo e confirmar com A (o adversário vê
// tudo), cada jogador aperta UMA vez: A = o 1º personagem que escolheu começa, B = o 2º, C = o 3º. Os outros
// dois seguem na ordem em que foram escolhidos. O EXE traduz o botão na sequência original de entradas
// (cima/baixo + A, 2 confirmações) e, enquanto os dois não terminam, mostra as duas colunas CONGELADAS na
// imagem de quando a tela abriu, então ninguém vê a escolha do outro. Depois da ordem, a tela original
// continua: esquerda/direita troca ADVANCED/EXTRA/SPECIAL e A confirma (3ª confirmação) — isso fica visível.
//
// RAM do Anniversary (diff de dumps 18/10): $10E4CB = tela (01 seleção, 05 ORDER SELECT, 02 luta);
// $1005CE = fase da tela (04 abrindo, 05 aceita entrada, 07 depois da 1ª confirmação, 00 ao sair);
// $10A84D/$10A85E = confirmações do P1/P2 (1, 2 = ordem feita, 3 = modo confirmado);
// $10A854-56 / $10A865-67 = ordem final (índice do personagem na escolha: 0 = 1º escolhido).
// Cima/baixo GIRAM a coluna (o de baixo, grande, é o candidato); A confirma o candidato. Confirmações só
// valem ~40 quadros depois da anterior. Sequências: A: A, A@45. B: cima, A@6, cima@48, A@54. C: baixo, A@6, A@50.
#include "ordem.h"
#include "ui.h"
#include <string.h>

#define RAM_TELA_ORD 0x1005C7   // 0x10 só na tela ORDER SELECT (verificado em Team VS e Practice, 18/10)
#define RAM_FASE_ORD 0x1005CE
#define RAM_CONT_P1 0x10A84D
#define RAM_CONT_P2 0x10A85E

int ordem_ligada = 1;
static int ativo, congelado_ok, quadros_ativo;
static u32 congelado[NEO_H][NEO_W];
static u8 antes[2];

typedef struct { u16 q; u8 bits; } Passo;        // bits ativos em 1 (bit0 cima, bit1 baixo, bit4 A)
static const Passo SEQ_A[] = {{0, 0x10}, {1, 0x10}, {2, 0x10}, {45, 0x10}, {46, 0x10}, {47, 0x10}};
static const Passo SEQ_B[] = {{0, 0x01}, {1, 0x01}, {2, 0x01}, {6, 0x10}, {7, 0x10}, {8, 0x10}, {48, 0x01}, {49, 0x01}, {50, 0x01}, {54, 0x10}, {55, 0x10}, {56, 0x10}};
static const Passo SEQ_C[] = {{0, 0x02}, {1, 0x02}, {2, 0x02}, {6, 0x10}, {7, 0x10}, {8, 0x10}, {50, 0x10}, {51, 0x10}, {52, 0x10}};
static const Passo *seq[2];
static int seq_n[2], seq_q[2];

static u8 rd(u32 a) { return (u8)m68k_read_memory_8(a); }

int ordem_ativa(void) { return ativo; }

void ordem_entradas(void)
{
	if (!ordem_ligada || rd(RAM_TELA_ORD) != 0x10) { ativo = 0; congelado_ok = 0; seq[0] = seq[1] = NULL; return; }
	if (!ativo) { ativo = 1; congelado_ok = 0; quadros_ativo = 0; antes[0] = antes[1] = 0xff; seq[0] = seq[1] = NULL; }
	quadros_ativo++;
	int pronta = rd(RAM_FASE_ORD) >= 5;
	if (pronta && !congelado_ok && quadros_ativo > 2) { memcpy(congelado, neo_fb, sizeof congelado); congelado_ok = 1; }   // imagem de antes de qualquer escolha
	for (int j = 0; j < 2; j++) {
		u8 *in = j ? &neo_in_p2 : &neo_in_p1;
		u8 ativos = (u8)~*in;
		u8 novo = ativos & (u8)~antes[j];
		antes[j] = ativos;
		int cont = rd(j ? RAM_CONT_P2 : RAM_CONT_P1);
		if (cont >= 2) continue;                       // ordem feita: tela original (modo + confirmação)
		if (seq[j]) {                                  // sequência em andamento: só ela entra
			u8 b = 0;
			for (int k = 0; k < seq_n[j]; k++) if (seq[j][k].q == seq_q[j]) b |= seq[j][k].bits;
			*in = (u8)~b;
			if (++seq_q[j] > seq[j][seq_n[j] - 1].q + 2) seq[j] = NULL;
			continue;
		}
		if (pronta && (novo & 0x10)) { seq[j] = SEQ_A; seq_n[j] = (int)(sizeof SEQ_A / sizeof SEQ_A[0]); seq_q[j] = 0; }
		else if (pronta && (novo & 0x20)) { seq[j] = SEQ_B; seq_n[j] = (int)(sizeof SEQ_B / sizeof SEQ_B[0]); seq_q[j] = 0; }
		else if (pronta && (novo & 0x40)) { seq[j] = SEQ_C; seq_n[j] = (int)(sizeof SEQ_C / sizeof SEQ_C[0]); seq_q[j] = 0; }
		*in = (u8)~(ativos & 0x0c);                    // só esquerda/direita (modo) passam; cima/baixo/botões não
		if (seq[j]) { u8 b = seq[j][0].bits; *in = (u8)~b; seq_q[j] = 1; }
	}
}

// colunas congeladas + dica, por cima do quadro (antes do treino/menus)
void ordem_desenha(u32 tela[NEO_H][NEO_W])
{
	if (!ativo || !congelado_ok || rd(RAM_FASE_ORD) < 5) return;
	int c1 = rd(RAM_CONT_P1), c2 = rd(RAM_CONT_P2);
	if (c1 >= 3 && c2 >= 3) return;                   // os dois confirmaram: pode mostrar
	for (int y = 0; y < 184; y++) {
		memcpy(&tela[y][0], &congelado[y][0], 140 * sizeof(u32));
		memcpy(&tela[y][180], &congelado[y][180], 140 * sizeof(u32));
	}
	ui_alvo(tela);
	for (int j = 0; j < 2; j++) {
		int c = j ? c2 : c1, x0 = j ? 170 : 0;
		const char *l1 = c < 2 ? "QUEM COMEÇA?" : c == 2 ? "ORDEM OK" : "PRONTO";
		const char *l2 = c < 2 ? "A=1º  B=2º  C=3º" : c == 2 ? "<> modo   A confirma" : "";
		int w = ui_largura(l1) > ui_largura(l2) ? ui_largura(l1) : ui_largura(l2);
		ui_caixa(x0 + 75 - w / 2 - 6, 150, x0 + 75 + w / 2 + 6, 178, 0xff0a0d14u, 215);
		ui_texto(x0 + 75 - ui_largura(l1) / 2, 155, l1, c < 2 ? 0xffffe060u : 0xff60ff90u);
		ui_texto(x0 + 75 - ui_largura(l2) / 2, 166, l2, 0xffe0e0e0u);
	}
}
