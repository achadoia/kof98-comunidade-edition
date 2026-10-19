// Tela de configurações (F1): abas Vídeo, Controles, Som e Melhorias. Desenhada por cima do jogo pausado.
#include "menu_cfg.h"
#include "config.h"
#include "entrada.h"
#include "ui.h"
#include "musica.h"
#include "efeitos.h"
#include <stdio.h>
#include <string.h>

int cfgmenu_aberto;
static int aba, linha, jogador;
static int capturando = -1;      // ação esperando tecla/botão (-1 = nenhuma)
static int assistente;           // capturando todas em sequência
static char aviso_txt[80];
static u32 aviso_t;
extern char cfg_arquivo[512];

static const char *ABAS[] = {"Vídeo", "Controles", "Som", "Melhorias"};
#define NABAS 4

static const char *liga(int v) { return v ? "ligado" : "desligado"; }

// linhas de cada aba ------------------------------------------------------------
static int linhas_aba(void)
{
	switch (aba) {
	case 0: return 9;
	case 1: return 3 + NACOES;     // jogador, assistente, ações..., restaurar
	case 2: return 5;
	default: return 6;
	}
}

static void aviso(const char *s) { snprintf(aviso_txt, sizeof aviso_txt, "%s", s); aviso_t = SDL_GetTicks() + 2500; }

static void salva(void) { config_salvar(cfg_arquivo); }

static void muda(int d)
{
	switch (aba) {
	case 0:
		switch (linha) {
		case 0: cfg.filtro = (cfg.filtro + d + NFILTROS) % NFILTROS; break;
		case 1: cfg.proporcao = (cfg.proporcao + d + NPROP) % NPROP; break;
		case 2: cfg.fundo_borrado = !cfg.fundo_borrado; break;
		case 3: cfg.tela_cheia = !cfg.tela_cheia; break;
		case 4: cfg.vsync = !cfg.vsync; break;
		case 5: cfg.mostra_fps = !cfg.mostra_fps; break;
		case 6: cfg.ritmo_monitor = !cfg.ritmo_monitor; break;
		case 7: cfg.sprites_sem_limite = !cfg.sprites_sem_limite; break;
		case 8: cfg.fotos = !cfg.fotos; break;
		}
		break;
	case 1:
		if (linha == 0) jogador = !jogador;
		break;
	case 2:
		if (linha == 0) { cfg.volume += d * 10; if (cfg.volume < 0) cfg.volume = 0; if (cfg.volume > 100) cfg.volume = 100; }
		else if (linha == 1) cfg.mudo = !cfg.mudo;
		else if (linha == 2) cfg.musica_arquivos = !cfg.musica_arquivos;
		else if (linha == 3) cfg.efeitos_arquivos = !cfg.efeitos_arquivos;
		else {
			char b[64];
			musica_procura();
			efeitos_procura();
			snprintf(b, sizeof b, "%d músicas, %d vozes/efeitos", musica_quantas(), efeitos_quantos());
			aviso(b);
		}
		break;
	default:
		switch (linha) {
		case 0: cfg.cenario += d; if (cfg.cenario < -1) cfg.cenario = NCENARIOS - 1; if (cfg.cenario >= NCENARIOS) cfg.cenario = -1; break;
		case 1: cfg.pausa_sem_foco = !cfg.pausa_sem_foco; break;
		case 2: cfg.inicio_rapido = !cfg.inicio_rapido; break;
		case 3: cfg.dica_treino = !cfg.dica_treino; break;
		case 4: cfg.run_ahead = (cfg.run_ahead + d + 3) % 3; break;
		case 5: cfg.ordem_botao = !cfg.ordem_botao; break;
		}
	}
	salva();
}

static void confirma(void)
{
	if (aba == 1) {
		if (linha == 1) { assistente = 1; capturando = 0; return; }
		if (linha >= 2 && linha < 2 + NACOES) { assistente = 0; capturando = linha - 2; return; }
		if (linha == 2 + NACOES) { config_controles_padrao(jogador); salva(); aviso("controles padrão restaurados"); return; }
	}
	muda(1);
}

