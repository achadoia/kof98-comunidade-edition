// Desenho em alta resolução (menus F1/F6 em 1672x941 por cima do jogo): fonte 8x8 em escala, caixas, faixas, imagens.
#pragma once
#include "neo.h"

#define HD_W 1672
#define HD_H 941

typedef struct { u32 *px; int w, h; } HdImagem;   // ARGB com alfa (PNG decodificado)

void hd_init(const char *base);                     // carrega as artes (arquivos em arte/ ou embutidas no EXE)
u32 *hd_buffer(void);                               // quadro HD (HD_W x HD_H), transparente quando limpo
void hd_limpa(void);
void hd_alvo(u32 *buf, int w, int h);
void hd_mistura(int x, int y, u32 cor, int alfa);
void hd_caixa(int x0, int y0, int x1, int y1, u32 cor, int alfa);                 // preenchida (x1/y1 exclusivos)
void hd_borda(int x0, int y0, int x1, int y1, u32 cor, int esp);                  // contorno
void hd_faixa(int x0, int x1, int yc, int alt, u32 cor, int a0, int a1);          // faixa horizontal com alfa indo de a0 a a1
void hd_degrade(int x0, int y0, int x1, int y1, u32 c0, u32 c1, int alfa, int inclina);   // retângulo (ou paralelogramo) com degradê horizontal
int hd_texto(int x, int y, const char *s, u32 cor, int esc);                      // negrito com sombra; devolve x final
int hd_largura(const char *s, int esc);
int hd_texto_dir(int xdir, int y, const char *s, u32 cor, int esc);               // alinhado à direita em xdir
int hd_texto_centro(int xc, int y, const char *s, u32 cor, int esc);
void hd_seta(int x, int yc, int dir, int tam, u32 cor);                           // triângulo ◀ (-1) ▶ (1)
void hd_imagem(int x, int y, const HdImagem *im);                                 // com alfa
void hd_maiusc(char *dst, const char *src, int tam);                              // MAIÚSCULAS (UTF-8 pt-BR)

extern HdImagem hd_cfg_fundo, hd_treino_titulo, hd_dpad, hd_botao_b;
