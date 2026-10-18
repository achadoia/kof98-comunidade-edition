// Carrega as ROMs do KOF98 (jogo + pai kof98 + BIOS neogeo) direto dos .zip do usuário.
// Os arquivos são achados pela extensão (.p1, .sp2, .c1...), então serve pros nomes do MAME,
// do FBNeo (romdata kof98aemod.zip) e do Frame Perfect.
#include "neo.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Roms R;

typedef struct {
	mz_zip_archive z[8];
	int n;
} Zips;

static int termina_com(const char *nome, const char *ext)
{
	size_t a = strlen(nome), b = strlen(ext);
	if (a < b) return 0;
	for (size_t i = 0; i < b; i++)
		if (tolower((unsigned char)nome[a - b + i]) != tolower((unsigned char)ext[i])) return 0;
	return 1;
}

static int base_igual(const char *nome, const char *alvo)
{
	const char *b = strrchr(nome, '/');
	b = b ? b + 1 : nome;
	for (; *b && *alvo; b++, alvo++)
		if (tolower((unsigned char)*b) != tolower((unsigned char)*alvo)) return 0;
	return *b == 0 && *alvo == 0;
}

// procura um arquivo por extensão (ou nome exato se exato=1) nos zips. Com o kof98.zip original ao lado
// (o Anniversary é um "clone": pega c3/c4 e v1-v4 do jogo pai), o mesmo ".c1" existe nos dois zips:
// o do hack (242ae-*) tem prioridade.
static u8 *ler(Zips *zs, const char *alvo, int exato, size_t *tam)
{
	int zi = -1; mz_uint fi = 0; int prio = -1;
	for (int i = 0; i < zs->n; i++) {
		mz_uint nf = mz_zip_reader_get_num_files(&zs->z[i]);
		for (mz_uint f = 0; f < nf; f++) {
			char nome[256];
			mz_zip_reader_get_filename(&zs->z[i], f, nome, sizeof nome);
			int ok = exato ? base_igual(nome, alvo) : termina_com(nome, alvo);
			if (!ok) continue;
			const char *b = strrchr(nome, '/');
			b = b ? b + 1 : nome;
			int p = (tolower((unsigned char)b[0]) == '2' && b[1] == '4' && b[2] == '2' && tolower((unsigned char)b[3]) == 'a' && tolower((unsigned char)b[4]) == 'e') ? 1 : 0;
			if (p > prio) { prio = p; zi = i; fi = f; }
		}
	}
	if (zi >= 0) {
		void *p = mz_zip_reader_extract_to_heap(&zs->z[zi], fi, tam, 0);
		if (p) return p;
	}
	*tam = 0;
	return NULL;
}

static void troca_bytes(u8 *p, size_t n)
{
	for (size_t i = 0; i + 1 < n; i += 2) { u8 t = p[i]; p[i] = p[i + 1]; p[i + 1] = t; }
}

#ifdef KOF_SOLO
// Versão de arquivo único: os patches (tools/embutir.py) são aplicados no programa original do
// kof98ae2016.zip ao carregar, igual ao tools/gerar_rom.py. Se o zip já for o modificado, nada muda.
extern const unsigned char embutido_patches[];
extern const size_t embutido_patches_tam;
static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void aplica_patches(void)
{
	const u8 *b = embutido_patches;
	size_t i = 0;
	int aplicados = 0, iguais = 0;
	while (i + 8 <= embutido_patches_tam) {
		u32 o = le32(b + i), n = le32(b + i + 4);
		i += 8;
		if (o + n <= R.p_size) {
			if (!memcmp(R.p + o, b + i, n)) iguais++;
			else { memcpy(R.p + o, b + i, n); aplicados++; }
		}
		i += n;
	}
	// proteção do hack (0x09D8E6): a soma das words do 1º MB tem que dar 7; compensa na mesma word do gerar_rom
	u32 soma = 0;
	for (u32 k = 0; k + 1 < 0x100000 && k + 1 < R.p_size; k += 2) soma += (u32)R.p[k] << 8 | R.p[k + 1];
	if ((soma & 0xffff) != 7 && aplicados) {
		const u32 c = 0xA7E00;
		u16 atual = (u16)(R.p[c] << 8 | R.p[c + 1]), novo = (u16)(atual + 7 - (soma & 0xffff));
		R.p[c] = (u8)(novo >> 8); R.p[c + 1] = (u8)novo;
	}
	fprintf(stderr, "patches: %d aplicados, %d já estavam\n", aplicados, iguais);
}
#endif

static u32 pot2(u32 v)
{
	u32 r = 1;
	while (r < v) r <<= 1;
	return r;
}