// guarda a ligação capturada: teclado no slot 0, controle no 1 (ou 2 se o 1 já for de outro tipo)
static void grava_ligacao(Ligacao l)
{
	Ligacao *s = cfg.lig[jogador][capturando];
	if (l.tipo == LIG_TECLA) s[0] = l;
	else if (s[1].tipo == LIG_NADA || s[1].tipo == l.tipo || s[2].tipo != LIG_NADA) { s[1] = l; if (l.tipo == LIG_HAT) memset(&s[2], 0, sizeof(Ligacao)); }
	else s[2] = l;
	salva();
	if (assistente && capturando + 1 < NACOES) capturando++;
	else { capturando = -1; if (assistente) aviso("controles gravados!"); assistente = 0; }
}

static void pula_captura(void)
{
	if (assistente && capturando + 1 < NACOES) capturando++;
	else { capturando = -1; assistente = 0; }
}

int menu_cfg_evento(const SDL_Event *e)
{
	if (e->type == SDL_KEYDOWN && !e->key.repeat && e->key.keysym.scancode == SDL_SCANCODE_F1) {
		cfgmenu_aberto = !cfgmenu_aberto;
		capturando = -1; assistente = 0;
		if (!cfgmenu_aberto) salva();
		return 1;
	}
	if (!cfgmenu_aberto) return 0;

	if (capturando >= 0) {
		if (e->type == SDL_KEYDOWN && !e->key.repeat) {
			SDL_Scancode s = e->key.keysym.scancode;
			if (s == SDL_SCANCODE_ESCAPE) { pula_captura(); return 1; }
			if (s == SDL_SCANCODE_DELETE || s == SDL_SCANCODE_BACKSPACE) {
				memset(cfg.lig[jogador][capturando], 0, sizeof(cfg.lig[0][0]));
				salva();
				pula_captura();
				return 1;
			}
			grava_ligacao((Ligacao){LIG_TECLA, 0, (s16)s, 0});
		} else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
			grava_ligacao((Ligacao){LIG_GCBOTAO, (u8)entrada_indice_instancia(e->cbutton.which), e->cbutton.button, 0});
		} else if (e->type == SDL_CONTROLLERAXISMOTION && (e->caxis.value > 24000 || e->caxis.value < -24000) && e->caxis.axis <= SDL_CONTROLLER_AXIS_RIGHTY) {
			grava_ligacao((Ligacao){LIG_GCEIXO, (u8)entrada_indice_instancia(e->caxis.which), e->caxis.axis, (s8)(e->caxis.value < 0 ? -1 : 1)});
		} else if (e->type == SDL_JOYBUTTONDOWN && !entrada_instancia_reconhecida(e->jbutton.which)) {
			grava_ligacao((Ligacao){LIG_BOTAO, (u8)entrada_indice_instancia(e->jbutton.which), e->jbutton.button, 0});
		} else if (e->type == SDL_JOYHATMOTION && !entrada_instancia_reconhecida(e->jhat.which) && e->jhat.value && !(e->jhat.value & (e->jhat.value - 1))) {
			grava_ligacao((Ligacao){LIG_HAT, (u8)entrada_indice_instancia(e->jhat.which), 0, (s8)e->jhat.value});
		} else if (e->type == SDL_JOYAXISMOTION && !entrada_instancia_reconhecida(e->jaxis.which) && (e->jaxis.value > 24000 || e->jaxis.value < -24000)) {
			grava_ligacao((Ligacao){LIG_EIXO, (u8)entrada_indice_instancia(e->jaxis.which), e->jaxis.axis, (s8)(e->jaxis.value < 0 ? -1 : 1)});
		}
		return 1;
	}

	int t = 0;     // 1 cima 2 baixo 3 esq 4 dir 5 ok 6 voltar
	if (e->type == SDL_KEYDOWN) {
		switch (e->key.keysym.scancode) {
		case SDL_SCANCODE_UP: t = 1; break; case SDL_SCANCODE_DOWN: t = 2; break;
		case SDL_SCANCODE_LEFT: t = 3; break; case SDL_SCANCODE_RIGHT: t = 4; break;
		case SDL_SCANCODE_RETURN: case SDL_SCANCODE_Z: t = 5; break;
		case SDL_SCANCODE_ESCAPE: t = 6; break;
		case SDL_SCANCODE_TAB: aba = (aba + 1) % NABAS; linha = 0; return 1;
		default: break;
		}
	} else if (e->type == SDL_JOYHATMOTION && !entrada_instancia_reconhecida(e->jhat.which)) {
		int v = e->jhat.value;
		t = v == SDL_HAT_UP ? 1 : v == SDL_HAT_DOWN ? 2 : v == SDL_HAT_LEFT ? 3 : v == SDL_HAT_RIGHT ? 4 : 0;
	} else if (e->type == SDL_JOYBUTTONDOWN && !entrada_instancia_reconhecida(e->jbutton.which)) {
		int b = e->jbutton.button;
		t = b == 0 ? 5 : b == 1 ? 6 : 0;
		if (b == 4) { aba = (aba + NABAS - 1) % NABAS; linha = 0; return 1; }
		if (b == 5) { aba = (aba + 1) % NABAS; linha = 0; return 1; }
	} else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
		int b = e->cbutton.button;
		t = b == SDL_CONTROLLER_BUTTON_DPAD_UP ? 1 : b == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? 2 : b == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? 3 :
		    b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 4 : b == SDL_CONTROLLER_BUTTON_A ? 5 : b == SDL_CONTROLLER_BUTTON_B ? 6 : 0;
		if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) { aba = (aba + NABAS - 1) % NABAS; linha = 0; return 1; }
		if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) { aba = (aba + 1) % NABAS; linha = 0; return 1; }
	}
	int n = linhas_aba();
	switch (t) {
	case 1: linha = linha <= -1 ? -1 : linha - 1; break;                 // -1 = barra de abas
	case 2: linha = linha + 1 >= n ? n - 1 : linha + 1; break;
	case 3: if (linha < 0) aba = (aba + NABAS - 1) % NABAS; else muda(-1); break;
	case 4: if (linha < 0) aba = (aba + 1) % NABAS; else muda(1); break;
	case 5: if (linha < 0) linha = 0; else confirma(); break;
	case 6: cfgmenu_aberto = 0; salva(); break;
	}
	return 1;
}

