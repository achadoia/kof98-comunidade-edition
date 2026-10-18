// Trilha trocável (som/musica/<código>*.ogg|.mp3|.wav) no lugar da música do chip. Ver musica.c.
#pragma once
#include "neo.h"
#ifdef __cplusplus
extern "C" {
#endif
extern int musica_ligada;                // 0 = sempre a música do chip
void musica_init(const char *pasta_exe); // pasta do EXE (com a barra no fim)
void musica_procura(void);               // relê a pasta
int musica_quantas(void);
void musica_comando(u8 cmd);             // byte do 68k pro Z80: toca arquivo ou enfileira pro chip
int musica_fila_tira(u8 *cmd);           // próximo byte a entregar ao Z80 (0 = fila vazia)
void musica_mistura(s16 *buf, int n);    // soma a música do arquivo nas amostras (estéreo, AUDIO_TAXA)
int musica_tocando(void);                // código tocando por arquivo (-1 = nenhum)
const char *musica_arquivo(int cod);
void musica_reset(void);
void musica_estado(Estado *e);
void musica_fila_estado(Estado *e);      // só prefixo/fila pro Z80 (run-ahead)
extern int musica_silenciosa;
// Menu Comunidade Edition: a música que estiver tocando (tema de abertura) segue até o jogador escolher um modo,
// a 50%; "pare tudo"/trocas de música do jogo ficam pendentes e valem quando o menu entrega o jogo.
extern int musica_modo_menu;
void musica_menu_fim(int aplicar_pendente);   // 0 = descarta (reset do jogo vem em seguida)
extern int musica_silenciosa_dummy_;            // 1 = quadro de run-ahead: nada toca por arquivo, o Z80 recebe o mesmo
#ifdef __cplusplus
}
#endif
