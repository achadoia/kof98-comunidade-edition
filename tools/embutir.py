"""Gera port/src/embutido.c com tudo que o EXE de arquivo único leva dentro:
  - arte do menu (port/arte/menu_fundo.png, decodificada no EXE pelo stb_image) e dos menus F1/F6 (port/arte/hd_*.png)
  - retratos (port/bin/arte/troca_tiles.bin, saído de tools/retratos.py)
  - patches da ROM (build/patches/<nome>.json da lista abaixo): o EXE aplica no kof98ae2016.zip original
    ao carregar, igual ao tools/gerar_rom.py (variante ae), inclusive a compensação do checksum do hack.
Nada de ROM, som ou arquivo da SNK entra aqui.

Uso: python tools/embutir.py
"""
import json
import os

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PATCHES = ["chizuru_ae", "inbloqueavel_ae", "extra_pulinho_ae", "levantar_ae", "frota_ae",
           "nome_terry_ae", "nome_daimon_ae", "nome_saisyu_ae", "nome_iori_ae", "nome_ralf_ae", "nome_chizuru_ae", "cenario_carros_ae", "hud_terry_ae"]


def arquivo(addr):
    """endereço do 68000 -> offset no programa linear (p1 1 MB + p2; banco 1 do p2 na janela 0x200000)"""
    if addr < 0x100000:
        return addr
    if 0x200000 <= addr < 0x300000:
        return 0x200000 + (addr - 0x200000)
    raise ValueError(f"endereço fora da ROM: {addr:#x}")


def bytes_c(dados, por_linha=24):
    linhas = []
    for i in range(0, len(dados), por_linha):
        linhas.append("\t" + ",".join(str(b) for b in dados[i:i + por_linha]) + ",")
    return "\n".join(linhas)


def main():
    png = open(os.path.join(RAIZ, "port", "arte", "menu_fundo.png"), "rb").read()
    tiles = open(os.path.join(RAIZ, "port", "bin", "arte", "troca_tiles.bin"), "rb").read()
    icone = open(os.path.join(RAIZ, "port", "arte", "icone_64.png"), "rb").read()
    fotos = open(os.path.join(RAIZ, "port", "bin", "arte", "fotos.bin"), "rb").read()
    overlay = open(os.path.join(RAIZ, "port", "bin", "arte", "overlay.bin"), "rb").read()
    gcdb = open(os.path.join(RAIZ, "port", "controles", "gamecontrollerdb.txt"), "rb").read()
    hd = {n: open(os.path.join(RAIZ, "port", "arte", a), "rb").read() for n, a in
          [("hd_cfg_png", "hd_cfg_fundo.png"), ("hd_treino_png", "hd_treino_titulo.png"), ("hd_dpad_png", "hd_dpad.png"), ("hd_b_png", "hd_botao_b.png")]}
    patches = []
    dono = {}
    for nome in PATCHES:
        for addr, dados in json.load(open(os.path.join(RAIZ, "build", "patches", nome + ".json"))):
            o = arquivo(addr)
            for k in range(len(dados)):
                if dono.get(o + k, nome) != nome:
                    raise SystemExit(f"patches {dono[o + k]} e {nome} se sobrepõem em {o + k:#x}")
                dono[o + k] = nome
            patches.append((o, bytes(dados)))
    patches.sort()
    blob = bytearray()
    for o, dados in patches:                      # u32 offset, u32 tamanho, bytes
        blob += o.to_bytes(4, "little") + len(dados).to_bytes(4, "little") + dados
    with open(os.path.join(RAIZ, "port", "src", "embutido.c"), "w", encoding="utf-8") as f:
        f.write("// Gerado por tools/embutir.py: arte do menu, retratos e patches da ROM dentro do EXE (versão de arquivo único).\n")
        f.write("#include <stddef.h>\n")
        f.write(f"const unsigned char embutido_menu_png[] = {{\n{bytes_c(png)}\n}};\nconst size_t embutido_menu_png_tam = {len(png)};\n")
        f.write(f"const unsigned char embutido_tiles[] = {{\n{bytes_c(tiles)}\n}};\nconst size_t embutido_tiles_tam = {len(tiles)};\n")
        f.write(f"const unsigned char embutido_icone_png[] = {{\n{bytes_c(icone)}\n}};\nconst size_t embutido_icone_png_tam = {len(icone)};\n")
        f.write(f"const unsigned char embutido_patches[] = {{\n{bytes_c(blob)}\n}};\nconst size_t embutido_patches_tam = {len(blob)};\n")
        f.write(f"const unsigned char embutido_fotos[] = {{\n{bytes_c(fotos)}\n}};\nconst size_t embutido_fotos_tam = {len(fotos)};\n")
        f.write(f"const unsigned char embutido_overlay[] = {{\n{bytes_c(overlay)}\n}};\nconst size_t embutido_overlay_tam = {len(overlay)};\n")
        f.write(f"const unsigned char embutido_gcdb[] = {{\n{bytes_c(gcdb)}\n}};\nconst size_t embutido_gcdb_tam = {len(gcdb)};\n")
        for n, d in hd.items():    # artes dos menus F1/F6 (src/hd.c)
            f.write(f"const unsigned char embutido_{n}[] = {{\n{bytes_c(d)}\n}};\nconst size_t embutido_{n}_tam = {len(d)};\n")
    print(f"embutido.c: menu {len(png)} B, retratos {len(tiles)} B, fotos {len(fotos)} B, overlay {len(overlay)} B, {len(patches)} patches ({len(blob)} B)")


if __name__ == "__main__":
    main()
