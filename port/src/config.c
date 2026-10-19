#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Config cfg;
u8 acao_ativa[2][NACOES];

const char *NOME_ACAO[NACOES] = {"Cima", "Baixo", "Esquerda", "Direita", "A  soco fraco", "B  chute fraco",
	"C  soco forte", "D  chute forte", "Start", "Moeda", "A+B  esquiva", "C+D  sopro", "A+B+C  modo MAX"};
const char *NOME_FILTRO[NFILTROS] = {"nítido", "suave", "linhas de TV", "tubo CRT", "fliperama"};
const char *NOME_PROP[NPROP] = {"4:3 da TV", "pixel inteiro", "tela toda"};
const char *NOME_CENARIO[NCENARIOS] = {"rua com carros", "rio e montanhas", "porto", "mercado árabe",
	"palácio espanhol", "ferrovia", "santuário", "festa com ônibus", "templo do chefe"};

const char *config_nome_cenario(int c) { return c < 0 || c >= NCENARIOS ? "o jogo escolhe" : NOME_CENARIO[c]; }

// O Anniversary já tem um "cenário escolhido": em 0x096264 ele usa o byte $10EBF9 (se < 0x80) como
// cenário da próxima luta e volta o byte pra 0xFF. O EXE só preenche quando o byte está livre.
#define CENARIO_PEDIDO 0x10EBF9
void config_aplica_cenario(void)
{
	if (cfg.cenario < 0 || cfg.cenario >= NCENARIOS) return;
	if (m68k_read_memory_8(CENARIO_PEDIDO) == 0xff) m68k_write_memory_8(CENARIO_PEDIDO, (unsigned)cfg.cenario);
}

static const char *CHAVE_ACAO[NACOES] = {"cima", "baixo", "esq", "dir", "a", "b", "c", "d", "start", "moeda", "ab", "cd", "abc"};

static void liga_tecla(int j, int a, int slot, SDL_Scancode s) { cfg.lig[j][a][slot] = (Ligacao){LIG_TECLA, 0, (s16)s, 0}; }
static void liga_botao(int j, int a, int slot, int d, int b) { cfg.lig[j][a][slot] = (Ligacao){LIG_BOTAO, (u8)d, (s16)b, 0}; }
static void liga_hat(int j, int a, int slot, int d, int dir) { cfg.lig[j][a][slot] = (Ligacao){LIG_HAT, (u8)d, 0, (s8)dir}; }
static void liga_eixo(int j, int a, int slot, int d, int e, int s) { cfg.lig[j][a][slot] = (Ligacao){LIG_EIXO, (u8)d, (s16)e, (s8)s}; }
static void liga_gc(int j, int a, int slot, int d, int b) { cfg.lig[j][a][slot] = (Ligacao){LIG_GCBOTAO, (u8)d, (s16)b, 0}; }
static void liga_gce(int j, int a, int slot, int d, int e, int s) { cfg.lig[j][a][slot] = (Ligacao){LIG_GCEIXO, (u8)d, (s16)e, (s8)s}; }

