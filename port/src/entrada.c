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

// Banco de mapeamentos da comunidade (SDL_GameControllerDB, só Windows): controle que o SDL não conhece de
// fábrica passa a ser reconhecido com os botões de nome fixo. Arquivo controles/gamecontrollerdb.txt ao lado
// do EXE (build normal) ou embutido (EXE único); o usuário ainda pode pôr um gamecontrollerdb.txt ao lado.
#ifdef KOF_SOLO
extern const unsigned char embutido_gcdb[]; extern const size_t embutido_gcdb_tam;
#endif
static void carrega_mapeamentos(void)
{
	int n = 0;
#ifdef KOF_SOLO
	SDL_RWops *rw = SDL_RWFromConstMem(embutido_gcdb, (int)embutido_gcdb_tam);
	if (rw) n += SDL_GameControllerAddMappingsFromRW(rw, 1);
#endif
	char *bp = SDL_GetBasePath(); char arq[700];
	snprintf(arq, sizeof arq, "%scontroles/gamecontrollerdb.txt", bp ? bp : "");
	int a = SDL_GameControllerAddMappingsFromFile(arq); if (a > 0) n += a;
	snprintf(arq, sizeof arq, "%sgamecontrollerdb.txt", bp ? bp : "");
	a = SDL_GameControllerAddMappingsFromFile(arq); if (a > 0) n += a;
	SDL_free(bp);
	if (getenv("KOF_DEBUG_ENTRADA")) fprintf(stderr, "mapeamentos de controle carregados: %d\n", n);
}

void entrada_init(void)
{
	carrega_mapeamentos();
	if (getenv("KOF_SEM_CONTROLE")) return;   // teste: ignora os controles ligados (um controle com botão preso fecha o menu da foto)
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	reabre();
	if (getenv("KOF_DEBUG_ENTRADA"))
		for (int i = 0; i < nctrl; i++)
			{
			char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(ctrl[i]), guid, sizeof guid);
			fprintf(stderr, "controle %d: %s (%d botões, %d eixos, %d hats) guid %s %s\n", i, SDL_JoystickName(ctrl[i]), SDL_JoystickNumButtons(ctrl[i]),
				SDL_JoystickNumAxes(ctrl[i]), SDL_JoystickNumHats(ctrl[i]), guid, gc[i] ? "reconhecido (game controller)" : "joystick cru");
		}
}

