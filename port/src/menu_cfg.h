// Tela de configurações (F1) — ver menu_cfg.c
#pragma once
#include <SDL.h>
#include "neo.h"

extern int cfgmenu_aberto;
int menu_cfg_evento(const SDL_Event *e);     // 1 = evento consumido
void menu_cfg_desenha_hd(void);               // no quadro HD (hd_buffer), por cima do jogo
void menu_cfg_abre(int aba);
