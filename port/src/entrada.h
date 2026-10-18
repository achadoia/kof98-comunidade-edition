// Teclado e controles (SDL Joystick: XInput, DirectInput, arcade sticks) -> ações da config.
#pragma once
#include <SDL.h>
#include "config.h"

void entrada_init(void);
void entrada_evento(const SDL_Event *e);      // conexão/desconexão de controles
void entrada_le(void);                         // preenche acao_ativa[][] pela config
int entrada_ligacao_ativa(const Ligacao *l);
int entrada_num_controles(void);
const char *entrada_nome_controle(int i);
int entrada_reconhecido(int i);                        // 1 = game controller (botões com nome fixo)
int entrada_instancia_reconhecida(SDL_JoystickID id);  // evento de joystick cru de um controle reconhecido: ignorar
int entrada_indice_instancia(SDL_JoystickID id);
void entrada_estado_texto(int i, char *buf, int tam);  // o que está apertado agora (diagnóstico)