void entrada_evento(const SDL_Event *e)
{
	if ((e->type == SDL_JOYDEVICEADDED || e->type == SDL_JOYDEVICEREMOVED || e->type == SDL_CONTROLLERDEVICEADDED || e->type == SDL_CONTROLLERDEVICEREMOVED) && !getenv("KOF_SEM_CONTROLE")) reabre();
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

int entrada_teclado_bloqueado;
int entrada_ligacao_ativa(const Ligacao *l)
{
	switch (l->tipo) {
	case LIG_TECLA: { if (entrada_teclado_bloqueado) return 0; const Uint8 *k = SDL_GetKeyboardState(NULL); return l->cod >= 0 && l->cod < SDL_NUM_SCANCODES && k[l->cod]; }
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

// ---------------------------------------------------------------- eventos -> comando de menu (F1/F6)
static int acao_para_menu(int a)
{
	switch (a) {
	case AC_CIMA: return MENU_CIMA; case AC_BAIXO: return MENU_BAIXO;
	case AC_ESQ: return MENU_ESQ; case AC_DIR: return MENU_DIR;
	case AC_A: case AC_START: return MENU_OK;
	default: return MENU_NADA;
	}
}

// a ligação l corresponde a este evento? (eixos: só a borda de entrada, guardada por controle/eixo)
static int liga_bate(const Ligacao *l, const SDL_Event *e, int disp, int novo_eixo)
{
	switch (e->type) {
	case SDL_KEYDOWN: return l->tipo == LIG_TECLA && l->cod == e->key.keysym.scancode;
	case SDL_CONTROLLERBUTTONDOWN: return l->tipo == LIG_GCBOTAO && l->disp == disp && l->cod == e->cbutton.button;
	case SDL_JOYBUTTONDOWN: return l->tipo == LIG_BOTAO && l->disp == disp && l->cod == e->jbutton.button;
	case SDL_JOYHATMOTION: return l->tipo == LIG_HAT && l->disp == disp && (e->jhat.value & l->val) && !(e->jhat.value & (e->jhat.value - 1));
	case SDL_CONTROLLERAXISMOTION: return novo_eixo && l->tipo == LIG_GCEIXO && l->disp == disp && l->cod == e->caxis.axis && l->val == novo_eixo;
	case SDL_JOYAXISMOTION: return novo_eixo && l->tipo == LIG_EIXO && l->disp == disp && l->cod == e->jaxis.axis && l->val == novo_eixo;
	default: return 0;
	}
}

int entrada_evento_menu(const SDL_Event *e)
{
	static s8 eixo_gc[MAX_CTRL][8], eixo_j[MAX_CTRL][8];
	int disp = 0, novo = 0;
	if (e->type == SDL_KEYDOWN) {
		if (e->key.repeat) return MENU_NADA;
		switch (e->key.keysym.scancode) {
		case SDL_SCANCODE_UP: return MENU_CIMA; case SDL_SCANCODE_DOWN: return MENU_BAIXO;
		case SDL_SCANCODE_LEFT: return MENU_ESQ; case SDL_SCANCODE_RIGHT: return MENU_DIR;
		case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_Z: return MENU_OK;
		case SDL_SCANCODE_ESCAPE: return MENU_VOLTAR;
		case SDL_SCANCODE_TAB: return (e->key.keysym.mod & KMOD_SHIFT) ? MENU_ABA_ANT : MENU_ABA_SEG;
		default: break;
		}
	} else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
		disp = entrada_indice_instancia(e->cbutton.which);
		switch (e->cbutton.button) {      // botões fixos do controle reconhecido (os ícones do rodapé)
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return MENU_CIMA; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return MENU_BAIXO;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return MENU_ESQ; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return MENU_DIR;
		case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: return MENU_OK;
		case SDL_CONTROLLER_BUTTON_B: return MENU_VOLTAR;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return MENU_ABA_ANT;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return MENU_ABA_SEG;
		default: break;
		}
	} else if (e->type == SDL_CONTROLLERAXISMOTION) {
		disp = entrada_indice_instancia(e->caxis.which);
		int ax = e->caxis.axis; if (ax < 0 || ax >= 8) return MENU_NADA;
		int v = e->caxis.value > 16000 ? 1 : e->caxis.value < -16000 ? -1 : 0;
		if (v == eixo_gc[disp][ax]) return MENU_NADA;
		eixo_gc[disp][ax] = (s8)v;
		if (!v) return MENU_NADA;
		novo = v;
		if (ax == SDL_CONTROLLER_AXIS_LEFTX) return v < 0 ? MENU_ESQ : MENU_DIR;   // analógico esquerdo navega
		if (ax == SDL_CONTROLLER_AXIS_LEFTY) return v < 0 ? MENU_CIMA : MENU_BAIXO;
	} else if (e->type == SDL_JOYBUTTONDOWN) {
		if (entrada_instancia_reconhecida(e->jbutton.which)) return MENU_NADA;
		disp = entrada_indice_instancia(e->jbutton.which);
	} else if (e->type == SDL_JOYHATMOTION) {
		if (entrada_instancia_reconhecida(e->jhat.which) || !e->jhat.value) return MENU_NADA;
		disp = entrada_indice_instancia(e->jhat.which);
	} else if (e->type == SDL_JOYAXISMOTION) {
		if (entrada_instancia_reconhecida(e->jaxis.which)) return MENU_NADA;
		disp = entrada_indice_instancia(e->jaxis.which);
		int ax = e->jaxis.axis; if (ax < 0 || ax >= 8) return MENU_NADA;
		int v = e->jaxis.value > 16000 ? 1 : e->jaxis.value < -16000 ? -1 : 0;
		if (v == eixo_j[disp][ax]) return MENU_NADA;
		eixo_j[disp][ax] = (s8)v;
		if (!v) return MENU_NADA;
		novo = v;
	} else return MENU_NADA;
	// teclas/botões configurados no F1 (qualquer jogador): direcional navega, A/Start confirma
	for (int j = 0; j < 2; j++)
		for (int a = 0; a < NACOES; a++) {
			int m = acao_para_menu(a);
			if (!m) continue;
			for (int k = 0; k < NLIG; k++) if (liga_bate(&cfg.lig[j][a][k], e, disp, novo)) return m;
		}
	if (e->type == SDL_JOYHATMOTION) {      // stick genérico sem configuração: hat navega
		int v = e->jhat.value;
		return v == SDL_HAT_UP ? MENU_CIMA : v == SDL_HAT_DOWN ? MENU_BAIXO : v == SDL_HAT_LEFT ? MENU_ESQ : v == SDL_HAT_RIGHT ? MENU_DIR : MENU_NADA;
	}
	if (e->type == SDL_JOYBUTTONDOWN) {
		int b = e->jbutton.button;
		return b == 0 ? MENU_OK : b == 1 ? MENU_VOLTAR : b == 4 ? MENU_ABA_ANT : b == 5 ? MENU_ABA_SEG : MENU_NADA;
	}
	return MENU_NADA;
}
