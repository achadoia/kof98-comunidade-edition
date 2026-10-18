// Configurações do EXE (F1): vídeo, controles, som, melhorias. Salvas em kof98.ini ao lado do EXE.
#pragma once
#include "neo.h"

enum { AC_CIMA, AC_BAIXO, AC_ESQ, AC_DIR, AC_A, AC_B, AC_C, AC_D, AC_START, AC_MOEDA, AC_AB, AC_CD, AC_ABC, NACOES };
enum { LIG_NADA, LIG_TECLA, LIG_BOTAO, LIG_EIXO, LIG_HAT, LIG_GCBOTAO, LIG_GCEIXO };   // GC = game controller (botão com nome fixo)

typedef struct { u8 tipo, disp; s16 cod; s8 val; } Ligacao;   // val: sinal do eixo / direção do hat
#define NLIG 3                                              // até 3 ligações por ação

enum { FILTRO_NITIDO, FILTRO_SUAVE, FILTRO_LINHAS, FILTRO_TUBO, FILTRO_ARCADE, NFILTROS };
enum { PROP_4X3, PROP_INTEIRA, PROP_ESTICAR, NPROP };

typedef struct {
	int filtro, proporcao, fundo_borrado, tela_cheia, vsync, mostra_fps;
	int volume, mudo;
	int musica_arquivos;         // 1 = toca som/musica/<código>.* no lugar da música do chip
	int efeitos_arquivos;        // 1 = toca som/efeitos/<código>.wav no lugar das vozes/efeitos do chip
	int cpu, pausa_sem_foco, inicio_rapido, dica_treino;
	int cenario;                 // -1 = o jogo escolhe; 0..NCENARIOS-1 = fixo
	int run_ahead;               // 0-2 quadros emulados à frente (menos atraso de resposta que o Neo Geo)
	int ritmo_monitor;           // 1 = roda a 60 Hz (ou múltiplo) com vsync: sem engasgo de 59,19 vs 60
	int sprites_sem_limite;      // 1 = ignora o limite de 96 sprites por linha (nada some em cena cheia)
	int ordem_botao;             // 1 = ORDER SELECT por A/B/C com as colunas escondidas do adversário
	Ligacao lig[2][NACOES][NLIG];
} Config;

extern Config cfg;
extern const char *NOME_ACAO[NACOES];
extern const char *NOME_FILTRO[NFILTROS];
extern const char *NOME_PROP[NPROP];
#define NCENARIOS 9
extern const char *NOME_CENARIO[NCENARIOS];
const char *config_nome_cenario(int c);
void config_aplica_cenario(void);       // antes de cada quadro

void config_padrao(void);
void config_controles_padrao(int jogador);
int config_carregar(const char *arq);
int config_salvar(const char *arq);
void config_descreve(const Ligacao *l, char *buf, int tam);

// estado das ações (lido do SDL); preenchido em main.c
extern u8 acao_ativa[2][NACOES];
void config_aplica_entradas(void);       // acao_ativa -> neo_in_p1/p2/start/coin
