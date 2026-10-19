// ORDER SELECT por botão (A/B/C = quem começa) com as colunas escondidas do adversário. Ver ordem.c.
#pragma once
#include "neo.h"
extern int ordem_ligada;                       // 0 = tela original
void ordem_entradas(void);                     // antes de cada quadro (depois das entradas do jogador)
void ordem_desenha(u32 tela[NEO_H][NEO_W], u32 ui[NEO_H][NEO_W]);    // colunas congeladas (tela) + interface (ui, camada transparente)
int ordem_ativa(void);
int ordem_na_tela(void);                       // a RAM diz que o ORDER SELECT está na tela (antes do módulo ligar)
int ordem_mostra_congelado(void);
extern OvlCaixa ordem_ovl[64][2];
