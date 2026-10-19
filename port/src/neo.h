// KOF98 port — estado compartilhado do runtime do Neo Geo (MVS).
// Referência de hardware: driver Neo Geo do MAME (BSD-3), em ref/mame/.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- ROMs
typedef struct {
	u8 *p;      u32 p_size;     // código 68000 (p1 + p2), big-endian
	u8 *bios;                   // BIOS MVS 128KB, big-endian
	u8 *sfix;                   // camada fixa da BIOS (128KB)
	u8 *s1;     u32 s1_size;    // camada fixa do cartucho
	u8 *spr;    u32 spr_mask;   // sprites (C ROMs intercaladas), buffer potência de 2
	u8 *m1;     u32 m1_size;    // Z80 do cartucho
	u8 *sm1;                    // Z80 da BIOS (128KB)
	u8 *vrom;   u32 vrom_size;  // ADPCM (V ROMs)
	u8 *zoom;                   // 000-lo.lo (tabela de zoom vertical, 128KB)
} Roms;
extern Roms R;

// zips: lista de arquivos .zip onde procurar (jogo + pai + BIOS). Retorna 0 se ok.
int roms_load(const char *const *zips, int nzips, int aes, char *erro, size_t erro_tam);

// ---------------------------------------------------------------- máquina
#define NEO_W 320
#define NEO_H 224
#define NEO_LINHAS 264          // NEOGEO_VTOTAL
#define NEO_CICLOS_LINHA 768    // 68000 a 12MHz, 384 pixels a 6MHz por linha
#define NEO_Z80_CICLOS_LINHA 256
#define NEO_HZ 59.1856
#define AUDIO_TAXA 48000        // saída de som (filtrada e reamostrada a partir dos 55,5 kHz do YM2610)

extern u32 neo_fb[NEO_H][NEO_W];   // ARGB8888 do último quadro

// entradas (ativas em 0): bit0 cima, 1 baixo, 2 esquerda, 3 direita, 4 A, 5 B, 6 C, 7 D
extern u8 neo_in_p1, neo_in_p2;
// bit0 start P1, bit1 select P1, bit2 start P2, bit3 select P2 (ativos em 0)
extern u8 neo_in_start;
// bit0 moeda 1, bit1 moeda 2, bit2 serviço (ativos em 0)
extern u8 neo_in_coin;
extern u8 neo_in_test;             // 0 = botão de teste apertado

void neo_init(void);
void neo_reset(void);
void neo_frame(void);              // roda um quadro inteiro (264 linhas)
extern int neo_linha;              // linha atual (vpos)
extern u32 neo_quadros;
extern int neo_aes;                // 1 = console (AES, menu MODE SELECT), 0 = arcade (MVS)
extern int neo_cpu_pct;            // velocidade do 68000 em % (200 = sem lentidão)

// 68000 (callbacks do Musashi)
unsigned int m68k_read_memory_8(unsigned int a);
unsigned int m68k_read_memory_16(unsigned int a);
unsigned int m68k_read_memory_32(unsigned int a);
void m68k_write_memory_8(unsigned int a, unsigned int d);
void m68k_write_memory_16(unsigned int a, unsigned int d);
void m68k_write_memory_32(unsigned int a, unsigned int d);

// ---------------------------------------------------------------- vídeo (video.c)
void video_init(void);
int video_troca_carrega(const char *arq);   // arte/troca_tiles.bin (retratos); retorna nº de tiles
int video_troca_embutida(void);             // versão de arquivo único (KOF_SOLO)
void video_reset(void);
u16 video_reg_r(u32 off, int so_lsb);
void video_reg_w(u32 off, u16 d, u16 mask);
u16 paleta_r(u32 off);
void paleta_w(u32 off, u16 d, u16 mask);
void video_linha_inicio(int linha);   // monta a lista de sprites da linha
void video_linha_fim(int linha);      // desenha a linha (se visível)
void video_vsync(void);               // contador de animação automática
extern int video_shadow, video_banco_paleta, video_fix_cart;
extern u8 video_anim_contador;
extern int video_limite_linha;        // sprites por linha (96 = hardware)
typedef struct { int x0, y0, x1, y1, n, esp, ox, oy, rx, vx0, vy0, vx1, vy1; } OvlCaixa;   // v* = parte visível no fim do quadro   // tiles de uma foto pintados neste quadro: caixa, n tiles, esp espelhados, origem (ox,oy) / borda direita rx
extern OvlCaixa video_ovl[64][2];     // [foto][lado esquerdo/direito da tela]
extern int video_ovl_n;
int video_ovl_carrega(const char *arq);
int video_ovl_carrega_bytes(const u8 *d, size_t n);
#define MAX_SPRITES_TELA 381
// interrupção de posição (IRQ2 / raster)
extern u32 video_irq2_ctrl;
extern s32 video_irq2_ciclos;         // ciclos 68k até disparar (-1 = parado)
extern u32 video_contador;
void video_irq2_recarregar(void);

// ---------------------------------------------------------------- interrupções (neo.c)
extern int irq_vblank, irq_raster, irq_cold;
void neo_atualiza_irq(void);

// ---------------------------------------------------------------- som (audio.cpp)
void audio_init(int taxa_saida);
void audio_reset(void);
void audio_comando(u8 cmd);          // 68k escreve em 0x320000
u8 audio_resposta(void);             // 68k lê o byte alto de 0x320000
void audio_roda_linha(void);         // roda o Z80 + YM por uma linha
void audio_bios_ou_cart(int cart);   // trava do sistema Q5 (BIOS/cartucho do Z80)
// amostras estéreo intercaladas geradas desde a última chamada
int audio_pega_amostras(s16 *dst, int max_pares);

// ---------------------------------------------------------------- savestate (estado.c)
typedef struct { u8 *buf; size_t pos, cap; int salvar; int erro; } Estado;
void est_bloco(Estado *e, void *p, size_t n);
#define EST(e, v) est_bloco((e), &(v), sizeof(v))
void neo_estado(Estado *e);
void video_estado(Estado *e);
void audio_estado(Estado *e);
int estado_salvar(const char *arq);    // 0 = ok
int estado_carregar(const char *arq);
extern int estado_versao_lida;          // versão do arquivo sendo carregado
extern int estado_leve;                 // 1 = sem a trilha trocável (run-ahead: salva/restaura a cada quadro)
void estado_mem_salvar(void);           // snapshot na memória (run-ahead)
void estado_mem_carregar(void);
void audio_descarta(void);              // joga fora as amostras do chip ainda não lidas (quadros de run-ahead)
void audio_fator_ritmo(double f);       // >1 = jogo rodando mais rápido que 59,19 Hz (ritmo do monitor)


#ifdef __cplusplus
}
#endif