void config_controles_padrao(int j)
{
	memset(cfg.lig[j], 0, sizeof cfg.lig[j]);
	static const SDL_Scancode t1[NACOES] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
		SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_1, SDL_SCANCODE_5,
		SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D};
	static const SDL_Scancode t2[NACOES] = {SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
		SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_2, SDL_SCANCODE_6,
		SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9};
	for (int a = 0; a < NACOES; a++) liga_tecla(j, a, 0, (j ? t2 : t1)[a]);
	// controle reconhecido (Xbox, Switch Pro, PlayStation...): nomes fixos do SDL. Layout Xbox: X=A, Y=B, A=C, B=D,
	// LB=A+B, RB=C+D, Start, Back=moeda; direcional + analógico esquerdo. Reserva (slot 2): hat/botão cru pra stick genérico.
	liga_gc(j, AC_CIMA, 1, j, SDL_CONTROLLER_BUTTON_DPAD_UP); liga_gc(j, AC_BAIXO, 1, j, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	liga_gc(j, AC_ESQ, 1, j, SDL_CONTROLLER_BUTTON_DPAD_LEFT); liga_gc(j, AC_DIR, 1, j, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
	liga_gce(j, AC_CIMA, 2, j, SDL_CONTROLLER_AXIS_LEFTY, -1); liga_gce(j, AC_BAIXO, 2, j, SDL_CONTROLLER_AXIS_LEFTY, 1);
	liga_gce(j, AC_ESQ, 2, j, SDL_CONTROLLER_AXIS_LEFTX, -1); liga_gce(j, AC_DIR, 2, j, SDL_CONTROLLER_AXIS_LEFTX, 1);
	liga_gc(j, AC_A, 1, j, SDL_CONTROLLER_BUTTON_X); liga_gc(j, AC_B, 1, j, SDL_CONTROLLER_BUTTON_Y);
	liga_gc(j, AC_C, 1, j, SDL_CONTROLLER_BUTTON_A); liga_gc(j, AC_D, 1, j, SDL_CONTROLLER_BUTTON_B);
	liga_gc(j, AC_START, 1, j, SDL_CONTROLLER_BUTTON_START); liga_gc(j, AC_MOEDA, 1, j, SDL_CONTROLLER_BUTTON_BACK);
	liga_gc(j, AC_AB, 1, j, SDL_CONTROLLER_BUTTON_LEFTSHOULDER); liga_gc(j, AC_CD, 1, j, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
	liga_botao(j, AC_A, 2, j, 2); liga_botao(j, AC_B, 2, j, 3); liga_botao(j, AC_C, 2, j, 0); liga_botao(j, AC_D, 2, j, 1);
	liga_botao(j, AC_START, 2, j, 7); liga_botao(j, AC_MOEDA, 2, j, 6);
	liga_botao(j, AC_AB, 2, j, 4); liga_botao(j, AC_CD, 2, j, 5);
}

void config_padrao(void)
{
	memset(&cfg, 0, sizeof cfg);
	cfg.filtro = FILTRO_LINHAS;
	cfg.proporcao = PROP_4X3;
	cfg.fundo_borrado = 1;
	cfg.vsync = 0;          // o áudio comanda o ritmo; vsync derrubava pra ~40 fps e picotava o som
	cfg.volume = 80;
	cfg.musica_arquivos = 1;
	cfg.efeitos_arquivos = 1;
	cfg.cpu = 200;
	cfg.pausa_sem_foco = 1;
	cfg.inicio_rapido = 1;
	cfg.dica_treino = 1;
	cfg.cenario = -1;
	cfg.run_ahead = 1;
	cfg.ritmo_monitor = 1;
	cfg.sprites_sem_limite = 1;
	cfg.ordem_botao = 1;
	cfg.fotos = 1;
	config_controles_padrao(0);
	config_controles_padrao(1);
}

// ---------------------------------------------------------------- texto das ligações
void config_descreve(const Ligacao *l, char *buf, int tam)
{
	switch (l->tipo) {
	case LIG_TECLA: snprintf(buf, tam, "%s", SDL_GetScancodeName((SDL_Scancode)l->cod)); break;
	case LIG_BOTAO: snprintf(buf, tam, "c%d bt%d", l->disp + 1, l->cod); break;
	case LIG_EIXO: snprintf(buf, tam, "c%d eixo%d%c", l->disp + 1, l->cod, l->val < 0 ? '-' : '+'); break;
	case LIG_HAT:
		snprintf(buf, tam, "c%d %s", l->disp + 1, l->val == SDL_HAT_UP ? "cima" : l->val == SDL_HAT_DOWN ? "baixo" :
			l->val == SDL_HAT_LEFT ? "esq" : "dir");
		break;
	case LIG_GCBOTAO: snprintf(buf, tam, "c%d %s", l->disp + 1, SDL_GameControllerGetStringForButton((SDL_GameControllerButton)l->cod)); break;
	case LIG_GCEIXO: snprintf(buf, tam, "c%d %s%c", l->disp + 1, SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)l->cod), l->val < 0 ? '-' : '+'); break;
	default: snprintf(buf, tam, "-"); break;
	}
}

