"""Troca o nome curto do lutador no HUD da luta (texto em minúsculas na camada fixa: tiles 0x300 + ASCII,
P1 à esquerda, P2 à direita) no KOF98 Anniversary (KOF_VARIANTE=ae). Grava build/patches/hud_<arquivo>_ae.json.

Tabela em 0x0B3E52 (ponteiros de 4 bytes), registros "\\x00\\x00\\x03" + nome + "\\xff" a partir de 0x0B3EEB
("kyo", "benimaru", "daimon", "terry", ...). Nome maior que o original vai pra CAVE_HUD e o ponteiro é trocado.

Uso: python tools/renomear_hud.py terry uraterry [arquivo]
"""
import json
import os
import re
import struct
import sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CAVE_HUD = 0x0A7A00            # área livre (0xFF): 0xA7A00-0xA7BFF (nomes completos usam 0xA7C00-0xA7DFF)


def main():
    antigo, novo = sys.argv[1].lower().encode(), sys.argv[2].lower().encode()
    arquivo = sys.argv[3] if len(sys.argv) > 3 else antigo.decode()
    if len(novo) > 12:
        raise SystemExit("nome do HUD: até 12 letras (a tela tem 40 colunas e o P2 fica à direita)")
    p = open(os.path.join(RAIZ, "build", "kof98ae_p.bin"), "rb").read()
    velho = b"\x00\x00\x03" + antigo + b"\xff"
    achados = [m.start() for m in re.finditer(re.escape(velho), p)]
    if len(achados) != 1:
        raise SystemExit(f"esperava 1 registro {velho!r}, achei {len(achados)}")
    rec = achados[0]
    ptrs = [m.start() for m in re.finditer(re.escape(struct.pack(">I", rec)), p)]
    if not ptrs:
        raise SystemExit(f"nenhum ponteiro pro registro {rec:#x}")
    reg = b"\x00\x00\x03" + novo + b"\xff"
    patches = []
    if len(reg) <= len(velho):
        patches.append([rec, list(reg + b"\xff" * 0)])
        print(f"{rec:#08x}: {velho!r} -> {reg!r}")
    else:
        cave = CAVE_HUD + 0x10 * (sum(antigo) % 32)
        patches.append([cave, list(reg)])
        for ptr in ptrs:
            patches.append([ptr, list(struct.pack(">I", cave))])
        print(f"{rec:#08x} -> novo registro em {cave:#08x} {reg!r} (ponteiros {[hex(x) for x in ptrs]})")
    destino = os.path.join(RAIZ, "build", "patches", f"hud_{arquivo}_ae.json")
    json.dump(patches, open(destino, "w"))
    print("patch:", os.path.relpath(destino, RAIZ))


if __name__ == "__main__":
    main()
