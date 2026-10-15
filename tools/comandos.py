"""Decodifica as listas de comandos (especiais) do KOF98.

Formato descoberto em 0x01720E (interpretador):
  lista = longwords apontando pra comandos, termina em valor negativo
  comando = [n_passos][janela][word limite] + n_passos x (tipo, valor)
  os passos são conferidos do último pro primeiro contra o histórico de 60 quadros.
Uso: python comandos.py <endereco_lista_hex> [...]
"""
import os
import struct
import sys

from variante import MEM as ROM  # espaço de endereços do 68000 (fixa + banco 0), orig ou ae

# bits do joystick Neo Geo: 0 cima, 1 baixo, 2 esquerda, 3 direita, 4-7 A B C D
NUM = {0x0: "5", 0x1: "8", 0x2: "2", 0x4: "4", 0x8: "6", 0x5: "7", 0x9: "9", 0x6: "1", 0xA: "3"}


def botoes(v):
    return "".join(b for i, b in enumerate("ABCD") if v & (0x10 << i))


def fmt(v):
    d = NUM.get(v & 0xF, f"?{v & 0xF:x}")
    b = botoes(v)
    return (d if d != "5" or not b else "") + ("+" + b if b and d != "5" else b)


def comando(addr):
    n, janela, limite = ROM[addr], ROM[addr + 1], struct.unpack(">H", ROM[addr + 2:addr + 4])[0]
    passos = [(ROM[addr + 4 + i * 2], ROM[addr + 5 + i * 2]) for i in range(n)]
    return n, janela, limite, passos


def lista(addr):
    out = []
    while True:
        p = struct.unpack(">I", ROM[addr:addr + 4])[0]
        if p & 0x80000000:
            return out
        out.append(p)
        addr += 4


if __name__ == "__main__":
    for a in sys.argv[1:]:
        base = int(a, 16)
        print(f"== lista {base:06x}")
        for i, p in enumerate(lista(base)):
            if ROM[p] & 0x80:  # slot vazio (interpretador pula n negativo)
                continue
            n, jan, lim, passos = comando(p)
            txt = "  ".join(f"t{t:02x}:{fmt(v)}({v:02x})" for t, v in passos)
            print(f"  [{i:2}] {p:06x} n={n} jan={jan:3} lim={lim:5}  {txt}")
