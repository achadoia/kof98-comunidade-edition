// Jogo online ponto a ponto. Ver rede.h.
//
// Pacotes (UDP, little-endian, cabeçalho "KN" + tipo):
//   OLA      versao u32, rom u32, papel u8, vi_voce u8        — procura o outro lado (a cada 200 ms)
//   PING/PONG t u32                                            — latência
//   INICIO   atraso u8, ordem u8, nonce u32                    — anfitrião define a partida; convidado responde PRONTO
//   PRONTO   nonce u32
//   ENTRADA  ini u32, n u8, n×(p u8, x u8), ack u32, crc_q u32, crc u32   — últimas entradas (redundância contra perda)
//   TCHAU
// Furo no NAT: cada lado manda OLA pro endereço público e pro endereço local do outro; o roteador que viu sair
// aceita a resposta. Quando o roteador do anfitrião não aceita (NAT simétrico), o anfitrião pode digitar o código
// do convidado e os dois furam ao mesmo tempo. UPnP tenta abrir a porta do anfitrião numa thread.
#include "rede.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define VERSAO_REDE 1
#define PORTA_BASE 7998
#define RING 512
#define OPEN_T 200

enum { T_OLA = 1, T_PING, T_PONG, T_INICIO, T_PRONTO, T_ENTRADA, T_TCHAU };

int rede_estado, rede_anfitriao, rede_ativa, rede_atraso = 3, rede_ping, rede_dessincronizou, rede_upnp = 2;
char rede_codigo[32], rede_msg[128];
int rede_escolha_pendente = -1;
int rede_atraso_cfg;             // 0 = automático pelo ping
int rede_ordem;                  // ORDER SELECT secreto (A/B/C) na partida: decisão do anfitrião

static SOCKET sock = INVALID_SOCKET;
static int porta_local;
static struct sockaddr_in par;              // o outro lado (quando conhecido)
static int par_ok, vi_par, par_me_ve;
static struct sockaddr_in cand[4];          // endereços candidatos do outro lado
static int ncand;
static u32 t_ola, t_ping, t_ultimo_rx, t_inicio_envio, t_pareou;
static u32 nonce, nonce_visto;
static int pongs;
static u32 rom_hash;
static int ping_acum[8], ping_n;

// STUN
static struct sockaddr_in stun_addr;
static u8 stun_tid[12];
static int stun_fase;                       // 0 nada, 1 esperando, 2 ok, -1 falhou
static u32 t_stun; static int stun_tentativas;
static u32 pub_ip; static u16 pub_porta;    // em ordem de rede
static u32 lan_ip;

// lockstep
static u8 loc_p[RING], loc_x[RING], rem_p[RING], rem_x[RING];
static u32 rem_q[RING];
static u32 quadro, loc_prox, rem_tem_ate, par_ack;
static u32 t_envio, esperando_n;
static u32 meu_crc[16], meu_crc_q[16];
static u32 crc_envia_q, crc_envia;

// ---------------------------------------------------------------- util
static void le32(const u8 *p, u32 *v) { memcpy(v, p, 4); }
static void es32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static void envia_para(const struct sockaddr_in *a, const u8 *b, int n) { if (sock != INVALID_SOCKET) sendto(sock, (const char *)b, n, 0, (const struct sockaddr *)a, sizeof *a); }
static void envia(const u8 *b, int n) { if (par_ok) envia_para(&par, b, n); }
static int mesmo(const struct sockaddr_in *a, const struct sockaddr_in *b) { return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port; }

static u32 hash_rom(void)
{
	u32 h = 2166136261u;
	for (u32 i = 0; i < R.p_size; i++) { h ^= R.p[i]; h *= 16777619u; }
	return h;
}

