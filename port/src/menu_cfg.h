// Tela de configurações (F1) — ver menu_cfg.c
#pragma once
#include <SDL.h>
#include "neo.h"

extern int cfgmenu_aberto;
int menu_cfg_evento(const SDL_Event *e);     // 1 = evento consumido
void menu_cfg_desenha(u32 tela[NEO_H][NEO_W]);
void menu_cfg_abre(int aba);