static void lig_txt(const Ligacao *l, char *b, int t)
{
	switch (l->tipo) {
	case LIG_TECLA: snprintf(b, t, "k%d", l->cod); break;
	case LIG_BOTAO: snprintf(b, t, "j%db%d", l->disp, l->cod); break;
	case LIG_EIXO: snprintf(b, t, "j%da%d%c", l->disp, l->cod, l->val < 0 ? '-' : '+'); break;
	case LIG_HAT: snprintf(b, t, "j%dh%d", l->disp, l->val); break;
	case LIG_GCBOTAO: snprintf(b, t, "g%db%d", l->disp, l->cod); break;
	case LIG_GCEIXO: snprintf(b, t, "g%da%d%c", l->disp, l->cod, l->val < 0 ? '-' : '+'); break;
	default: snprintf(b, t, "-"); break;
	}
}

static void txt_lig(const char *s, Ligacao *l)
{
	int d, c; char sn;
	memset(l, 0, sizeof *l);
	if (sscanf(s, "k%d", &c) == 1) *l = (Ligacao){LIG_TECLA, 0, (s16)c, 0};
	else if (sscanf(s, "j%da%d%c", &d, &c, &sn) == 3) *l = (Ligacao){LIG_EIXO, (u8)d, (s16)c, (s8)(sn == '-' ? -1 : 1)};
	else if (sscanf(s, "j%db%d", &d, &c) == 2) *l = (Ligacao){LIG_BOTAO, (u8)d, (s16)c, 0};
	else if (sscanf(s, "j%dh%d", &d, &c) == 2) *l = (Ligacao){LIG_HAT, (u8)d, 0, (s8)c};
	else if (sscanf(s, "g%da%d%c", &d, &c, &sn) == 3) *l = (Ligacao){LIG_GCEIXO, (u8)d, (s16)c, (s8)(sn == '-' ? -1 : 1)};
	else if (sscanf(s, "g%db%d", &d, &c) == 2) *l = (Ligacao){LIG_GCBOTAO, (u8)d, (s16)c, 0};
}

// ---------------------------------------------------------------- arquivo
int config_salvar(const char *arq)
{
	FILE *f = fopen(arq, "w");
	if (!f) return 1;
	fprintf(f, "# KOF98 Anniversary PC (kof98-recomp) - configurações (F1 no jogo)\n");
	fprintf(f, "filtro=%d\nproporcao=%d\nfundo_borrado=%d\ntela_cheia=%d\nsincronia_vertical=%d\nmostra_fps=%d\n",
		cfg.filtro, cfg.proporcao, cfg.fundo_borrado, cfg.tela_cheia, cfg.vsync, cfg.mostra_fps);
	fprintf(f, "volume=%d\nmudo=%d\npausa_sem_foco=%d\ninicio_rapido=%d\ndica_treino=%d\ncenario=%d\n",
		cfg.volume, cfg.mudo, cfg.pausa_sem_foco, cfg.inicio_rapido, cfg.dica_treino, cfg.cenario);
	fprintf(f, "musica_arquivos=%d\nefeitos_arquivos=%d\n", cfg.musica_arquivos, cfg.efeitos_arquivos);
	fprintf(f, "run_ahead=%d\nritmo_monitor=%d\nsprites_sem_limite=%d\nordem_botao=%d\nfotos=%d\n", cfg.run_ahead, cfg.ritmo_monitor, cfg.sprites_sem_limite, cfg.ordem_botao, cfg.fotos);
	for (int j = 0; j < 2; j++)
		for (int a = 0; a < NACOES; a++) {
			fprintf(f, "p%d.%s=", j + 1, CHAVE_ACAO[a]);
			for (int k = 0; k < NLIG; k++) {
				char b[24];
				lig_txt(&cfg.lig[j][a][k], b, sizeof b);
				fprintf(f, "%s%s", k ? "," : "", b);
			}
			fputc('\n', f);
		}
	fclose(f);
	return 0;
}