static const char ALFA[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";   // base32 sem 0/O/1/I
static void codifica(const u8 *b, int nb, char *s)       // nb*8 bits -> ceil(nb*8/5) chars
{
	int bits = 0; u32 acc = 0; int o = 0;
	for (int i = 0; i < nb; i++) {
		acc = (acc << 8) | b[i]; bits += 8;
		while (bits >= 5) { s[o++] = ALFA[(acc >> (bits - 5)) & 31]; bits -= 5; }
	}
	if (bits) s[o++] = ALFA[(acc << (5 - bits)) & 31];
	s[o] = 0;
}
static int decodifica(const char *s, u8 *b, int nb)
{
	int bits = 0; u32 acc = 0; int o = 0;
	for (; *s; s++) {
		if (*s == '-' || *s == ' ') continue;
		const char *p = strchr(ALFA, toupper((unsigned char)*s));
		if (!p) return 0;
		acc = (acc << 5) | (u32)(p - ALFA); bits += 5;
		if (bits >= 8) { if (o < nb) b[o++] = (u8)(acc >> (bits - 8)); bits -= 8; }
	}
	return o == nb;
}
static void monta_codigo(void)
{
	u8 b[10];
	memcpy(b, &pub_ip, 4); memcpy(b + 4, &pub_porta, 2); memcpy(b + 6, &lan_ip, 4);
	char s[24]; codifica(b, 10, s);
	snprintf(rede_codigo, sizeof rede_codigo, "%.4s-%.4s-%.4s-%.4s", s, s + 4, s + 8, s + 12);
}

static u32 descobre_lan(void)
{
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) return 0;
	struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(53); a.sin_addr.s_addr = inet_addr("8.8.8.8");
	u32 ip = 0;
	if (connect(s, (struct sockaddr *)&a, sizeof a) == 0) { struct sockaddr_in me; int n = sizeof me; if (getsockname(s, (struct sockaddr *)&me, &n) == 0) ip = me.sin_addr.s_addr; }
	closesocket(s);
	return ip;
}

static int resolve(const char *host, u16 porta, struct sockaddr_in *out)
{
	struct addrinfo dica = {0}, *r = NULL;
	dica.ai_family = AF_INET; dica.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(host, NULL, &dica, &r) != 0 || !r) return 0;
	*out = *(struct sockaddr_in *)r->ai_addr; out->sin_port = htons(porta);
	freeaddrinfo(r);
	return 1;
}

