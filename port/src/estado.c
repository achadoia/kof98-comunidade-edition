// Savestate: cada módulo serializa suas variáveis por est_bloco (mesma ordem pra salvar e carregar).
#include "neo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESTADO_MAGICO 0x4b4f4639u   // "KOF9"
#define ESTADO_VERSAO 2

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
	if (!e->salvar && (magico != ESTADO_MAGICO || versao != ESTADO_VERSAO)) { e->erro = 1; return; }
	neo_estado(e);
	video_estado(e);
	audio_estado(e);
}

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