// Configs de antes de 18/10 guardavam o controle pelo joystick cru com os números do Xbox (hat + bt0-7). Quem nunca
// mexeu no controle tem exatamente esse padrão antigo: troca pelo padrão novo (game controller), que vale pra
// qualquer controle reconhecido. Quem configurou o controle à mão fica como está.
static void migra_controle(int j)
{
	Config velha;
	memcpy(&velha, &cfg, sizeof velha);
	// padrão antigo (slots 1 e 2) montado do mesmo jeito que config_controles_padrao fazia antes
	Ligacao ant[NACOES][NLIG];
	memset(ant, 0, sizeof ant);
	ant[AC_CIMA][1] = (Ligacao){LIG_HAT, (u8)j, 0, (s8)SDL_HAT_UP}; ant[AC_BAIXO][1] = (Ligacao){LIG_HAT, (u8)j, 0, (s8)SDL_HAT_DOWN};
	ant[AC_ESQ][1] = (Ligacao){LIG_HAT, (u8)j, 0, (s8)SDL_HAT_LEFT}; ant[AC_DIR][1] = (Ligacao){LIG_HAT, (u8)j, 0, (s8)SDL_HAT_RIGHT};
	ant[AC_CIMA][2] = (Ligacao){LIG_EIXO, (u8)j, 1, -1}; ant[AC_BAIXO][2] = (Ligacao){LIG_EIXO, (u8)j, 1, 1};
	ant[AC_ESQ][2] = (Ligacao){LIG_EIXO, (u8)j, 0, -1}; ant[AC_DIR][2] = (Ligacao){LIG_EIXO, (u8)j, 0, 1};
	ant[AC_A][1] = (Ligacao){LIG_BOTAO, (u8)j, 2, 0}; ant[AC_B][1] = (Ligacao){LIG_BOTAO, (u8)j, 3, 0};
	ant[AC_C][1] = (Ligacao){LIG_BOTAO, (u8)j, 0, 0}; ant[AC_D][1] = (Ligacao){LIG_BOTAO, (u8)j, 1, 0};
	ant[AC_START][1] = (Ligacao){LIG_BOTAO, (u8)j, 7, 0}; ant[AC_MOEDA][1] = (Ligacao){LIG_BOTAO, (u8)j, 6, 0};
	ant[AC_AB][1] = (Ligacao){LIG_BOTAO, (u8)j, 4, 0}; ant[AC_CD][1] = (Ligacao){LIG_BOTAO, (u8)j, 5, 0};
	for (int a = 0; a < NACOES; a++)
		for (int k = 1; k < NLIG; k++) {
			const Ligacao *l = &cfg.lig[j][a][k];
			if (l->tipo == LIG_NADA) continue;
			if (memcmp(l, &ant[a][k], sizeof *l)) return;   // mexeu no controle: não migra
		}
	Ligacao tecl[NACOES];
	for (int a = 0; a < NACOES; a++) tecl[a] = cfg.lig[j][a][0];
	config_controles_padrao(j);
	for (int a = 0; a < NACOES; a++) cfg.lig[j][a][0] = tecl[a];   // teclado do usuário fica
}