// ---------------------------------------------------------------- UPnP (thread)
static char upnp_ctrl[512], upnp_host[128]; static int upnp_porta_http; static char upnp_srv[96];
static int http(const char *host, int porta, const char *req, int nreq, char *resp, int cap)
{
	struct sockaddr_in a; if (!resolve(host, (u16)porta, &a)) return -1;
	SOCKET s = socket(AF_INET, SOCK_STREAM, 0); if (s == INVALID_SOCKET) return -1;
	DWORD to = 3000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof to); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&to, sizeof to);
	int n = -1;
	if (connect(s, (struct sockaddr *)&a, sizeof a) == 0 && send(s, req, nreq, 0) == nreq) {
		n = 0; int r;
		while (n < cap - 1 && (r = recv(s, resp + n, cap - 1 - n, 0)) > 0) n += r;
		resp[n] = 0;
	}
	closesocket(s);
	return n;
}
static int upnp_soap(const char *acao, const char *args, char *resp, int cap)
{
	char corpo[1024], req[2048];
	snprintf(corpo, sizeof corpo, "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>", acao, upnp_srv, args, acao);
	int n = snprintf(req, sizeof req, "POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"%s#%s\"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", upnp_ctrl, upnp_host, upnp_porta_http, upnp_srv, acao, (int)strlen(corpo), corpo);
	return http(upnp_host, upnp_porta_http, req, n, resp, cap);
}
static int upnp_thread(void *arg)
{
	int porta = (int)(intptr_t)arg;
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) { rede_upnp = -1; return 0; }
	DWORD to = 2500; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof to);
	const char *busca = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
	struct sockaddr_in m = {0}; m.sin_family = AF_INET; m.sin_port = htons(1900); m.sin_addr.s_addr = inet_addr("239.255.255.250");
	sendto(s, busca, (int)strlen(busca), 0, (struct sockaddr *)&m, sizeof m);
	static char buf[65536]; char loc[512] = "";
	for (int t = 0; t < 4 && !loc[0]; t++) {
		int n = recv(s, buf, sizeof buf - 1, 0);
		if (n <= 0) break;
		buf[n] = 0;
		for (char *p = buf; *p; p++) if (!_strnicmp(p, "LOCATION:", 9)) { p += 9; while (*p == ' ') p++; char *e = strpbrk(p, "\r\n"); if (e) *e = 0; snprintf(loc, sizeof loc, "%s", p); break; }
	}
	closesocket(s);
	if (!loc[0] || strncmp(loc, "http://", 7)) { rede_upnp = -1; return 0; }
	char *h = loc + 7, *cam = strchr(h, '/'); if (!cam) { rede_upnp = -1; return 0; }
	snprintf(upnp_host, sizeof upnp_host, "%.*s", (int)(cam - h), h);
	upnp_porta_http = 80;
	char *dp = strchr(upnp_host, ':'); if (dp) { *dp = 0; upnp_porta_http = atoi(dp + 1); }
	char req[1024]; int n = snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", cam, upnp_host, upnp_porta_http);
	if (http(upnp_host, upnp_porta_http, req, n, buf, sizeof buf) <= 0) { rede_upnp = -1; return 0; }
	const char *tipos[] = {"urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANPPPConnection:1", "urn:schemas-upnp-org:service:WANIPConnection:2"};
	char *st = NULL;
	for (int i = 0; i < 3 && !st; i++) { st = strstr(buf, tipos[i]); if (st) snprintf(upnp_srv, sizeof upnp_srv, "%s", tipos[i]); }
	char *cu = st ? strstr(st, "<controlURL>") : NULL;
	if (!cu) { rede_upnp = -1; return 0; }
	cu += 12; char *fim = strchr(cu, '<'); if (!fim) { rede_upnp = -1; return 0; }
	if (!strncmp(cu, "http://", 7)) { char *c2 = strchr(cu + 7, '/'); cu = c2 ? c2 : cu; }
	snprintf(upnp_ctrl, sizeof upnp_ctrl, "%.*s", (int)(fim - cu), cu);
	char ip[32]; struct in_addr ia; ia.s_addr = lan_ip; snprintf(ip, sizeof ip, "%s", inet_ntoa(ia));
	char args[768];
	for (int tent = 0; tent < 2; tent++) {
		snprintf(args, sizeof args, "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol><NewInternalPort>%d</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>KOF98 Comunidade Edition</NewPortMappingDescription><NewLeaseDuration>%d</NewLeaseDuration>", porta, porta, ip, tent ? 0 : 7200);
		if (upnp_soap("AddPortMapping", args, buf, sizeof buf) > 0 && strstr(buf, "200 OK")) { rede_upnp = 1; return 0; }
	}
	rede_upnp = -1;
	return 0;
}

// ---------------------------------------------------------------- STUN
static const char *STUNS[] = {"stun.l.google.com", "stun1.l.google.com", "stun.cloudflare.com"};
static const u16 STUNP[] = {19302, 19302, 3478};
static void stun_envia(void)
{
	int i = stun_tentativas % 3;
	if (!resolve(STUNS[i], STUNP[i], &stun_addr)) { stun_tentativas++; return; }
	u8 r[20] = {0, 1, 0, 0, 0x21, 0x12, 0xa4, 0x42};
	for (int k = 0; k < 12; k++) stun_tid[k] = r[8 + k] = (u8)(rand() ^ SDL_GetTicks() >> (k & 7));
	envia_para(&stun_addr, r, 20);
	t_stun = SDL_GetTicks(); stun_tentativas++; stun_fase = 1;
}
static void stun_recebe(const u8 *b, int n)
{
	if (n < 20 || b[0] != 1 || b[1] != 1 || memcmp(b + 8, stun_tid, 12)) return;
	int len = b[2] << 8 | b[3], p = 20;
	while (p + 4 <= 20 + len && p + 4 <= n) {
		int tipo = b[p] << 8 | b[p + 1], al = b[p + 2] << 8 | b[p + 3];
		if ((tipo == 0x0020 || tipo == 0x0001) && al >= 8 && b[p + 5] == 1) {
			u16 porta; u32 ip; memcpy(&porta, b + p + 6, 2); memcpy(&ip, b + p + 8, 4);
			if (tipo == 0x0020) { porta ^= htons(0x2112); ip ^= htonl(0x2112A442u); }
			pub_porta = porta; pub_ip = ip; stun_fase = 2;
			monta_codigo();
			return;
		}
		p += 4 + ((al + 3) & ~3);
	}
}

