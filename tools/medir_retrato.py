"""Mede um ícone/retrato na tela pra port/arte/retratos.json: acha a borda branca em volta de um ponto e
lista os tiles (código -> origem x, y) que pintam a parte de dentro. Precisa de uma captura do build de teste:
  KOF_MAPA="quadro,mapa.bin" kof98_dev.exe --sem-janela --quadros quadro ... --foto foto.bmp

Uso: python tools/medir_retrato.py mapa.bin foto.bmp x y ["onde"]
Imprime o bloco JSON do alvo. Recusa tile espelhado/ampliado (a origem não bate pixel a pixel).
"""
import json
import sys

import numpy as np
from PIL import Image


def borda(branco, x, y, dx, dy, lim=60):
    """anda de (x, y) até a primeira linha/coluna quase toda branca (a moldura do ícone)"""
    for k in range(1, lim):
        if dx:
            cx = x + dx * k
            seg = branco[max(0, y - 10):y + 11, cx]
        else:
            cy = y + dy * k
            seg = branco[cy, max(0, x - 10):x + 11]
        if seg.mean() >= 0.8:
            return x + dx * k if dx else y + dy * k
    raise SystemExit("não achei a borda branca")


def main():
    mapa, foto, x, y = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    onde = sys.argv[5] if len(sys.argv) > 5 else "?"
    m = np.fromfile(mapa, np.uint32).reshape(224, 320)
    im = np.array(Image.open(foto).convert("RGB")).astype(int)
    branco = im.min(2) > 200
    x0, x1 = borda(branco, x, y, -1, 0) + 1, borda(branco, x, y, 1, 0) - 1
    y0, y1 = borda(branco, x, y, 0, -1) + 1, borda(branco, x, y, 0, 1) - 1
    origens = {}
    for sy in range(y0, y1 + 1):
        for sx in range(x0, x1 + 1):
            v = int(m[sy, sx])
            if v == 0xFFFFFFFF:
                continue
            code, ly, cx = v >> 8, (v >> 4) & 15, v & 15
            o = (sx - cx, sy - ly)
            if origens.setdefault(code, o) != o:
                raise SystemExit(f"tile {code:x} espelhado ou ampliado nesta captura")
    alvo = {"onde": onde, "area": [x0, y0, x1, y1], "tiles": {f"{c:x}": list(o) for c, o in sorted(origens.items())}}
    print(json.dumps(alvo, ensure_ascii=False))


if __name__ == "__main__":
    main()
