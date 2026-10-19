// Jogo online ponto a ponto (sem servidor): sala por código, UDP direto entre os dois PCs.
// Código da sala = endereço público (descoberto por STUN) + endereço da rede local, em base32.
// Netcode por atraso fixo (lockstep): a entrada de cada quadro vale 'atraso' quadros depois, e um lado só
// avança quando já tem a entrada do outro. Anfitrião = 1P, convidado = 2P. Ver rede.c.
#pragma once
#include "neo.h"

enum { REDE_DESLIGADA, REDE_CRIANDO, REDE_ENTRANDO, REDE_PAREANDO, REDE_LIGADA, REDE_ERRO };

extern int rede_estado;
extern int rede_anfitriao;          // 1 = criei a sala (sou o 1P)
extern int rede_ativa;              // 1 = partida em andamento (lockstep)
extern int rede_atraso;             // quadros de atraso da partida
extern int rede_ping;               // ms (ida e volta), suavizado
extern int rede_dessincronizou;     // 1 = o estado dos dois jogos divergiu
extern int rede_upnp;               // 0 tentando, 1 abriu a porta, -1 não conseguiu, 2 não tentou
extern char rede_codigo[32];        // meu código (anfitrião ou convidado)
extern char rede_msg[128];          // última mensagem (erro/aviso)
extern int rede_escolha_pendente;   // anfitrião: item do menu escolhido, a enviar no próximo quadro (-1 = nada)
extern int rede_atraso_cfg;         // config: 0 = automático pelo ping, 1-10 fixo (só o anfitrião decide)
extern int rede_ordem;              // ORDER SELECT secreto na partida (anfitrião manda a sua config)
int rede_anfitriao_digita(const char *codigo_do_convidado);   // fura o NAT dos dois lados (plano B)
int rede_porta_local(void);
void rede_conta_espera(int parado);

int rede_init(void);                // Winsock; 0 = ok
void rede_fim(void);
int rede_criar(void);               // abre a porta, descobre o endereço e espera o convidado
int rede_entrar(const char *codigo);// decodifica e procura o anfitrião
void rede_desligar(void);           // encerra tudo (avisa o outro lado)
void rede_poll(void);               // recebe/envia (chamar a cada giro do laço, sempre)
const char *rede_status(void);      // texto pra tela

// rollback: fora dos menus o jogo anda prevendo a entrada do outro lado (repete a última); quando a entrada
// de verdade chega diferente, volta ao estado daquele quadro e re-simula (estado_ring_*). Até REDE_JANELA
// quadros à frente do outro; além disso espera. Dentro dos menus e do ORDER SELECT continua lockstep puro.
#define REDE_JANELA 8
extern int rede_previsao;           // 1 = previsão/rollback ligados neste momento (main decide: só na luta)
int rede_quadro_real(u32 q);        // 1 = o quadro q foi/será simulado com a entrada real do outro (não previsão)
int rede_rollback(void);            // quadro pra voltar e re-simular (-1 = nada); limpa o pedido
void rede_entradas_de(u32 q, u8 *p1, u8 *x1, u8 *p2, u8 *x2);   // entradas (reais ou previstas) do quadro q
void rede_hash_depois(u32 qn);      // soma de verificação do estado após o quadro (qn = quadro+1), também na re-simulação
void rede_checa_crc(void);          // compara as somas pendentes do outro lado (chamar depois do rollback)
extern int rede_rollbacks;          // contagem (diagnóstico)
// chat (tecla T): mensagens dos dois lados, mais recente no fim; some da tela 10 s depois
#define REDE_CHAT_N 4
typedef struct { char texto[72]; u32 t; int de_mim; } RedeChat;
extern RedeChat rede_chat[REDE_CHAT_N];
extern int rede_chat_n;
void rede_chat_envia(const char *s);

// lockstep
int rede_pode(void);                // quantos quadros seguidos já dá pra rodar (0 = esperando o outro lado)
void rede_passo(u8 meu_p, u8 meu_x, u8 *p1, u8 *x1, u8 *p2, u8 *x2);   // registra a minha entrada, devolve as dos dois
u32 rede_quadro(void);
void rede_depois_quadro(void);      // soma de verificação periódica do estado
int rede_esperando(void);           // quadros seguidos parado esperando o outro

// byte x das entradas: bit0 = start, bit1 = escolha do menu pelo anfitrião (item nos bits 4-7)
#define REDE_X_START 1
#define REDE_X_MENU 2
