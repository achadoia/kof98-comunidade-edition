// Savestate: cada módulo serializa suas variáveis por est_bloco (mesma ordem pra salvar e carregar).
#include "neo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESTADO_MAGICO 0x4b4f4639u   // "KOF9"
#define ESTADO_VERSAO 3
int estado_leve;
int estado_versao_lida = ESTADO_VERSAO;   // 2 = arquivo antigo (sem para_n/para_p na música)

void est_bloco(Estado *e, void *p, size_t n)
{
	if (e->salvar) {
		if (e->pos + n > e->cap) {
			size_t nc = e->cap ? e->cap * 2 : 1 << 20;
			while (nc < e->pos + n) nc *= 2;
			e->buf = realloc(e->buf, nc);
			e->cap = nc;
		}
		memcpy(e->buf + e->pos, p, n);
	} else {
		if (e->pos + n > e->cap) { e->erro = 1; return; }
		memcpy(p, e->buf + e->pos, n);
	}
	e->pos += n;
}

static void tudo(Estado *e)
{
	u32 magico = ESTADO_MAGICO, versao = ESTADO_VERSAO;
	EST(e, magico); EST(e, versao);
	if (!e->salvar && (magico != ESTADO_MAGICO || versao < 2 || versao > ESTADO_VERSAO)) { e->erro = 1; return; }
	estado_versao_lida = e->salvar ? ESTADO_VERSAO : (int)versao;
	neo_estado(e);
	video_estado(e);
	audio_estado(e);
}

// run-ahead: snapshot na memória, sem arquivo. O buffer fica alocado entre quadros.
static Estado mem;
void estado_mem_salvar(void)
{
	mem.pos = 0; mem.salvar = 1; mem.erro = 0;
	tudo(&mem);
}
size_t estado_mem_tam(void) { return mem.pos; }
void estado_mem_carregar(void)
{
	size_t cap = mem.cap, tam = mem.pos;
	mem.cap = tam; mem.pos = 0; mem.salvar = 0; mem.erro = 0;
	tudo(&mem);
	mem.cap = cap;
}

// rollback online: estado no início de cada quadro, num anel (o mais velho é sobrescrito)
#define RING_EST 16
static Estado anel[RING_EST]; static u32 anel_q[RING_EST]; static int anel_ok[RING_EST];
void estado_ring_salvar(u32 q)
{
	Estado *e = &anel[q % RING_EST];
	int leve = estado_leve;
	estado_leve = 1;                     // o anel é sempre leve (salvar e carregar precisam do mesmo formato)
	e->pos = 0; e->salvar = 1; e->erro = 0;
	tudo(e);
	estado_leve = leve;
	anel_q[q % RING_EST] = q; anel_ok[q % RING_EST] = 1;
}
int estado_ring_carregar(u32 q)
{
	int i = (int)(q % RING_EST);
	if (!anel_ok[i] || anel_q[i] != q) return 1;
	Estado *e = &anel[i];
	int leve = estado_leve;
	estado_leve = 1;                     // (bug até 20/10: carregava sem leve, o bloco da música não batia e musica_reset() zerava o prefixo de voz e a fila do Z80 = dessincronia)
	size_t cap = e->cap, tam = e->pos;
	e->cap = tam; e->pos = 0; e->salvar = 0; e->erro = 0;
	tudo(e);
	e->cap = cap;
	estado_leve = leve;
	return e->erro;
}
void estado_ring_limpa(void) { memset(anel_ok, 0, sizeof anel_ok); }

int estado_salvar(const char *arq)
{
	Estado e = {0};
	e.salvar = 1;
	tudo(&e);
	FILE *f = fopen(arq, "wb");
	if (!f) { free(e.buf); return 1; }
	fwrite(e.buf, 1, e.pos, f);
	fclose(f);
	free(e.buf);
	return 0;
}

int estado_carregar(const char *arq)
{
	FILE *f = fopen(arq, "rb");
	if (!f) return 1;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	Estado e = {0};
	e.buf = malloc((size_t)n);
	e.cap = (size_t)fread(e.buf, 1, (size_t)n, f);
	fclose(f);
	tudo(&e);
	free(e.buf);
	return e.erro;
}
