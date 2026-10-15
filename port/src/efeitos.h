// Vozes e efeitos trocáveis (som/efeitos/<P><XX>.wav) no lugar dos sons do chip. Ver efeitos.c.
#pragma once
#include "neo.h"
#ifdef __cplusplus
extern "C" {
#endif
extern int efeitos_ligados;              // 0 = sempre o som do chip
void efeitos_init(const char *pasta_exe);
void efeitos_procura(void);              // relê a pasta
int efeitos_quantos(void);
int efeitos_tem(int cod);                // cod = (prefixo & 0xf) << 8 | parâmetro, ou o byte do efeito simples
void efeitos_toca(int canal_prefixo, int cod);   // canal_prefixo = prefixo - 0x16 (0-8), -1 = efeito simples
void efeitos_para_tudo(void);
void efeitos_para(int cod);              // comando 0x14: para esse som se estiver tocando
void efeitos_mistura(s16 *buf, int n);   // soma nas amostras (estéreo, AUDIO_TAXA)
#ifdef __cplusplus
}
#endif