// ---------------------------------------------------------------- pacotes
static void manda_ola(void)
{
	u8 b[16] = {'K', 'N', T_OLA};
	es32(b + 3, VERSAO_REDE); es32(b + 7, rom_hash); b[11] = (u8)rede_anfitriao; b[12] = (u8)vi_par;
	if (par_ok) envia(b, 13);
	for (int i = 0; i < ncand; i++) if (!par_ok || !mesmo(&cand[i], &par)) envia_para(&cand[i], b, 13);
}
static void manda_ping(void) { u8 b[8] = {'K', 'N', T_PING}; es32(b + 3, SDL_GetTicks()); envia(b, 7); }
static void manda_inicio(void) { u8 b[12] = {'K', 'N', T_INICIO, (u8)rede_atraso, (u8)rede_ordem}; es32(b + 5, nonce); envia(b, 9); }
static void manda_pronto(void) { u8 b[8] = {'K', 'N', T_PRONTO}; es32(b + 3, nonce_visto); envia(b, 7); }
static void manda_entradas(void)
{
	if (!rede_ativa || !par_ok) return;
	u32 ini = loc_prox > 64 ? loc_prox - 64 : 0;
	if (par_ack > ini) ini = par_ack;
	if (loc_prox > 16 && ini > loc_prox - 16) ini = loc_prox - 16;
	if (ini >= loc_prox) ini = loc_prox ? loc_prox - 1 : 0;
	u32 n = loc_prox - ini; if (n > 64) n = 64;
	u8 b[3 + 4 + 1 + 128 + 12];
	b[0] = 'K'; b[1] = 'N'; b[2] = T_ENTRADA; es32(b + 3, ini); b[7] = (u8)n;
	for (u32 i = 0; i < n; i++) { b[8 + i * 2] = loc_p[(ini + i) % RING]; b[9 + i * 2] = loc_x[(ini + i) % RING]; }
	u8 *q = b + 8 + n * 2;
	es32(q, rem_tem_ate); es32(q + 4, crc_envia_q); es32(q + 8, crc_envia);
	envia(b, (int)(8 + n * 2 + 12));
	t_envio = SDL_GetTicks();
}

static void comeca_partida(void)
{
	rede_estado = REDE_LIGADA; rede_ativa = 1; rede_dessincronizou = 0;
	quadro = 0; rem_tem_ate = 0; par_ack = 0; esperando_n = 0;
	memset(rem_q, 0xff, sizeof rem_q);
	for (int i = 0; i < rede_atraso; i++) { loc_p[i] = 0xff; loc_x[i] = 0; }
	loc_prox = (u32)rede_atraso;
	memset(meu_crc_q, 0xff, sizeof meu_crc_q); crc_envia_q = 0xffffffffu;
	snprintf(rede_msg, sizeof rede_msg, "Conectado! ping %d ms, atraso %d quadros", rede_ping, rede_atraso);
	extern void frontend_online_comeca(void);
	frontend_online_comeca();
}

