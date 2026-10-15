"""Corrige o bug "projétil + golpe que tira a guarda" (Athena, Iori, Takuma, Kensou, Heidern, Rugal...).

Causa (achada no MAME, 14/10): a decisão de entrar em guarda (0x0148AA) olha o atacante e
devolve "não pode guardar" se ele tiver +0xE5 bit 2 ligado — flag que teleporte e agarrões de
comando ligam. Esse teste vem ANTES do teste de projétil defensável vivo (+0xE1 bit 2, ligado em
0x024A7C quando o projétil nasce). Resultado: com a bola voando, o 2º golpe deixa a bola indefensável.

Correção: o `btst #2,$e5(a0)` em 0x0148B8 vira jsr pra uma rotina que, se houver projétil
defensável vivo (+0xE1 bit 2), devolve Z=1 (como se o bit estivesse desligado). Sem projétil,
o comportamento é idêntico ao original.

Saída: build/patches/inbloqueavel.json
"""
import json
import os
import struct

from variante import CAVE as BASE, MEM as ROM, VARIANTE

SITE = 0x0148B8
ORIGINAL = "0828000200e5"   # btst.b #2,$e5(a0)
CAVE = BASE + 0x40

ROTINA = bytes.fromhex(
    "0828000200e1"  # +00 btst.b #2,$e1(a0)   projétil defensável vivo?
    "6608"          # +06 bne.s  projetil (+10)
    "0828000200e5"  # +08 btst.b #2,$e5(a0)
    "4e75"          # +0E rts
    "b000"          # +10 projetil: cmp.b d0,d0  -> Z=1
    "4e75"          # +12 rts
)


def main():
    assert ROM[SITE:SITE + 6].hex() == ORIGINAL, "bytes inesperados no teste de guarda"
    assert ROM[CAVE:CAVE + len(ROTINA)] == b"\xff" * len(ROTINA), "área da rotina não está livre"
    patch = [[CAVE, list(ROTINA)], [SITE, list(bytes.fromhex("4eb9") + struct.pack(">I", CAVE))]]
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches")
    os.makedirs(pasta, exist_ok=True)
    json.dump(patch, open(os.path.join(pasta, f"inbloqueavel_{VARIANTE}.json"), "w"))
    print("inbloqueavel: 2 blocos")


if __name__ == "__main__":
    main()
