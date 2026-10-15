// Desenho de interface por cima do quadro (texto 8x8 ASCII + Latin-1, caixas, molduras).
#pragma once
#include "neo.h"

void ui_alvo(u32 (*tela)[NEO_W]);
void ui_px(int x, int y, u32 c);
void ui_mistura(int x, int y, u32 c, int alfa);
void ui_caixa(int x0, int y0, int x1, int y1, u32 cor, int alfa);          // preenchida
void ui_retangulo(int x0, int y0, int x1, int y1, u32 cor, int alfa_fundo); // contorno (+ fundo opcional)
int ui_texto(int x, int y, const char *s, u32 cor);                         // retorna x final
int ui_largura(const char *s);
void ui_texto_centro(int y, const char *s, u32 cor);

#define UI_CIANO 0xff4dd0e0u
#define UI_FUNDO 0xff10131cu
#define UI_BRANCO 0xffffffffu
#define UI_CINZA 0xffb8c0ccu
#define UI_AMARELO 0xffffe060u
#define UI_APAGADO 0xff8890a0u