static void recebe(const u8 *b, int n, const struct sockaddr_in *de)
{
	if (n < 3 || b[0] != 'K' || b[1] != 'N') return;
	if (par_ok && !mesmo(de, &par)) { if (b[2] != T_OLA) return; }   // só troca de par por um OLA (ex.: LAN responde antes)
	if (!par_ok || (b[2] == T_OLA && !par_me_ve && !mesmo(de, &par))) { par = *de; par_ok = 1; }
	t_ultimo_rx = SDL_GetTicks();
	u32 v;
	switch (b[2]) {
	case T_OLA:
		if (n < 13) return;
		le32(b + 3, &v); if (v != VERSAO_REDE) { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "O outro lado usa outra versão do jogo"); return; }
		le32(b + 7, &v); if (v != rom_hash) { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "ROM diferente do outro lado (kof98ae2016.zip)"); return; }
		if (b[11] == (u8)rede_anfitriao) { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, rede_anfitriao ? "Os dois criaram sala: um deve ENTRAR" : "Os dois entraram: um deve CRIAR a sala"); return; }
		vi_par = 1; if (b[12]) par_me_ve = 1;
		if (rede_estado == REDE_CRIANDO || rede_estado == REDE_ENTRANDO) { rede_estado = REDE_PAREANDO; t_pareou = SDL_GetTicks(); pongs = 0; ping_n = 0; }
		manda_ola();
		break;
	case T_PING: { u8 r[8] = {'K', 'N', T_PONG}; memcpy(r + 3, b + 3, 4); envia(r, 7); } break;
	case T_PONG:
		if (n < 7) return;
		le32(b + 3, &v);
		{ int rtt = (int)(SDL_GetTicks() - v); ping_acum[ping_n % 8] = rtt; ping_n++; pongs++;
		  int s = 0, k = ping_n < 8 ? ping_n : 8; for (int i = 0; i < k; i++) s += ping_acum[i]; rede_ping = s / k; }
		break;
	case T_INICIO:
		if (n < 9 || rede_anfitriao) return;
		le32(b + 5, &v);
		if (rede_ativa && v == nonce_visto) { manda_pronto(); return; }
		if (rede_ativa) return;
		nonce_visto = v; rede_atraso = b[3]; rede_ordem = b[4];
		manda_pronto();
		comeca_partida();
		break;
	case T_PRONTO:
		if (n < 7 || !rede_anfitriao || rede_ativa) return;
		le32(b + 3, &v); if (v != nonce) return;
		comeca_partida();
		break;
	case T_ENTRADA: {
		if (!rede_ativa || n < 8) return;
		u32 ini; le32(b + 3, &ini); u32 cnt = b[7];
		if (n < (int)(8 + cnt * 2 + 12)) return;
		for (u32 i = 0; i < cnt; i++) {
			u32 f = ini + i;
			if (f < rem_tem_ate || f >= rem_tem_ate + RING - 8) continue;
			rem_p[f % RING] = b[8 + i * 2]; rem_x[f % RING] = b[9 + i * 2]; rem_q[f % RING] = f;
		}
		while (rem_q[rem_tem_ate % RING] == rem_tem_ate) rem_tem_ate++;
		const u8 *q = b + 8 + cnt * 2;
		le32(q, &v); if (v > par_ack) par_ack = v;
		u32 cq, cc; le32(q + 4, &cq); le32(q + 8, &cc);
		if (cq != 0xffffffffu) for (int i = 0; i < 16; i++) if (meu_crc_q[i] == cq && meu_crc[i] != cc) rede_dessincronizou = 1;
		break; }
	case T_TCHAU:
		if (rede_estado == REDE_DESLIGADA) return;
		rede_ativa = 0; rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "O outro jogador saiu");
		{ extern void frontend_online_caiu(void); frontend_online_caiu(); }
		break;
	}
}

// ---------------------------------------------------------------- API
int rede_init(void)
{
	WSADATA w; return WSAStartup(MAKEWORD(2, 2), &w) != 0;
}
void rede_fim(void) { rede_desligar(); WSACleanup(); }

