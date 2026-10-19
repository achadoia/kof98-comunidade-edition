// Tela de configurações (F1): abas Vídeo, Controles, Som e Melhorias. Desenhada em alta resolução (1672x941,
// src/hd.c) por cima do jogo pausado, com a arte arte/hd_cfg_fundo.png de fundo. Navega com as setas, com as
// teclas/botões configurados dos 2 jogadores e com os botões fixos do controle (entrada_evento_menu).
#include "menu_cfg.h"
#include "config.h"
#include "entrada.h"
#include "hd.h"
#include "musica.h"
#include "efeitos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cfgmenu_aberto;
static int aba, linha, jogador;
static int capturando = -1;      // ação esperando tecla/botão (-1 = nenhuma)
static int assistente;           // capturando todas em sequência
static char aviso_txt[80];
static u32 aviso_t;
extern char cfg_arquivo[512];

static const char *ABAS[] = {"VÍDEO", "CONTROLES", "SOM", "MELHORIAS"};
#define NABAS 4

static const char *liga(int v) { return v ? "ligado" : "desligado"; }

// linhas de cada aba ------------------------------------------------------------
static int linhas_aba(void)
{
	switch (aba) {
	case 0: return 9;
	case 1: return 3 + NACOES;     // jogador, assistente, ações..., restaurar
	case 2: return 5;
	default: return 7;
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
		case 6: cfg.atraso_online = (cfg.atraso_online + d + 11) % 11; break;
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

// guarda a ligação capturada no slot do seu tipo: 0 teclado, 1 botão / 2 eixo do controle reconhecido,
// 3 botão ou hat / 4 eixo de stick cru (cada tipo tem o seu lugar, nada se perde)
static int slot_de(int tipo)
{
	switch (tipo) { case LIG_TECLA: return 0; case LIG_GCBOTAO: return 1; case LIG_GCEIXO: return 2; case LIG_EIXO: return 4; default: return 3; }
}
static void grava_ligacao(Ligacao l)
{
	Ligacao *s = cfg.lig[jogador][capturando];
	s[slot_de(l.tipo)] = l;
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

	int t = entrada_evento_menu(e);
	int n = linhas_aba();
#ifdef KOF_TESTE
	if (getenv("KOF_DEBUG_ENTRADA") && t) fprintf(stderr, "menu F1: evento %d -> comando %d (aba %d linha %d)\n", e->type, t, aba, linha);
#endif
	switch (t) {
	case MENU_CIMA: linha = linha <= -1 ? -1 : linha - 1; break;                 // -1 = barra de abas
	case MENU_BAIXO: linha = linha + 1 >= n ? n - 1 : linha + 1; break;
	case MENU_ESQ: if (linha < 0) aba = (aba + NABAS - 1) % NABAS; else muda(-1); break;
	case MENU_DIR: if (linha < 0) aba = (aba + 1) % NABAS; else muda(1); break;
	case MENU_OK: if (linha < 0) linha = 0; else confirma(); break;
	case MENU_VOLTAR: cfgmenu_aberto = 0; salva(); break;
	case MENU_ABA_ANT: aba = (aba + NABAS - 1) % NABAS; linha = 0; break;
	case MENU_ABA_SEG: aba = (aba + 1) % NABAS; linha = 0; break;
	}
	return 1;
}

// desenho em 1672x941 ---------------------------------------------------------------
#define C_BRANCO 0xf4f4f4u
#define C_CINZA 0xe4e4e4u
#define C_APAGADO 0xc0c0c0u
#define C_LARANJA 0xf08a1eu
#define C_VERMELHO 0xc8281au
#define C_AMARELO 0xffd020u
#define C_VERDE 0x40ff70u
#define X_NOME 95
#define X_VALOR 1000
#define X_PAINEL1 1032

static char maiu[160];
static const char *M(const char *s) { hd_maiusc(maiu, s, sizeof maiu); return maiu; }

// uma linha de opção: nome à esquerda, valor à direita (com setas quando selecionada)
static void item(int y, int i, const char *nome, const char *valor, int passo)
{
	int sel = i == linha, alt = passo - 7, yc = y + 12;
	if (sel) hd_degrade(48, yc - alt / 2, X_PAINEL1, yc + alt / 2, 0xd93a1cu, 0xf29a1au, 255, 10);
	else hd_caixa(80, yc + alt / 2 + 2, X_VALOR + 10, yc + alt / 2 + 3, 0x8a3018u, 140);
	hd_texto(X_NOME, y, M(nome), sel ? C_BRANCO : C_CINZA, 3);
	if (!valor) return;
	char v[96];
	snprintf(v, sizeof v, "%s", M(valor));
	int fim_nome = X_NOME + hd_largura(M(nome), 3) + 30, esc = 3;
	if (sel) {
		if (X_VALOR - 48 - hd_largura(v, 3) < fim_nome) esc = 2;        // valor comprido: letra menor pra não invadir o nome
		int w = hd_largura(v, esc), x0 = X_VALOR - 28 - w;
		hd_texto(x0, y + (esc == 2 ? 4 : 0), v, C_BRANCO, esc);
		hd_seta(x0 - 20, yc, -1, 11, C_BRANCO);
		hd_seta(X_VALOR - 14, yc, 1, 11, C_BRANCO);
	} else {
		if (X_VALOR - hd_largura(v, 3) < fim_nome) esc = 2;
		hd_texto_dir(X_VALOR, y + (esc == 2 ? 4 : 0), v, C_CINZA, esc);
	}
}

static int ajuda(int y, const char *s, u32 cor) { hd_texto(X_NOME, y, M(s), cor, 3); return y + 35; }

static void abas(void)
{
	int x = 60, y0 = 145, y1 = 195;
	for (int i = 0; i < NABAS; i++) {
		int w = hd_largura(ABAS[i], 3) + 100, ativa = i == aba;
		if (ativa) {
			hd_degrade(x, y0, x + w, y1, 0xd8341cu, 0xe8701eu, 255, 14);
			if (linha < 0) hd_caixa(x, y1 - 5, x + w + 14, y1, C_AMARELO, 255);   // foco na barra de abas
		} else {
			hd_caixa(x, y0, x + w, y1, 0x141418u, 225);
			hd_borda(x, y0, x + w, y1, 0xc8c8c8u, 2);
		}
		hd_texto(x + 50 + (ativa ? 8 : 0), y0 + 13, ABAS[i], ativa ? C_BRANCO : C_CINZA, 3);
		x += w + 10;
	}
}

static void rodape(const char *esq, const char *dir)
{
	hd_caixa(0, 852, HD_W, HD_H, 0x06070cu, 235);
	hd_caixa(0, 849, HD_W, 852, C_VERMELHO, 255);
	int x = 600;
	hd_imagem(x, 862, &hd_dpad); x += hd_dpad.w + 14;
	x = hd_texto(x, 876, esq, C_BRANCO, 3) + 70;
	hd_imagem(x, 864, &hd_botao_b); x += hd_botao_b.w + 14;
	hd_texto(x, 876, dir, C_BRANCO, 3);
}

void menu_cfg_desenha_hd(void)
{
	if (!cfgmenu_aberto) return;
	u32 *buf = hd_buffer();
	if (hd_cfg_fundo.px && hd_cfg_fundo.w == HD_W && hd_cfg_fundo.h == HD_H) memcpy(buf, hd_cfg_fundo.px, (size_t)HD_W * HD_H * 4);
	else { hd_caixa(0, 0, HD_W, HD_H, 0x05070cu, 255); hd_texto(60, 40, "CONFIGURAÇÕES", C_LARANJA, 6); }
	hd_texto_dir(1640, 28, "F1  FECHA", C_BRANCO, 3);
	abas();
	// moldura do painel: linha fina à esquerda e embaixo, canto cortado
	hd_caixa(46, 240, 48, 828, 0xb8321au, 210);
	for (int i = 0; i < 18; i++) hd_caixa(46 + i, 828 + i, 48 + i, 830 + i, 0xb8321au, 210);
	hd_caixa(64, 846, 1058, 848, 0xb8321au, 210);

	char v[64];
	int y = 243;
	switch (aba) {
	case 0:
		item(y, 0, "Filtro de imagem", NOME_FILTRO[cfg.filtro], 47); y += 47;
		item(y, 1, "Proporção", NOME_PROP[cfg.proporcao], 47); y += 47;
		item(y, 2, "Laterais borradas", liga(cfg.fundo_borrado), 47); y += 47;
		item(y, 3, "Tela cheia  (F11)", liga(cfg.tela_cheia), 47); y += 47;
		item(y, 4, "Sincronia vertical (vsync)", liga(cfg.vsync), 47); y += 47;
		item(y, 5, "Mostrar FPS", liga(cfg.mostra_fps), 47); y += 47;
		{ extern int ritmo_monitor_ativo; extern double ritmo_monitor_hz;
		  if (!cfg.ritmo_monitor) snprintf(v, sizeof v, "desligado (59,19 Hz)");
		  else if (ritmo_monitor_ativo) snprintf(v, sizeof v, "%.0f Hz", ritmo_monitor_hz);
		  else snprintf(v, sizeof v, "monitor incompatível"); }
		item(y, 6, "Ritmo do monitor", v, 47); y += 47;
		item(y, 7, "Sprites sem limite", liga(cfg.sprites_sem_limite), 47); y += 47;
		item(y, 8, "Fotos dos homenageados em HD", liga(cfg.fotos), 47); y += 47;
		y += 22;
		{ int x = hd_texto(X_NOME, y, "F4", C_LARANJA, 3); hd_texto(x + 36, y, "TROCA O FILTRO DURANTE O JOGO", C_BRANCO, 3); y += 42; }
		y = ajuda(y, "Ritmo do monitor: o jogo roda a 60 Hz", C_APAGADO);
		y = ajuda(y, "(1,4% mais rápido) casado com o vsync,", C_APAGADO);
		y = ajuda(y, "sem o engasgo de 59,19 Hz na tela de 60.", C_APAGADO);
		break;
	case 1: {
		int passo = 31;
		snprintf(v, sizeof v, "jogador %d  (%d ctrl)", jogador + 1, entrada_num_controles());
		item(y, 0, "Editando", v, passo); y += passo;
		item(y, 1, "Configurar tudo (passo a passo)", NULL, passo); y += passo;
		for (int a = 0; a < NACOES; a++) {
			int i = 2 + a, sel = i == linha, ativa = acao_ativa[jogador][a];
			char d1[32] = "", d2[32] = "", tudo[80];
			const Ligacao *L = cfg.lig[jogador][a];
			config_descreve(&L[0], d1, sizeof d1);
			{   // mostra a ligação que vale pro controle ligado: reconhecido = slots 1/2, cru = 3/4
				int cru = entrada_num_controles() > jogador && !entrada_reconhecido(jogador);
				const Ligacao *c = cru ? (L[3].tipo ? &L[3] : &L[4]) : (L[1].tipo ? &L[1] : &L[2]);
				config_descreve(c, d2, sizeof d2);
			}
			if (capturando == a) snprintf(tudo, sizeof tudo, "aperte...");
			else snprintf(tudo, sizeof tudo, "%s / %s", d1, d2);
			if ((int)strlen(tudo) > 26) tudo[26] = 0;
			item(y, i, NOME_ACAO[a], tudo, passo);
			if (ativa) hd_caixa(62, y + 2, 80, y + 22, C_VERDE, 255);                       // apertado agora
			if (capturando == a) hd_texto(X_NOME, y, M(NOME_ACAO[a]), C_AMARELO, 3);
			else if (ativa && !sel) hd_texto(X_NOME, y, M(NOME_ACAO[a]), C_VERDE, 3);
			y += passo;
		}
		item(y, 2 + NACOES, "Restaurar padrão", NULL, passo);
		break;
	}
	case 2:
		snprintf(v, sizeof v, "%d%%", cfg.volume);
		item(y, 0, "Volume", v, 47); y += 47;
		item(y, 1, "Mudo", liga(cfg.mudo), 47); y += 47;
		item(y, 2, "Músicas da pasta som/musica", liga(cfg.musica_arquivos), 47); y += 47;
		item(y, 3, "Vozes/efeitos de som/efeitos", liga(cfg.efeitos_arquivos), 47); y += 47;
		snprintf(v, sizeof v, "%d + %d arquivos", musica_quantas(), efeitos_quantos());
		item(y, 4, "Reler as pastas", v, 47); y += 47;
		y += 12;
		for (int i = 0; i < 10; i++) {
			int on = i * 10 < cfg.volume && !cfg.mudo;
			if (on) hd_degrade(X_NOME + i * 46, y, X_NOME + i * 46 + 38, y + 30, 0xd93a1cu, 0xf29a1au, 255, 0);
			else hd_caixa(X_NOME + i * 46, y, X_NOME + i * 46 + 38, y + 30, 0x2a2a30u, 255);
		}
		y += 52;
		{   // qual música está tocando: ajuda a saber que arquivo trocar
			extern int musica_ultima_cod;
			int c = musica_ultima_cod;
			const char *arq = c >= 0 ? musica_arquivo(c == 0x4b ? 0x21 : c) : NULL;
			if (c < 0) snprintf(v, sizeof v, "Tocando: nenhuma");
			else if (musica_tocando() >= 0 && arq) {
				const char *n = strrchr(arq, '/');
				snprintf(v, sizeof v, "Tocando: %02X  %s", c, n ? n + 1 : arq);
			} else snprintf(v, sizeof v, "Tocando: %02X  (chip original)", c);
			if ((int)strlen(v) > 38) v[38] = 0;
			hd_texto(X_NOME, y, M(v), C_AMARELO, 3); y += 42;
		}
		y = ajuda(y, "Pra trocar: .ogg .mp3 ou .wav em", C_APAGADO);
		y = ajuda(y, "som/musica, com o nome começando", C_APAGADO);
		y = ajuda(y, "pelo código (ex.: \"28 rock.mp3\").", C_APAGADO);
		break;
	default:
		item(y, 0, "Cenário", config_nome_cenario(cfg.cenario), 47); y += 47;
		item(y, 1, "Pausar sem foco", liga(cfg.pausa_sem_foco), 47); y += 47;
		item(y, 2, "Início rápido (pula o logo)", liga(cfg.inicio_rapido), 47); y += 47;
		item(y, 3, "Dica do F6 ao iniciar", liga(cfg.dica_treino), 47); y += 47;
		snprintf(v, sizeof v, cfg.run_ahead ? "%d quadro%s" : "desligado", cfg.run_ahead, cfg.run_ahead > 1 ? "s" : "");
		item(y, 4, "Run-ahead (resposta rápida)", v, 47); y += 47;
		item(y, 5, "Ordem do trio por botão", liga(cfg.ordem_botao), 47); y += 47;
		snprintf(v, sizeof v, cfg.atraso_online ? "%d quadros" : "auto (pelo ping)", cfg.atraso_online);
		item(y, 6, "Atraso online (anfitrião)", v, 47); y += 47;
		y += 22;
		y = ajuda(y, "Run-ahead: emula à frente e volta,", C_APAGADO);
		y = ajuda(y, "tira 1-2 quadros do atraso do jogo.", C_APAGADO);
		y = ajuda(y, "Ordem por botão: no ORDER SELECT,", C_APAGADO);
		y = ajuda(y, "A/B/C = 1º/2º/3º escolhido começa", C_APAGADO);
		y = ajuda(y, "(o adversário não vê).", C_APAGADO);
		break;
	}

	rodape("SELECIONAR", "VOLTAR");
	if (aba == 1 && capturando < 0) {   // o que o controle está mandando agora: mostra na hora se ele é reconhecido e se os botões chegam
		char est[64], lin[128];
		entrada_estado_texto(jogador, est, sizeof est);
		snprintf(lin, sizeof lin, "controle %d %s: %s", jogador + 1, entrada_num_controles() > jogador ? (entrada_reconhecido(jogador) ? "ok" : "cru") : "-", est);
		if ((int)strlen(lin) > 58) lin[58] = 0;
		hd_texto(X_NOME, 808, M(lin), C_VERDE, 2);
	}
	if (capturando >= 0) {
		char b[96];
		snprintf(b, sizeof b, "P%d: aperte para  %s", jogador + 1, NOME_ACAO[capturando]);
		int w = hd_largura(M(b), 3) + 90;
		hd_caixa((HD_W - w) / 2, 410, (HD_W + w) / 2, 520, 0x0a0c14u, 245);
		hd_borda((HD_W - w) / 2, 410, (HD_W + w) / 2, 520, C_AMARELO, 3);
		hd_texto_centro(HD_W / 2, 432, M(b), C_BRANCO, 3);
		hd_texto_centro(HD_W / 2, 478, "ESC PULA   DEL APAGA", C_APAGADO, 2);
	}
	if (SDL_GetTicks() < aviso_t) {
		int w = hd_largura(M(aviso_txt), 3) + 60;
		hd_caixa((HD_W - w) / 2, 786, (HD_W + w) / 2, 836, 0x1d6b3au, 240);
		hd_texto_centro(HD_W / 2, 799, M(aviso_txt), C_BRANCO, 3);
	}
}

void menu_cfg_abre(int qual) { cfgmenu_aberto = 1; aba = qual % NABAS; linha = 0; }
