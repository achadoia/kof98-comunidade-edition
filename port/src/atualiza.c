// Auto-update pelo GitHub Releases (achadoia/kof98-comunidade-edition). Ver atualiza.h.
#include "atualiza.h"
#include "versao.h"
#include <windows.h>
#include <winhttp.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REPO_API L"/repos/achadoia/kof98-comunidade-edition/releases/latest"

int atualiza_estado, atualiza_adiada;
char atualiza_versao[32], atualiza_msg[160];
unsigned atualiza_bytes, atualiza_total;
static char url_exe[600];
static char arq_novo[MAX_PATH];

static int versao_num(const char *v)   // "1.6.0" -> 1*1000000 + 6*1000 + 0
{
	int a = 0, b = 0, c = 0;
	if (*v == 'v' || *v == 'V') v++;
	sscanf(v, "%d.%d.%d", &a, &b, &c);
	return a * 1000000 + b * 1000 + c;
}

static int json_texto(const char *json, const char *chave, char *out, int cap, const char *contem)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", chave);
	const char *p = json;
	for (;;) {
		p = strstr(p, k);
		if (!p) return 0;
		p += strlen(k);
		int i = 0;
		while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
		out[i] = 0;
		if (!contem || strstr(out, contem)) return i > 0;
	}
}

// GET https; 'arq' != NULL grava em arquivo (com progresso), senão devolve em buf
static int http_get(const wchar_t *host, const wchar_t *caminho, char *buf, int cap, const char *arq)
{
	int ok = 0;
	HINTERNET s = WinHttpOpen(L"KOF98CE/" KOF_VERSAO, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET c = s ? WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
	HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", caminho, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
	if (r) {
		DWORD to = 15000; WinHttpSetTimeouts(r, to, to, to, 60000);
		if (WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, NULL, 0, 0, 0) && WinHttpReceiveResponse(r, NULL)) {
			DWORD st = 0, tam = sizeof st;
			WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &st, &tam, WINHTTP_NO_HEADER_INDEX);
			DWORD total = 0; tam = sizeof total;
			WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &total, &tam, WINHTTP_NO_HEADER_INDEX);
			if (st == 200) {
				FILE *f = arq ? fopen(arq, "wb") : NULL;
				if (arq && !f) { snprintf(atualiza_msg, sizeof atualiza_msg, "Não consegui gravar ao lado do EXE (pasta protegida?)"); }
				else {
					static char pedaco[65536];
					DWORD n = 0; unsigned lido = 0; ok = 1;
					atualiza_total = total;
					for (;;) {
						if (!WinHttpReadData(r, pedaco, sizeof pedaco, &n)) { ok = 0; break; }
						if (!n) break;
						if (f) { if (fwrite(pedaco, 1, n, f) != n) { ok = 0; break; } }
						else { if ((int)(lido + n) >= cap) n = (DWORD)(cap - 1 - lido); memcpy(buf + lido, pedaco, n); }
						lido += n; atualiza_bytes = lido;
						if (!f && (int)lido >= cap - 1) break;
					}
					if (f) fclose(f); else buf[lido] = 0;
					if (ok && total && lido != total) ok = 0;
				}
			} else snprintf(atualiza_msg, sizeof atualiza_msg, "GitHub respondeu %lu", (unsigned long)st);
		} else snprintf(atualiza_msg, sizeof atualiza_msg, "Sem resposta do GitHub (sem internet?)");
	}
	if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
	return ok;
}

