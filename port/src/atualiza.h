// Auto-update: ao abrir, consulta a release mais nova no GitHub; se for mais nova que KOF_VERSAO, o menu oferece
// atualizar. Aceitando, baixa o EXE novo ao lado do atual, grava um .bat que espera o jogo fechar, troca o
// arquivo e abre o jogo de novo. Ver atualiza.c.
#pragma once

enum { ATU_NADA, ATU_CHECANDO, ATU_ATUAL, ATU_NOVA, ATU_BAIXANDO, ATU_PRONTA, ATU_ERRO };
extern int atualiza_estado;
extern char atualiza_versao[32];      // "1.6.0" da release mais nova
extern char atualiza_msg[160];
extern int atualiza_adiada;           // 1 = o jogador escolheu "depois"
extern unsigned atualiza_bytes, atualiza_total;

void atualiza_checa(void);            // dispara a consulta (thread)
void atualiza_baixar(void);           // dispara o download (thread)
int atualiza_aplicar(void);           // ATU_PRONTA: grava o .bat e o executa; 1 = pode fechar o jogo