static int abre_porta(void)
{
	if (sock != INVALID_SOCKET) return 1;
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) { snprintf(rede_msg, sizeof rede_msg, "Não consegui criar o socket UDP"); return 0; }
	for (int p = PORTA_BASE; p < PORTA_BASE + 12; p++) {
		struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons((u16)p); a.sin_addr.s_addr = INADDR_ANY;
		if (bind(sock, (struct sockaddr *)&a, sizeof a) == 0) { porta_local = p; break; }
	}
	if (!porta_local) { closesocket(sock); sock = INVALID_SOCKET; snprintf(rede_msg, sizeof rede_msg, "Nenhuma porta UDP livre (7998-8009)"); return 0; }
	u_long nb = 1; ioctlsocket(sock, FIONBIO, &nb);
	return 1;
}
static void zera(void)
{
	par_ok = vi_par = par_me_ve = 0; ncand = 0; rede_ativa = 0; rede_dessincronizou = 0; pongs = 0; ping_n = 0; rede_ping = 0;
	rede_codigo[0] = 0; rede_msg[0] = 0; stun_fase = 0; stun_tentativas = 0; rede_escolha_pendente = -1;
	lan_ip = descobre_lan(); rom_hash = hash_rom(); nonce = (u32)SDL_GetTicks() * 2654435761u + (u32)rand();
	t_ultimo_rx = t_ola = SDL_GetTicks();
}
int rede_criar(void)
{
	rede_desligar();
	zera();
	if (!abre_porta()) { rede_estado = REDE_ERRO; return 0; }
	rede_anfitriao = 1; rede_estado = REDE_CRIANDO;
	stun_envia();
	rede_upnp = 0;
	SDL_DetachThread(SDL_CreateThread(upnp_thread, "upnp", (void *)(intptr_t)porta_local));
	return 1;
}
int rede_entrar(const char *codigo)
{
	rede_desligar();
	zera();
	if (!abre_porta()) { rede_estado = REDE_ERRO; return 0; }
	rede_anfitriao = 0;
	u8 b[10]; char ip[64];
	if (decodifica(codigo, b, 10)) {
		struct sockaddr_in a = {0}; a.sin_family = AF_INET;
		memcpy(&a.sin_addr.s_addr, b, 4); memcpy(&a.sin_port, b + 4, 2); cand[ncand++] = a;
		memcpy(&a.sin_addr.s_addr, b + 6, 4); if (a.sin_addr.s_addr && a.sin_addr.s_addr != cand[0].sin_addr.s_addr) cand[ncand++] = a;
	} else if (sscanf(codigo, "%63[0-9.]", ip) == 1 && strchr(ip, '.')) {   // "ip" ou "ip:porta" (Radmin/ZeroTier/LAN)
		struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr(ip);
		const char *dp = strchr(codigo, ':'); a.sin_port = htons((u16)(dp ? atoi(dp + 1) : PORTA_BASE));
		if (a.sin_addr.s_addr == INADDR_NONE) { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "Código inválido"); return 0; }
		cand[ncand++] = a;
	} else { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "Código inválido (16 letras ou ip:porta)"); return 0; }
	rede_estado = REDE_ENTRANDO;
	stun_envia();          // meu código de resposta (pro anfitrião digitar se o furo não sair)
	manda_ola();
	return 1;
}
int rede_anfitriao_digita(const char *codigo)   // anfitrião digitou o código do convidado: fura dos dois lados
{
	u8 b[10];
	if (!decodifica(codigo, b, 10)) return 0;
	struct sockaddr_in a = {0}; a.sin_family = AF_INET; ncand = 0;
	memcpy(&a.sin_addr.s_addr, b, 4); memcpy(&a.sin_port, b + 4, 2); cand[ncand++] = a;
	memcpy(&a.sin_addr.s_addr, b + 6, 4); if (a.sin_addr.s_addr) cand[ncand++] = a;
	manda_ola();
	return 1;
}
void rede_desligar(void)
{
	if (rede_estado != REDE_DESLIGADA && par_ok) { u8 b[3] = {'K', 'N', T_TCHAU}; envia(b, 3); envia(b, 3); }
	if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
	porta_local = 0; rede_estado = REDE_DESLIGADA; rede_ativa = 0; par_ok = 0; ncand = 0; rede_escolha_pendente = -1;
}

