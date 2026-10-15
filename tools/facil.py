"""Gera o patch de golpes facilitados para todos os personagens.

Todos os personagens apontam pros mesmos ~35 modelos de comando (tabela 0x0B14A2,
ver docs/comandos.md), então o patch reescreve só esses modelos, no lugar.

Saída: build/facil.json  = [[endereco, [bytes...]], ...]  (aplicado pelo Lua no MAME
e pelo recomp), mais um relatório no stdout.
Uso: python facil.py
"""
import json
import os
import struct

from comandos import ROM, comando, lista
from variante import VARIANTE

TABELA_PERSONAGENS = 0x0B14A2
N_ENTRADAS = 76
CARGA_FATOR = 0.8  # carga 20% mais curta (pedido do usuário)

# bits de direção: 1 cima, 2 baixo, 4 esquerda, 8 direita
DIR = {"5": 0, "8": 1, "2": 2, "4": 4, "6": 8, "7": 5, "9": 9, "1": 6, "3": 10}
NUM = {v: k for k, v in DIR.items()}
ESPELHO = str.maketrans("4613", "6431")

# padrão original (virado pra direita, só as direções antes do botão) -> facilitado
REGRAS = {
    "236": "23",        # ↓↘→   -> ↓↘
    "214": "21",        # ↓↙←   -> ↓↙
    "62~6": "63",       # →↓↘ (shoryuken, último passo "qualquer →") -> →↘
    "42~4": "41",       # ←↓↙ -> ←↙
    "426": "43",        # meia-lua ←↙↓↘→ -> ←↘
    "624": "61",        # meia-lua →↘↓↙← -> →↙
    "236236": "2323",   # super ↓↘→↓↘→ -> ↓↘↓↘
    "214214": "2121",
    "23624": "2321",    # ↓↘→ + meia-lua trás
    "21426": "2123",
    "426426": "4343",
    "624624": "6161",
}
REGRAS.update({k.translate(ESPELHO): v.translate(ESPELHO) for k, v in list(REGRAS.items())})


def modelos():
    vistos = set()
    for i in range(N_ENTRADAS):
        for k in (0, 4):
            lst = struct.unpack(">I", ROM[TABELA_PERSONAGENS + i * 8 + k:][:4])[0]
            for c in lista(lst):
                if c not in vistos and not ROM[c] & 0x80:
                    vistos.add(c)
                    yield c


def assinatura(passos):
    """Direções antes do gatilho; '~' marca passo 'qualquer bit' (tipo 04)."""
    s = ""
    for t, v in passos[:-1]:
        if t in (0x14, 0x28):
            s += NUM.get(v & 0xF, "?")
        elif t in (0x04, 0x18) and v & 0xF and not v & 0xF0:
            s += "~" + NUM.get(v & 0xF, "?")  # direção aproximada (04 / 18 do kof98ae)
        else:
            return None
    return s


def facilitar(addr):
    n, jan, lim, passos = comando(addr)
    if lim and passos and passos[-1][0] in (0x0C, 0x10) or any(t in (0x0C, 0x10) for t, _ in passos):
        novo_lim = round(lim * CARGA_FATOR)
        return "carga", struct.pack(">H", novo_lim), addr + 2, f"carga {lim} -> {novo_lim} quadros"
    sig = assinatura(passos)
    regras = {k.replace("~", ""): v for k, v in REGRAS.items()}
    if sig is None or sig.replace("~", "") not in regras:
        return None
    alvo = regras[sig.replace("~", "")]
    gat_t, gat_v = passos[-1]
    dirs = [DIR[c] for c in alvo]
    if gat_t == 0x2C:
        # direções proibidas no aperto: tira as da última direção, senão ↘+botão não vale
        gat_v = (gat_v & 0xF0) | (gat_v & 0x0F & ~dirs[-1])
    else:
        gat_t, gat_v = 0x2C, (gat_v & 0xF0)
    novos = [(0x14, d) for d in dirs]
    novos[-1] = (0x28, dirs[-1])  # última direção sem botão junto, igual ao original
    novos.append((gat_t, gat_v))
    corpo = bytes([len(novos), jan]) + struct.pack(">H", lim) + bytes(b for p in novos for b in p)
    return "motion", corpo, addr, f"{sig.replace('~', '')} -> {alvo}"


def main():
    patch, usados = [], {}
    for c in sorted(modelos()):
        r = facilitar(c)
        if not r:
            n, jan, lim, passos = comando(c)
            print(f"{c:06x}  (sem regra) " + " ".join(f"{t:02x}:{v:02x}" for t, v in passos))
            continue
        tipo, dados, onde, desc = r
        if tipo == "motion":
            chave = dados[4:]
            if chave in usados:
                raise SystemExit(f"conflito: {c:06x} e {usados[chave]:06x} viram o mesmo comando")
            usados[chave] = c
        patch.append([onde, list(dados)])
        print(f"{c:06x}  {desc}")
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches", f"facil_{VARIANTE}.json")
    json.dump(patch, open(out, "w"))
    print(f"{len(patch)} modelos alterados -> {out}")


if __name__ == "__main__":
    main()
