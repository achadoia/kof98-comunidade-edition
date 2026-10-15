// Menu inicial (Comunidade Edition) por cima do jogo. Ver frontend.c.
#pragma once
#include <SDL.h>
#include "neo.h"

extern int frontend_ativo;                 // menu na tela
int frontend_carrega(const char *pasta_exe);   // 0 = sem arte (abre direto no jogo)
int frontend_tem_arte(void);
void frontend_abre(void);                  // reinicia o jogo e mostra o menu
void frontend_controles(int *rodando);     // lê teclado/controle e navega
int frontend_passos(void);                 // quadros do jogo a rodar neste giro (avanço rápido ao escolher)
int frontend_piloto(void);                 // 1 = o piloto definiu as entradas deste quadro
void frontend_desenha(SDL_Renderer *ren);