// desenho -----------------------------------------------------------------------
static void item(int y, int i, const char *nome, const char *valor, u32 cor_nome)
{
	int sel = i == linha;
	if (sel) ui_caixa(14, y - 2, NEO_W - 15, y + 8, UI_CIANO, 60);
	ui_texto(20, y, nome, sel ? UI_BRANCO : cor_nome);
	if (valor) {
		char b[48];
		snprintf(b, sizeof b, sel ? "< %s >" : "%s", valor);
		ui_texto(NEO_W - 20 - ui_largura(b), y, b, sel ? UI_AMARELO : 0xffe0e0e0u);
	}
}

void menu_cfg_desenha(u32 tela[NEO_H][NEO_W])
{
	if (!cfgmenu_aberto) return;
	ui_alvo(tela);
	ui_caixa(0, 0, NEO_W - 1, NEO_H - 1, 0xff05070cu, 228);
	ui_texto(10, 6, "CONFIGURAÇÕES", UI_CIANO);
	ui_texto(NEO_W - 10 - ui_largura("F1 fecha"), 6, "F1 fecha", UI_APAGADO);

	// abas como pílulas
	int x = 10;
	for (int i = 0; i < NABAS; i++) {
		int w = ui_largura(ABAS[i]) + 12;
		int ativa = i == aba;
		if (ativa) ui_caixa(x, 19, x + w, 31, UI_CIANO, linha < 0 ? 255 : 170);
		else ui_retangulo(x, 19, x + w, 31, 0xff3a4250u, 0);
		ui_texto(x + 6, 22, ABAS[i], ativa ? 0xff0a0d14u : UI_CINZA);
		x += w + 6;
	}

	char v[48];
	int y = 40;
	switch (aba) {
	case 0:
		item(y, 0, "Filtro de imagem", NOME_FILTRO[cfg.filtro], UI_CINZA); y += 14;
		item(y, 1, "Proporção", NOME_PROP[cfg.proporcao], UI_CINZA); y += 14;
		item(y, 2, "Laterais borradas", liga(cfg.fundo_borrado), UI_CINZA); y += 14;
		item(y, 3, "Tela cheia  (F11)", liga(cfg.tela_cheia), UI_CINZA); y += 14;
		item(y, 4, "Sincronia vertical (vsync)", liga(cfg.vsync), UI_CINZA); y += 14;
		item(y, 5, "Mostrar FPS", liga(cfg.mostra_fps), UI_CINZA); y += 14;
		{ extern int ritmo_monitor_ativo; extern double ritmo_monitor_hz;
		  if (!cfg.ritmo_monitor) snprintf(v, sizeof v, "desligado (59,19 Hz)");
		  else if (ritmo_monitor_ativo) snprintf(v, sizeof v, "%.0f Hz do monitor", ritmo_monitor_hz);
		  else snprintf(v, sizeof v, "ligado (monitor incompatível)"); }
		item(y, 6, "Ritmo do monitor", v, UI_CINZA); y += 14;
		item(y, 7, "Sprites sem limite por linha", liga(cfg.sprites_sem_limite), UI_CINZA); y += 14;
		item(y, 8, "Fotos dos homenageados em HD", liga(cfg.fotos), UI_CINZA); y += 14;
		ui_texto(20, y + 6, "F4 troca o filtro durante o jogo.", UI_APAGADO);
		ui_texto(20, y + 18, "Ritmo do monitor: o jogo roda a 60 Hz", UI_APAGADO);
		ui_texto(20, y + 28, "(1,4% mais rápido) casado com o vsync,", UI_APAGADO);
		ui_texto(20, y + 38, "sem o engasgo de 59,19 Hz na tela de 60.", UI_APAGADO);
		break;
	case 1: {
		snprintf(v, sizeof v, "jogador %d  (%d ctrl)", jogador + 1, entrada_num_controles());
		item(y, 0, "Editando", v, UI_CINZA); y += 12;
		item(y, 1, "Configurar tudo (passo a passo)", NULL, UI_CIANO); y += 12;
		for (int a = 0; a < NACOES; a++) {
			int i = 2 + a, sel = i == linha;
			if (sel) ui_caixa(14, y - 2, NEO_W - 15, y + 8, UI_CIANO, 60);
			int ativa = acao_ativa[jogador][a];
			if (ativa) ui_caixa(16, y - 1, 18, y + 7, 0xff40ff70u, 255);
			ui_texto(22, y, NOME_ACAO[a], capturando == a ? UI_AMARELO : ativa ? 0xff60ff90u : sel ? UI_BRANCO : UI_CINZA);
			char d1[32] = "", d2[32] = "", tudo[64];
			const Ligacao *L = cfg.lig[jogador][a];
			config_descreve(&L[0], d1, sizeof d1);
			config_descreve(L[1].tipo ? &L[1] : &L[2], d2, sizeof d2);
			if (capturando == a) snprintf(tudo, sizeof tudo, "aperte...");
			else snprintf(tudo, sizeof tudo, "%s | %s", d1, d2);
			if ((int)strlen(tudo) > 20) tudo[20] = 0;
			ui_texto(NEO_W - 18 - ui_largura(tudo), y, tudo, capturando == a ? UI_AMARELO : 0xffd0d6e0u);
			y += 10;
		}
		item(y + 2, 2 + NACOES, "Restaurar padrão", NULL, UI_CINZA);
		break;
	}
	case 2:
		snprintf(v, sizeof v, "%d%%", cfg.volume);
		item(y, 0, "Volume", v, UI_CINZA); y += 14;
		item(y, 1, "Mudo", liga(cfg.mudo), UI_CINZA); y += 14;
		item(y, 2, "Músicas da pasta som/musica", liga(cfg.musica_arquivos), UI_CINZA); y += 14;
		item(y, 3, "Vozes/efeitos de som/efeitos", liga(cfg.efeitos_arquivos), UI_CINZA); y += 14;
		snprintf(v, sizeof v, "%d + %d arquivos", musica_quantas(), efeitos_quantos());
		item(y, 4, "Reler as pastas", v, UI_CINZA); y += 14;
		for (int i = 0; i < 10; i++)
			ui_caixa(20 + i * 14, y + 4, 30 + i * 14, y + 14, i * 10 < cfg.volume && !cfg.mudo ? UI_CIANO : 0xff2a303cu, 255);
		y += 24;
		{   // qual música está tocando: ajuda a saber que arquivo trocar
			extern int musica_ultima_cod;
			int c = musica_ultima_cod;
			const char *arq = c >= 0 ? musica_arquivo(c == 0x4b ? 0x21 : c) : NULL;
			if (c < 0) snprintf(v, sizeof v, "Tocando: nenhuma");
			else if (musica_tocando() >= 0 && arq) {
				const char *n = strrchr(arq, '/');
				snprintf(v, sizeof v, "Tocando: %02X  %s", c, n ? n + 1 : arq);
			} else snprintf(v, sizeof v, "Tocando: %02X  (chip original)", c);
			if ((int)strlen(v) > 46) v[46] = 0;
			ui_texto(20, y, v, UI_AMARELO);
		}
		ui_texto(20, y + 14, "Pra trocar: .ogg .mp3 ou .wav em", UI_APAGADO);
		ui_texto(20, y + 24, "som/musica, com o nome começando", UI_APAGADO);
		ui_texto(20, y + 34, "pelo código (ex.: \"28 rock.mp3\").", UI_APAGADO);
		break;
	default:
		item(y, 0, "Cenário", config_nome_cenario(cfg.cenario), UI_CINZA); y += 14;
		item(y, 1, "Pausar sem foco", liga(cfg.pausa_sem_foco), UI_CINZA); y += 14;
		item(y, 2, "Início rápido (pula o logo)", liga(cfg.inicio_rapido), UI_CINZA); y += 14;
		item(y, 3, "Dica do F6 ao iniciar", liga(cfg.dica_treino), UI_CINZA); y += 14;
		snprintf(v, sizeof v, cfg.run_ahead ? "%d quadro%s" : "desligado", cfg.run_ahead, cfg.run_ahead > 1 ? "s" : "");
		item(y, 4, "Run-ahead (resposta mais rápida)", v, UI_CINZA); y += 14;
		item(y, 5, "Ordem do trio por botão (A/B/C)", liga(cfg.ordem_botao), UI_CINZA); y += 14;
		ui_texto(20, y + 4, "Run-ahead: emula à frente e volta; tira", UI_APAGADO);
		ui_texto(20, y + 14, "1-2 quadros do atraso interno do jogo.", UI_APAGADO);
		ui_texto(20, y + 26, "Ordem por botão: no ORDER SELECT, A/B/C", UI_APAGADO);
		ui_texto(20, y + 36, "= 1º/2º/3º escolhido começa; o adversário", UI_APAGADO);
		ui_texto(20, y + 46, "não vê. <> muda ADV/EXTRA/SPECIAL, A ok.", UI_APAGADO);
		break;
	}

	// rodapé
	char lin[96];
	if (aba == 1 && capturando < 0) {   // o que o controle está mandando agora: mostra na hora se ele é reconhecido e se os botões chegam
		char est[64];
		entrada_estado_texto(jogador, est, sizeof est);
		snprintf(lin, sizeof lin, "c%d %s: %s", jogador + 1, entrada_num_controles() > jogador ? (entrada_reconhecido(jogador) ? "ok" : "cru") : "-", est);
		if ((int)strlen(lin) > 44) lin[44] = 0;
	}
	const char *dica = capturando >= 0 ? "Esc pula   Del apaga" : aba == 1 ? lin : "^v escolhe   <> muda   Tab muda aba";
	ui_caixa(0, NEO_H - 14, NEO_W - 1, NEO_H - 1, 0xff0a0d14u, 230);
	ui_texto_centro(NEO_H - 11, dica, UI_APAGADO);
	if (capturando >= 0) {
		char b[80];
		snprintf(b, sizeof b, "P%d: aperte para  %s", jogador + 1, NOME_ACAO[capturando]);
		int w = ui_largura(b) + 20;
		ui_caixa((NEO_W - w) / 2, 96, (NEO_W + w) / 2, 124, UI_FUNDO, 245);
		ui_retangulo((NEO_W - w) / 2, 96, (NEO_W + w) / 2, 124, UI_AMARELO, 0);
		ui_texto_centro(106, b, UI_BRANCO);
	}
	if (SDL_GetTicks() < aviso_t) {
		int w = ui_largura(aviso_txt) + 16;
		ui_caixa((NEO_W - w) / 2, NEO_H - 32, (NEO_W + w) / 2, NEO_H - 18, 0xff1d6b3au, 240);
		ui_texto_centro(NEO_H - 28, aviso_txt, UI_BRANCO);
	}
}

void menu_cfg_abre(int qual) { cfgmenu_aberto = 1; aba = qual % NABAS; linha = 0; }
