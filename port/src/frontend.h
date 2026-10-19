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
void frontend_vigia(void);                 // abre o menu por cima sempre que o MODE SELECT original aparece
void frontend_desenha(SDL_Renderer *ren);
int frontend_evento(const SDL_Event *e);   // digitação do código da sala; 1 = consumiu
int frontend_voltar(void);                 // Esc: fecha tela online/sala; 0 = nada a fechar
void frontend_online_comeca(void);         // conectou: reinicia e vai pro 2P VS (os dois lados)
void frontend_online_caiu(void);
void frontend_escolhe_item(int item);      // escolha do anfitrião vinda pelos quadros da rede
void frontend_copiou(void);
