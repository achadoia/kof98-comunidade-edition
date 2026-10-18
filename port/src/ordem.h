// ORDER SELECT por botão (A/B/C = quem começa) com as colunas escondidas do adversário. Ver ordem.c.
#pragma once
#include "neo.h"
extern int ordem_ligada;                       // 0 = tela original
void ordem_entradas(void);                     // antes de cada quadro (depois das entradas do jogador)
void ordem_desenha(u32 tela[NEO_H][NEO_W]);    // colunas congeladas + dica
int ordem_ativa(void);