static int checa_thread(void *arg)
{
	(void)arg;
	static char json[65536];
	if (!http_get(L"api.github.com", REPO_API, json, sizeof json, NULL)) { atualiza_estado = ATU_ERRO; return 0; }
	char tag[32];
	if (!json_texto(json, "tag_name", tag, sizeof tag, NULL) || !json_texto(json, "browser_download_url", url_exe, sizeof url_exe, ".exe")) { atualiza_estado = ATU_ERRO; snprintf(atualiza_msg, sizeof atualiza_msg, "Release sem EXE"); return 0; }
	snprintf(atualiza_versao, sizeof atualiza_versao, "%s", tag[0] == 'v' ? tag + 1 : tag);
	const char *fake = getenv("KOF_VERSAO_FAKE");          // teste: finge ser uma versão antiga
	atualiza_estado = versao_num(tag) > versao_num(fake ? fake : KOF_VERSAO) ? ATU_NOVA : ATU_ATUAL;
	return 0;
}
void atualiza_checa(void)
{
	if (atualiza_estado != ATU_NADA) return;
	atualiza_estado = ATU_CHECANDO;
	SDL_DetachThread(SDL_CreateThread(checa_thread, "atualiza", NULL));
}

static int baixa_thread(void *arg)
{
	(void)arg;
	// url: https://github.com/.../releases/download/vX/arquivo.exe (o GitHub redireciona; o WinHTTP segue)
	const char *u = url_exe;
	if (!strncmp(u, "https://", 8)) u += 8;
	const char *cam = strchr(u, '/');
	wchar_t host[128], caminho[600];
	if (!cam) { atualiza_estado = ATU_ERRO; return 0; }
	MultiByteToWideChar(CP_UTF8, 0, u, (int)(cam - u), host, 127); host[cam - u] = 0;
	MultiByteToWideChar(CP_UTF8, 0, cam, -1, caminho, 599); caminho[599] = 0;
	char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
	snprintf(arq_novo, sizeof arq_novo, "%s.novo", exe);
	if (!http_get(host, caminho, NULL, 0, arq_novo)) { remove(arq_novo); atualiza_estado = ATU_ERRO; if (!atualiza_msg[0]) snprintf(atualiza_msg, sizeof atualiza_msg, "Download falhou"); return 0; }
	atualiza_estado = ATU_PRONTA;
	return 0;
}
void atualiza_baixar(void)
{
	if (atualiza_estado != ATU_NOVA) return;
	atualiza_estado = ATU_BAIXANDO; atualiza_bytes = atualiza_total = 0; atualiza_msg[0] = 0;
	SDL_DetachThread(SDL_CreateThread(baixa_thread, "baixa", NULL));
}

// .bat que espera este processo fechar, troca o EXE pelo novo e abre o jogo de novo
int atualiza_aplicar(void)
{
	if (atualiza_estado != ATU_PRONTA) return 0;
	char exe[MAX_PATH], bat[MAX_PATH], pasta[MAX_PATH];
	GetModuleFileNameA(NULL, exe, MAX_PATH);
	snprintf(pasta, sizeof pasta, "%s", exe);
	char *b = strrchr(pasta, '\\'); if (b) *b = 0;
	snprintf(bat, sizeof bat, "%s\\atualizar_kof98.bat", pasta);
	FILE *f = fopen(bat, "w");
	if (!f) { snprintf(atualiza_msg, sizeof atualiza_msg, "Não consegui gravar o atualizador"); atualiza_estado = ATU_ERRO; return 0; }
	fprintf(f, "@echo off\r\ncd /d \"%s\"\r\n:espera\r\ntimeout /t 1 /nobreak >nul\r\ntasklist /FI \"PID eq %lu\" 2>nul | find \"%lu\" >nul && goto espera\r\n"
	           "move /y \"%s\" \"%s\" >nul\r\nif errorlevel 1 (timeout /t 2 /nobreak >nul & move /y \"%s\" \"%s\" >nul)\r\nstart \"\" \"%s\"\r\ndel \"%%~f0\"\r\n",
	        pasta, (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentProcessId(), arq_novo, exe, arq_novo, exe, exe);
	fclose(f);
	char cmd[MAX_PATH + 32]; snprintf(cmd, sizeof cmd, "cmd.exe /c \"%s\"", bat);
	STARTUPINFOA si = {0}; si.cb = sizeof si; PROCESS_INFORMATION pi = {0};
	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, pasta, &si, &pi)) { snprintf(atualiza_msg, sizeof atualiza_msg, "Não consegui abrir o atualizador"); atualiza_estado = ATU_ERRO; return 0; }
	CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
	return 1;
}