int config_carregar(const char *arq)
{
	config_padrao();
	FILE *f = fopen(arq, "r");
	if (!f) return 1;
	char linha[256];
	while (fgets(linha, sizeof linha, f)) {
		char *ig = strchr(linha, '=');
		if (!ig || linha[0] == '#') continue;
		*ig = 0;
		char *v = ig + 1;
		v[strcspn(v, "\r\n")] = 0;
		int n = atoi(v);
		if (!strcmp(linha, "filtro")) cfg.filtro = n % NFILTROS;
		else if (!strcmp(linha, "proporcao")) cfg.proporcao = n % NPROP;
		else if (!strcmp(linha, "fundo_borrado")) cfg.fundo_borrado = n;
		else if (!strcmp(linha, "tela_cheia")) cfg.tela_cheia = n;
		else if (!strcmp(linha, "sincronia_vertical")) cfg.vsync = n;
		else if (!strcmp(linha, "mostra_fps")) cfg.mostra_fps = n;
		else if (!strcmp(linha, "volume")) cfg.volume = n;
		else if (!strcmp(linha, "mudo")) cfg.mudo = n;
		else if (!strcmp(linha, "musica_arquivos")) cfg.musica_arquivos = n;
		else if (!strcmp(linha, "efeitos_arquivos")) cfg.efeitos_arquivos = n;
		else if (!strcmp(linha, "pausa_sem_foco")) cfg.pausa_sem_foco = n;
		else if (!strcmp(linha, "inicio_rapido")) cfg.inicio_rapido = n;
		else if (!strcmp(linha, "dica_treino")) cfg.dica_treino = n;
		else if (!strcmp(linha, "cenario")) cfg.cenario = n < -1 || n >= NCENARIOS ? -1 : n;
		else if (!strcmp(linha, "run_ahead")) cfg.run_ahead = n < 0 ? 0 : n > 2 ? 2 : n;
		else if (!strcmp(linha, "ritmo_monitor")) cfg.ritmo_monitor = n;
		else if (!strcmp(linha, "sprites_sem_limite")) cfg.sprites_sem_limite = n;
		else if (!strcmp(linha, "ordem_botao")) cfg.ordem_botao = n;
		else if (!strcmp(linha, "fotos")) cfg.fotos = n;
		else if (linha[0] == 'p' && (linha[1] == '1' || linha[1] == '2') && linha[2] == '.') {
			int j = linha[1] - '1';
			for (int a = 0; a < NACOES; a++) {
				if (strcmp(linha + 3, CHAVE_ACAO[a])) continue;
				char *tok = strtok(v, ",");
				for (int k = 0; k < NLIG; k++) {
					if (tok) txt_lig(tok, &cfg.lig[j][a][k]); else memset(&cfg.lig[j][a][k], 0, sizeof(Ligacao));
					tok = tok ? strtok(NULL, ",") : NULL;
				}
			}
		}
	}
	fclose(f);
	migra_controle(0);
	migra_controle(1);
	cfg.cpu = 200;   // fixo: sem a lentidão do Neo Geo
	if (cfg.volume < 0 || cfg.volume > 100) cfg.volume = 80;
	return 0;
}

// ---------------------------------------------------------------- ações -> portas do Neo Geo
void config_aplica_entradas(void)
{
	neo_in_p1 = neo_in_p2 = 0xff;
	neo_in_start = 0x0f;
	neo_in_coin = 0x07;
	neo_in_test = 1;
	for (int j = 0; j < 2; j++) {
		const u8 *A = acao_ativa[j];
		u8 b = 0;
		if (A[AC_CIMA]) b |= 1;
		if (A[AC_BAIXO]) b |= 2;
		if (A[AC_ESQ]) b |= 4;
		if (A[AC_DIR]) b |= 8;
		if (A[AC_A] || A[AC_AB] || A[AC_ABC]) b |= 0x10;
		if (A[AC_B] || A[AC_AB] || A[AC_ABC]) b |= 0x20;
		if (A[AC_C] || A[AC_CD] || A[AC_ABC]) b |= 0x40;
		if (A[AC_D] || A[AC_CD]) b |= 0x80;
		if ((b & 3) == 3) b &= (u8)~3;           // cima+baixo juntos = nenhum (SOCD neutro)
		if ((b & 12) == 12) b &= (u8)~12;
		if (j == 0) neo_in_p1 = (u8)~b; else neo_in_p2 = (u8)~b;
		if (A[AC_START]) neo_in_start &= (u8)~(j ? 4 : 1);
		if (A[AC_MOEDA]) neo_in_coin &= (u8)~(j ? 2 : 1);
	}
}