int roms_load(const char *const *zips, int nzips, int aes, char *erro, size_t erro_tam)
{
	Zips zs;
	memset(&zs, 0, sizeof zs);
	for (int i = 0; i < nzips && zs.n < 8; i++) {
		if (mz_zip_reader_init_file(&zs.z[zs.n], zips[i], 0)) zs.n++;
	}
	if (!zs.n) { snprintf(erro, erro_tam, "nenhum zip encontrado"); return 1; }

	size_t t1, t2;
	u8 *p1 = ler(&zs, ".p1", 0, &t1);
	u8 *p2 = ler(&zs, ".sp2", 0, &t2);
	if (!p2) p2 = ler(&zs, ".p2", 0, &t2);
	if (!p1) { snprintf(erro, erro_tam, "programa (.p1) não encontrado"); return 1; }
	R.p_size = (u32)(0x100000 + (p2 ? t2 : 0));
	R.p = calloc(1, R.p_size);
	memcpy(R.p, p1, t1 < 0x100000 ? t1 : 0x100000);
	if (p2) memcpy(R.p + 0x100000, p2, t2);
	troca_bytes(R.p, R.p_size);
	free(p1); free(p2);
#ifdef KOF_SOLO
	aplica_patches();
#endif

	size_t tb;
	const char *nome_bios = aes ? "neo-epo.bin" : "sp-s2.sp1";   // AES Ásia (console) / MVS Europa
	R.bios = ler(&zs, nome_bios, 1, &tb);
	if (!R.bios) { snprintf(erro, erro_tam, "BIOS %s não encontrada (neogeo.zip)", nome_bios); return 1; }
	troca_bytes(R.bios, tb);
	R.sfix = ler(&zs, "sfix.sfix", 1, &tb);
	R.sm1 = ler(&zs, "sm1.sm1", 1, &tb);
	R.zoom = ler(&zs, "000-lo.lo", 1, &tb);
	if (!R.sfix || !R.sm1 || !R.zoom) { snprintf(erro, erro_tam, "arquivos da BIOS faltando (sfix/sm1/000-lo)"); return 1; }

	size_t ts;
	R.s1 = ler(&zs, ".s1", 0, &ts);
	R.s1_size = (u32)ts;
	size_t tm;
	R.m1 = ler(&zs, ".m1", 0, &tm);
	R.m1_size = (u32)tm;
	if (!R.s1 || !R.m1) { snprintf(erro, erro_tam, "S1/M1 não encontrados"); return 1; }

	// V ROMs: v1..v8, cada uma na posição do seu índice (faltando uma, as outras não podem escorregar:
	// o chip tocaria a amostra errada)
	u8 *vs[8] = {0};
	size_t vt[8] = {0}, vmax = 0;
	int nv = 0;
	char faltam[400] = "";
	for (int i = 0; i < 8; i++) {
		char ext[8];
		snprintf(ext, sizeof ext, ".v%d", i + 1);
		vs[i] = ler(&zs, ext, 0, &vt[i]);
		if (vs[i]) { nv = i + 1; if (vt[i] > vmax) vmax = vt[i]; }
	}
	for (int i = 0; i < nv; i++) if (!vs[i]) { char t[16]; snprintf(t, sizeof t, " v%d", i + 1); strncat(faltam, t, sizeof faltam - strlen(faltam) - 1); }
	size_t vtot = 0;
	for (int i = 0; i < nv; i++) vtot += vs[i] ? vt[i] : vmax;
	R.vrom_size = (u32)vtot;
	R.vrom = calloc(1, vtot ? vtot : 1);
	for (size_t i = 0, o = 0; i < (size_t)nv; i++) { if (vs[i]) { memcpy(R.vrom + o, vs[i], vt[i]); o += vt[i]; free(vs[i]); } else o += vmax; }

	// C ROMs: pares (c1,c2), (c3,c4)... intercalados byte a byte (load16_byte)
	u8 *cs[16] = {0};
	size_t ct[16] = {0}, cmax = 0;
	int npares = 0;
	for (int i = 0; i < 16; i++) {
		char ext[8];
		snprintf(ext, sizeof ext, ".c%d", i + 1);
		cs[i] = ler(&zs, ext, 0, &ct[i]);
		if (cs[i]) { if (ct[i] > cmax) cmax = ct[i]; npares = i / 2 + 1; }
	}
	if (!npares) { snprintf(erro, erro_tam, "sprites (.c1...) não encontrados"); return 1; }
	for (int i = 0; i < npares * 2; i++) if (!cs[i]) { char t[16]; snprintf(t, sizeof t, " c%d", i + 1); strncat(faltam, t, sizeof faltam - strlen(faltam) - 1); }
	if (faltam[0]) {   // set incompleto: personagens invisíveis e sons trocados (ex.: zip do Anniversary sem os arquivos do kof98 original)
		snprintf(erro, erro_tam, "faltam arquivos da ROM:%s (o kof98ae2016.zip usa c3, c4 e v1-v4 do kof98.zip original)", faltam);
		return 1;
	}
	u32 spr_tam = (u32)(npares * 2 * cmax);
	u32 buf = pot2(spr_tam);
	R.spr = calloc(1, buf);
	R.spr_mask = buf - 1;
	for (int i = 0; i < 16; i++) {
		if (!cs[i]) continue;
		u8 *dst = R.spr + (size_t)(i / 2) * 2 * cmax + (i & 1);
		for (size_t k = 0; k < ct[i]; k++) dst[k * 2] = cs[i][k];
		free(cs[i]);
	}

	for (int i = 0; i < zs.n; i++) mz_zip_reader_end(&zs.z[i]);
	fprintf(stderr, "ROMs: programa %u KB, sprites %u MB (%d pares), V %u MB, S1 %u KB, M1 %u KB\n",
		R.p_size >> 10, spr_tam >> 20, npares, R.vrom_size >> 20, R.s1_size >> 10, R.m1_size >> 10);
	return 0;
}