void rede_poll(void)
{
	if (sock == INVALID_SOCKET) return;
	u8 b[1500]; struct sockaddr_in de; int dl;
	for (int k = 0; k < 64; k++) {
		dl = sizeof de;
		int n = recvfrom(sock, (char *)b, sizeof b, 0, (struct sockaddr *)&de, &dl);
		if (n <= 0) break;
		if (stun_fase == 1 && mesmo(&de, &stun_addr)) { stun_recebe(b, n); continue; }
		recebe(b, n, &de);
	}
	u32 ag = SDL_GetTicks();
	if (stun_fase == 1 && ag - t_stun > 800) { if (stun_tentativas >= 6) stun_fase = -1; else stun_envia(); }
	if (rede_estado == REDE_ERRO || rede_estado == REDE_DESLIGADA) return;
	if (!rede_ativa) {
		if (ag - t_ola >= OPEN_T) { t_ola = ag; manda_ola(); if (vi_par) manda_ping(); }
		if (rede_estado == REDE_PAREANDO && rede_anfitriao && par_me_ve && (pongs >= 4 || ag - t_pareou > 2500) && ag - t_inicio_envio >= 250) {
			if (rede_atraso_cfg > 0) rede_atraso = rede_atraso_cfg;
			else { rede_atraso = (rede_ping / 2 + 16) / 17 + 1; if (rede_atraso < 2) rede_atraso = 2; if (rede_atraso > 10) rede_atraso = 10; }
			manda_inicio(); t_inicio_envio = ag;
		}
		if (rede_estado == REDE_ENTRANDO && ag - t_ultimo_rx > 25000) { rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "Não achei o anfitrião (confira o código / tente o código de resposta)"); }
	} else {
		if (ag - t_ping >= 1000) { t_ping = ag; manda_ping(); }
		if (ag - t_envio >= 30) manda_entradas();
		if (ag - t_ultimo_rx > 10000) { rede_ativa = 0; rede_estado = REDE_ERRO; snprintf(rede_msg, sizeof rede_msg, "Conexão perdida"); extern void frontend_online_caiu(void); frontend_online_caiu(); }
	}
}

const char *rede_status(void)
{
	static char s[160];
	switch (rede_estado) {
	case REDE_CRIANDO:
		if (stun_fase == 1) return "Descobrindo o endereço público...";
		if (stun_fase == -1) return "Sem resposta do STUN (sem internet?). Dá pra usar só por IP (Radmin/ZeroTier/LAN).";
		snprintf(s, sizeof s, "Esperando o convidado...  (porta %d, UPnP: %s)", porta_local, rede_upnp == 1 ? "porta aberta" : rede_upnp == 0 ? "tentando" : "não abriu");
		return s;
	case REDE_ENTRANDO: return "Procurando o anfitrião...";
	case REDE_PAREANDO: snprintf(s, sizeof s, "Achei! Medindo o ping (%d ms)...", rede_ping); return s;
	case REDE_LIGADA: return rede_msg;
	case REDE_ERRO: return rede_msg;
	default: return "";
	}
}
int rede_porta_local(void) { return porta_local; }

// ---------------------------------------------------------------- lockstep
int rede_pode(void) { return rede_ativa && rem_tem_ate > quadro ? (int)(rem_tem_ate - quadro) : 0; }
int rede_esperando(void) { return (int)esperando_n; }
void rede_conta_espera(int parado) { esperando_n = parado ? esperando_n + 1 : 0; }
u32 rede_quadro(void) { return quadro; }
void rede_passo(u8 meu_p, u8 meu_x, u8 *p1, u8 *x1, u8 *p2, u8 *x2)
{
	u32 w = quadro + (u32)rede_atraso;
	if (rede_escolha_pendente >= 0 && rede_anfitriao) { meu_x |= REDE_X_MENU | (u8)(rede_escolha_pendente << 4); rede_escolha_pendente = -1; }
	loc_p[w % RING] = meu_p; loc_x[w % RING] = meu_x; loc_prox = w + 1;
	u8 lp = loc_p[quadro % RING], lx = loc_x[quadro % RING], rp = rem_p[quadro % RING], rx = rem_x[quadro % RING];
	if (rede_anfitriao) { *p1 = lp; *x1 = lx; *p2 = rp; *x2 = rx; } else { *p1 = rp; *x1 = rx; *p2 = lp; *x2 = lx; }
	quadro++;
	manda_entradas();
}
void rede_depois_quadro(void)
{
	if (!rede_ativa || quadro % 120 != 0) return;
	extern unsigned int m68k_read_memory_16(unsigned int);
	u32 h = 2166136261u;
	for (u32 a = 0x100000; a < 0x110000; a += 2) { h ^= m68k_read_memory_16(a); h *= 16777619u; }
	int i = (int)((quadro / 120) % 16);
	meu_crc_q[i] = quadro; meu_crc[i] = h; crc_envia_q = quadro; crc_envia = h;
}
