// Teclado e controles -> ações da config.
// Controles conhecidos pelo SDL (Xbox, Switch Pro, PlayStation, 8BitDo...) abrem como "game controller": os
// botões têm nome fixo (A/B/X/Y, direcional, LB/RB, Start, Back) e a mesma configuração vale pra qualquer um
// deles (18/10: o usuário trocou Xbox 360 por Switch Pro e os números de botão do joystick mudaram todos).
// Controle desconhecido (arcade stick genérico) continua pelo joystick cru (botão N, hat, eixo).
#include "entrada.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_CTRL 8
static SDL_Joystick *ctrl[MAX_CTRL];
static SDL_GameController *gc[MAX_CTRL];
static int nctrl;

static void reabre(void)
{
	for (int i = 0; i < nctrl; i++) {
		if (gc[i]) SDL_GameControllerClose(gc[i]);
		else if (ctrl[i]) SDL_JoystickClose(ctrl[i]);
		gc[i] = NULL; ctrl[i] = NULL;
	}
	nctrl = 0;
	for (int i = 0; i < SDL_NumJoysticks() && nctrl < MAX_CTRL; i++) {
		if (SDL_IsGameController(i)) {
			SDL_GameController *g = SDL_GameControllerOpen(i);
			if (!g) continue;
			gc[nctrl] = g;
			ctrl[nctrl] = SDL_GameControllerGetJoystick(g);
			nctrl++;
		} else {
			SDL_Joystick *j = SDL_JoystickOpen(i);
			if (j) ctrl[nctrl++] = j;
		}
	}
}

void entrada_init(void)
{
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	reabre();
	if (getenv("KOF_DEBUG_ENTRADA"))
		for (int i = 0; i < nctrl; i++)
			fprintf(stderr, "controle %d: %s (%d botões) %s\n", i, SDL_JoystickName(ctrl[i]), SDL_JoystickNumButtons(ctrl[i]),
				gc[i] ? "reconhecido (game controller)" : "joystick cru");
}

void entrada_evento(const SDL_Event *e)
{
	if (e->type == SDL_JOYDEVICEADDED || e->type == SDL_JOYDEVICEREMOVED || e->type == SDL_CONTROLLERDEVICEADDED || e->type == SDL_CONTROLLERDEVICEREMOVED) reabre();
	if (getenv("KOF_DEBUG_ENTRADA")) {
		if (e->type == SDL_CONTROLLERBUTTONDOWN) fprintf(stderr, "gc botão %d (%s)\n", e->cbutton.button, SDL_GameControllerGetStringForButton((SDL_GameControllerButton)e->cbutton.button));
		if (e->type == SDL_JOYBUTTONDOWN) fprintf(stderr, "joy %d botão %d\n", e->jbutton.which, e->jbutton.button);
		if (e->type == SDL_JOYHATMOTION) fprintf(stderr, "joy %d hat %d\n", e->jhat.which, e->jhat.value);
	}
}

int entrada_num_controles(void) { return nctrl; }
const char *entrada_nome_controle(int i) { return i < nctrl && ctrl[i] ? SDL_JoystickName(ctrl[i]) : "?"; }
int entrada_reconhecido(int i) { return i < nctrl && gc[i] != NULL; }

// o evento de joystick cru veio de um controle reconhecido? (então vale o evento de game controller, não este)
int entrada_instancia_reconhecida(SDL_JoystickID id)
{
	for (int i = 0; i < nctrl; i++) if (gc[i] && SDL_JoystickInstanceID(ctrl[i]) == id) return 1;
	return 0;
}

// índice (0..) do controle pela instância do evento
int entrada_indice_instancia(SDL_JoystickID id)
{
	for (int i = 0; i < nctrl; i++) if (ctrl[i] && SDL_JoystickInstanceID(ctrl[i]) == id) return i;
	return 0;
}

int entrada_ligacao_ativa(const Ligacao *l)
{
	switch (l->tipo) {
	case LIG_TECLA: { const Uint8 *k = SDL_GetKeyboardState(NULL); return l->cod >= 0 && l->cod < SDL_NUM_SCANCODES && k[l->cod]; }
	case LIG_BOTAO: return l->disp < nctrl && ctrl[l->disp] && SDL_JoystickGetButton(ctrl[l->disp], l->cod);
	case LIG_EIXO: {
		if (l->disp >= nctrl || !ctrl[l->disp]) return 0;
		int v = SDL_JoystickGetAxis(ctrl[l->disp], l->cod);
		return l->val < 0 ? v < -16000 : v > 16000;
	}
	case LIG_HAT: return l->disp < nctrl && ctrl[l->disp] && (SDL_JoystickGetHat(ctrl[l->disp], 0) & l->val);
	case LIG_GCBOTAO: return l->disp < nctrl && gc[l->disp] && SDL_GameControllerGetButton(gc[l->disp], (SDL_GameControllerButton)l->cod);
	case LIG_GCEIXO: {
		if (l->disp >= nctrl || !gc[l->disp]) return 0;
		int v = SDL_GameControllerGetAxis(gc[l->disp], (SDL_GameControllerAxis)l->cod);
		return l->val < 0 ? v < -16000 : v > 16000;
	}
	default: return 0;
	}
}

// texto do que está apertado agora no controle i (diagnóstico no F1)
void entrada_estado_texto(int i, char *buf, int tam)
{
	buf[0] = 0;
	if (i >= nctrl || !ctrl[i]) { snprintf(buf, tam, "sem controle %d", i + 1); return; }
	int n = 0;
	if (gc[i]) {
		for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX && n < tam - 12; b++)
			if (SDL_GameControllerGetButton(gc[i], (SDL_GameControllerButton)b)) n += snprintf(buf + n, tam - n, "%s ", SDL_GameControllerGetStringForButton((SDL_GameControllerButton)b));
		for (int a = 0; a < 2; a++) {
			int v = SDL_GameControllerGetAxis(gc[i], (SDL_GameControllerAxis)a);
			if (v < -16000 || v > 16000) n += snprintf(buf + n, tam - n, "%s%c ", a ? "ly" : "lx", v < 0 ? '-' : '+');
		}
	} else {
		for (int b = 0; b < SDL_JoystickNumButtons(ctrl[i]) && n < tam - 8; b++)
			if (SDL_JoystickGetButton(ctrl[i], b)) n += snprintf(buf + n, tam - n, "bt%d ", b);
		if (SDL_JoystickNumHats(ctrl[i]) && SDL_JoystickGetHat(ctrl[i], 0)) n += snprintf(buf + n, tam - n, "hat%d ", SDL_JoystickGetHat(ctrl[i], 0));
	}
	if (!n) snprintf(buf, tam, "(nada apertado)");
}

void entrada_le(void)
{
	for (int j = 0; j < 2; j++)
		for (int a = 0; a < NACOES; a++) {
			u8 v = 0;
			for (int k = 0; k < NLIG; k++) if (entrada_ligacao_ativa(&cfg.lig[j][a][k])) v = 1;
			acao_ativa[j][a] = v;
		}
}
