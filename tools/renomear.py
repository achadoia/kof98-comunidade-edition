"""Troca o nome completo de um lutador no KOF98 Anniversary (KOF_VARIANTE=ae) e grava o patch em
build/patches/nome_<arquivo>_ae.json.

O nome completo fica em duas tabelas de ponteiros (0x0B4568 alinhada à esquerda, 0x0B48D6 à direita), cada
registro = 3 bytes de controle (posição) + texto + 0xFF; nome e sobrenome separados por \\r\\n, campos de 8.
  "pz)RALF    \\xff"            (uma linha)        "py)TERRY   \\r\\nBOGARD  \\xff"  (duas linhas)
  "s\\xda9    RALF\\xff"  (direita, uma linha)     "s\\xd99   TERRY\\r\\n  BOGARD\\xff"
Se o nome novo cabe no lugar, grava em cima; senão o registro novo vai pra área livre (CAVE) e o ponteiro
das duas tabelas é trocado. Nomes com mais de 8 letras ficam numa linha só (ex.: RALFECINHA).

Uso: python tools/renomear.py "RALF" "RALFECINHA" [arquivo]
     python tools/renomear.py "TERRY BOGARD" "TERRY URAMESHI" [arquivo]
"""
import json
import os
import struct
import sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TABELAS = ((0x0B4568, str.ljust), (0x0B48D6, str.rjust))
NREG = 60
CAVE_NOMES = 0x0A7C00            # área livre (0xFF) pra registros maiores: 0xA7C00-0xA7DFF; o 0xF6800 do hack NÃO era livre (código do ae)
PREFIXO_1LINHA = {str.ljust: b"pz)", str.rjust: b"s\xda9"}
PREFIXO_2LINHAS = {str.ljust: b"py)", str.rjust: b"s\xd99"}


def texto(partes, alinha):
    if len(partes) == 1:
        return alinha(partes[0], 8).encode()
    return (alinha(partes[0], 8) + "\r\n" + alinha(partes[1], 8)).encode()


def main():
    antigo, novo = sys.argv[1].upper().split(), sys.argv[2].upper().split()
    arquivo = sys.argv[3] if len(sys.argv) > 3 else antigo[0].lower()
    if any(len(x) > 8 for x in novo) and len(novo) > 1:
        raise SystemExit("nome de 2 linhas: cada parte com até 8 letras; use uma linha só pra nomes maiores")
    p = open(os.path.join(RAIZ, "build", "kof98ae_p.bin"), "rb").read()
    patches = []
    cave = CAVE_NOMES + 0x40 * (sum(map(ord, arquivo)) % 8)   # cada lutador num pedaço próprio (64 bytes)
    for base, alinha in TABELAS:
        velho = texto(antigo, alinha)
        achados = []
        for i in range(NREG):
            ptr = struct.unpack(">I", p[base + 4 * i:base + 4 * i + 4])[0]
            if p[ptr + 3:ptr + 3 + len(velho) + 1] == velho + b"\xff":
                achados.append((i, ptr))
        if len(achados) != 1:
            raise SystemExit(f"esperava 1 registro {velho!r} na tabela {base:#x}, achei {len(achados)}")
        i, ptr = achados[0]
        prefixo = (PREFIXO_1LINHA if len(novo) == 1 else PREFIXO_2LINHAS)[alinha]
        reg = prefixo + texto(novo, alinha) + b"\xff"
        velho_reg_tam = 3 + len(velho) + 1
        if len(reg) <= velho_reg_tam:
            patches.append([ptr, list(reg)])
            print(f"{ptr:#08x}: {p[ptr:ptr + velho_reg_tam]!r} -> {reg!r}")
        else:
            patches.append([cave, list(reg)])
            patches.append([base + 4 * i, list(struct.pack(">I", cave))])
            print(f"{ptr:#08x} -> novo registro em {cave:#08x} {reg!r} (ponteiro {base + 4 * i:#08x})")
            cave += (len(reg) + 1) & ~1
    destino = os.path.join(RAIZ, "build", "patches", f"nome_{arquivo}_ae.json")
    json.dump(patches, open(destino, "w"))
    print("patch:", os.path.relpath(destino, RAIZ))


if __name__ == "__main__":
    main()
