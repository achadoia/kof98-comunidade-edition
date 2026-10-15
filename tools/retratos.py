"""Gera bin/arte/troca_tiles.bin a partir de port/arte/retratos.json: para cada tile listado, 16x16 pixels
ARGB tirados da imagem do retrato, já no tamanho do ícone no jogo.

Pixel ARGB: alfa 0 = o EXE usa o pixel original do tile (borda branca, cursor...); alfa 1 = transparente
(some o pixel original, usado no rosto do HUD, que é recortado); outro alfa = essa cor.

Nitidez: os ícones originais têm 26x27 px e 15 cores, contorno escuro. A imagem é reduzida por média de área
(não borra como Lanczos), ganha saturação/contraste/realce de borda e vai pra paleta de N cores sem pontilhado.
Opções por alvo: "recorte" [x0,y0,x1,y1] na imagem original, "espelhar" (vira pra esquerda),
"sem_fundo" (tira o laranja), "cores" (padrão 32).

Formato: repetido { u32 código; u32 pixels[16*16] }, little-endian.
Uso: python tools/retratos.py   (grava build/retratos_previa.png com cada alvo ampliado 6x)
"""
import json
import os
import struct

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ARTE = os.path.join(RAIZ, "port", "arte")


def processa(img, alvo, w, h):
    """recorta, reduz e deixa nítido; devolve (RGB w x h, máscara de fundo w x h)"""
    rec = img.crop(tuple(alvo["recorte"]))
    if alvo.get("espelhar"):
        rec = rec.transpose(Image.FLIP_LEFT_RIGHT)
    # fundo laranja do card (~#FA8202): só o que está ligado às bordas do recorte (a pele também é alaranjada)
    marca = rec.copy()
    for canto in ((0, 0), (rec.width - 1, 0), (0, rec.height - 1), (rec.width - 1, rec.height - 1),
                  (rec.width // 2, 0), (0, rec.height // 3), (rec.width - 1, rec.height // 3)):
        c = rec.getpixel(canto)
        if c[0] > 220 and 90 < c[1] < 170 and c[2] < 40:
            ImageDraw.floodfill(marca, canto, (255, 0, 255), thresh=70)
    laranja = np.all(np.asarray(marca) == (255, 0, 255), axis=2)
    peq = rec.resize((w, h), Image.BOX)
    fundo = np.asarray(Image.fromarray((laranja * 255).astype(np.uint8)).resize((w, h), Image.BOX)) > 140
    peq = ImageEnhance.Color(peq).enhance(1.1)
    peq = ImageEnhance.Contrast(peq).enhance(1.12)
    peq = peq.filter(ImageFilter.UnsharpMask(radius=1, percent=90, threshold=2))
    peq = peq.quantize(colors=alvo.get("cores", 32), method=Image.MEDIANCUT, dither=Image.NONE).convert("RGB")
    return peq, fundo


def main():
    spec = json.load(open(os.path.join(ARTE, "retratos.json"), encoding="utf-8"))
    saida = bytearray()
    previas = []
    for r in spec["retratos"]:
        img_padrao = Image.open(os.path.join(ARTE, r["imagem"])).convert("RGB")
        for alvo in r["alvos"]:
            img = Image.open(os.path.join(ARTE, alvo["imagem"])).convert("RGB") if alvo.get("imagem") else img_padrao
            x0, y0, x1, y1 = alvo["area"]
            w, h = x1 - x0 + 1, y1 - y0 + 1
            peq, fundo = processa(img, alvo, w, h)
            sem_fundo = alvo.get("sem_fundo")
            prev = Image.new("RGB", (w, h), (40, 40, 60))
            for cod, (ox, oy) in alvo["tiles"].items():
                px = []
                for ty in range(16):
                    for tx in range(16):
                        sx, sy = ox + tx, oy + ty
                        if x0 <= sx <= x1 and y0 <= sy <= y1:
                            if sem_fundo and fundo[sy - y0, sx - x0]:
                                px.append(0x01000000)
                            else:
                                cr, cg, cb = peq.getpixel((sx - x0, sy - y0))
                                px.append(0xff000000 | cr << 16 | cg << 8 | cb)
                                prev.putpixel((sx - x0, sy - y0), (cr, cg, cb))
                        else:
                            px.append(0x01000000 if sem_fundo else 0)
                saida += struct.pack("<I256I", int(cod, 16), *px)
            previas.append(prev)
            print(f"{r['nome']}: {alvo['onde']} ({w}x{h} px, {len(alvo['tiles'])} tiles)")
    destino = os.path.join(RAIZ, "port", "bin", "arte", "troca_tiles.bin")
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    open(destino, "wb").write(saida)
    print("->", os.path.relpath(destino, RAIZ), len(saida) // 1028, "tiles")
    tela = Image.new("RGB", (sum(p.width * 6 + 6 for p in previas), max(p.height for p in previas) * 6), "black")
    x = 0
    for p in previas:
        tela.paste(p.resize((p.width * 6, p.height * 6), Image.NEAREST), (x, 0))
        x += p.width * 6 + 6
    tela.save(os.path.join(RAIZ, "build", "retratos_previa.png"))


if __name__ == "__main__":
    main()
