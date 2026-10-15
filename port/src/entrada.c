#include "entrada.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_CTRL 8
static SDL_Joystick *ctrl[MAX_CTRL];
static int nctrl;

static void reabre(void)
{
	for (int i = 0; i < nctrl; i++) if (ctrl[i]) SDL_JoystickClose(ctrl[i]);
	nctrl = 0;
	for (int i = 0; i < SDL_NumJoysticks() && nctrl < MAX_CTRL; i++) {
		SDL_Joystick *j = SDL_JoystickOpen(i);
		if (j) ctrl[nctrl++] = j;
	}
}

void entrada_init(void)
{
	SDL_JoystickEventState(SDL_ENABLE);
	reabre();
	if (getenv("KOF_DEBUG_ENTRADA"))
		for (int i = 0; i < nctrl; i++)
			fprintf(stderr, "controle %d: %s (%d botões)\n", i, SDL_JoystickName(ctrl[i]), SDL_JoystickNumButtons(ctrl[i]));
}

void entrada_evento(const SDL_Event *e)
{
	if (e->type == SDL_JOYDEVICEADDED || e->type == SDL_JOYDEVICEREMOVED) reabre();
}

int entrada_num_controles(void) { return nctrl; }
const char *entrada_nome_controle(int i) { return i < nctrl && ctrl[i] ? SDL_JoystickName(ctrl[i]) : "?"; }

int entrada_ligacao_ativa(const Ligacao *l)
{
	switch (l->tipo) {
	case LIG_TECLA: { const Uint8 *k = SDL_GetKeyboardState(NULL); return l->cod >= 0 && l->cod < SDL_NUM_SCANCODES && k[l->cod]; }
	case LIG_BOTAO: return l->disp < nctrl && SDL_JoystickGetButton(ctrl[l->disp], l->cod);
	case LIG_EIXO: {
		if (l->disp >= nctrl) return 0;
		int v = SDL_JoystickGetAxis(ctrl[l->disp], l->cod);
		return l->val < 0 ? v < -16000 : v > 16000;
	}
	case LIG_HAT: return l->disp < nctrl && (SDL_JoystickGetHat(ctrl[l->disp], 0) & l->val);
	default: return 0;
	}
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
