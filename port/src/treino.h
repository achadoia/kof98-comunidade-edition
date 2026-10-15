// Treino avançado (F6) — ver treino.c
#pragma once
#include "neo.h"

enum { TECLA_F5 = 1, TECLA_F6, TECLA_F7, TECLA_F8, TECLA_F9, TECLA_CIMA, TECLA_BAIXO, TECLA_ESQ, TECLA_DIR, TECLA_OK, TECLA_ESC };

extern int treino_aberto;                 // menu aberto = jogo pausado
void treino_entradas(void);               // depois de ler controles, antes do quadro
void treino_depois_quadro(void);
void treino_desenha(u32 tela[NEO_H][NEO_W]);
int treino_tecla(int tecla);              // 1 = tecla consumida
void treino_opcao(const char *nome, int v);  // pro modo de teste (KOF_TREINO="hitbox=1,cmd=1")
extern u32 treino_aviso_ate;
