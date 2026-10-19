"""Fotos dos homenageados em alta resolução (18/10): gera port/bin/arte/overlay.bin (tiles -> alvo, com o deslocamento
de cada tile dentro da arte) e port/bin/arte/fotos.bin (PNG da foto já recortada, no tamanho da arte x ESCALA) a partir
de port/arte/retratos.json. O EXE anota onde cada arte foi pintada e desenha a foto por cima, no tamanho da janela.

Formatos (little-endian):
  overlay.bin: u32 n_alvos; por alvo: u32 n_codes; n_codes x { u32 code; s16 dx; s16 dy }   (dx,dy = tile dentro da arte)
  fotos.bin:   u32 n; por foto (mesma ordem dos alvos): u32 tam_png; u16 w; u16 h; u8 espelhar; u8 0; png
Uso: python tools/fotos.py
"""
import io
import json
import os
import struct

from PIL import Image

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ARTE = os.path.join(RAIZ, "port", "arte")
ESCALA = 8


def main():
    spec = json.load(open(os.path.join(ARTE, "retratos.json"), encoding="utf-8"))
    ovl = bytearray()
    fot = bytearray()
    alvos = []
    for r in spec["retratos"]:
        img_padrao = Image.open(os.path.join(ARTE, r["imagem"])).convert("RGB")
        for alvo in r["alvos"]:
            if alvo.get("sem_foto"):
                continue
            x0, y0, x1, y1 = alvo["area"]
            w, h = x1 - x0 + 1, y1 - y0 + 1
            flags = 1 if alvo.get("espelhar") else 0
            if alvo.get("foto_sem_fundo"):
                rec = Image.open(os.path.join(ARTE, alvo["foto_sem_fundo"])).convert("RGBA")
                rec = rec.crop(rec.getbbox())                       # só a cabeça
                flags |= 2                                          # caixa inteira, sem recorte da parte visível
                # encaixa na proporção da arte (w x h), centralizado embaixo (a cabeça apoia na base da caixa)
                esc = min(w * ESCALA / rec.width, h * ESCALA / rec.height)
                rec = rec.resize((max(1, int(rec.width * esc)), max(1, int(rec.height * esc))), Image.LANCZOS)
                peq = Image.new("RGBA", (w * ESCALA, h * ESCALA), (0, 0, 0, 0))
                peq.paste(rec, ((w * ESCALA - rec.width) // 2, h * ESCALA - rec.height), rec)
            else:
                img = Image.open(os.path.join(ARTE, alvo["imagem"])).convert("RGB") if alvo.get("imagem") else img_padrao
                rec = img.crop(tuple(alvo["recorte"]))
                peq = rec.resize((w * ESCALA, h * ESCALA), Image.LANCZOS)
            buf = io.BytesIO()
            peq.save(buf, format="PNG", optimize=True)
            png = buf.getvalue()
            fot += struct.pack("<IHHBB", len(png), w, h, flags, 0) + png
            tiles = alvo["tiles"]
            ovl += struct.pack("<I", len(tiles))
            for cod, (ox, oy) in tiles.items():
                ovl += struct.pack("<Ihh", int(cod, 16), ox - x0, oy - y0)
            alvos.append((r["nome"], alvo["onde"], w, h, len(tiles), len(png)))
    saida = os.path.join(RAIZ, "port", "bin", "arte")
    os.makedirs(saida, exist_ok=True)
    open(os.path.join(saida, "overlay.bin"), "wb").write(struct.pack("<I", len(alvos)) + ovl)
    open(os.path.join(saida, "fotos.bin"), "wb").write(struct.pack("<I", len(alvos)) + fot)
    for a in alvos:
        print("%-16s %-40s %3dx%-3d %2d tiles  png %6d B" % a)
    print("-> overlay.bin %d B, fotos.bin %d B, %d alvos" % (4 + len(ovl), 4 + len(fot), len(alvos)))


if __name__ == "__main__":
    main()
