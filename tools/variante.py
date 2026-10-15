"""Qual ROM está sendo trabalhada: KOF_VARIANTE=orig (kof98 MVS, padrão) ou ae (kof98ae2016).

Expõe:
  MEM   -> bytes no espaço de endereços do 68000 (0x000000-0x2FFFFF; janela 0x200000 = BANCO),
           pra ler tabelas/listas pelo endereço que o jogo usa
  CAVE  -> início da área livre na ROM fixa onde os patches põem rotinas novas
  arquivo(addr) -> offset no binário linear (build/kof98*_p.bin) de um endereço do 68000
"""
import os

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
VARIANTE = os.environ.get("KOF_VARIANTE", "orig")

if VARIANTE == "ae":
    BIN = os.path.join(RAIZ, "build", "kof98ae_p.bin")
    CAVE = 0x09F000      # 0x09ED3C-0x0A7E24 livre (0xFF) no ae2016; 0x0F6500 o hack usa
    BANCO = 1            # banco do p2 na janela 0x200000 durante a luta (lido no MAME)
else:
    BIN = os.path.join(RAIZ, "build", "kof98_p.bin")
    CAVE = 0x0F6500
    BANCO = 0

LINEAR = open(BIN, "rb").read()


def arquivo(addr):
    if addr < 0x100000:
        return addr
    if 0x200000 <= addr < 0x300000:
        return 0x100000 + BANCO * 0x100000 + (addr - 0x200000)
    raise ValueError(f"endereço fora da ROM: {addr:#x}")


MEM = LINEAR[:0x100000] + b"\x00" * 0x100000 + LINEAR[arquivo(0x200000):arquivo(0x200000) + 0x100000]
