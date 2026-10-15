"""Quebra os infinitos de "pulinho + golpe" do modo EXTRA (Kyo →→ + ↓C no canto, Mai →→ + ↓↙←A...).

Causa (medida no MAME, 15/10): no EXTRA o →→ é um pulinho que leva ~23 quadros do 1º toque até
poder agir; o ↓C do Kyo deixa o oponente ~29 quadros atordoado. No canto (sem empurrão) sobra
vantagem e pulinho → ↓C emenda pra sempre.

Correção: no pouso do pulinho (estado em 0x0153A4, anim de pouso 0x2C, só com +0x1D5 = EXTRA)
o personagem fica EXTRA_QUADROS quadros a mais sem poder agir. Contador no byte livre +0x1F8.
  0x0153B4  move.l #$153ba,(a4)  ->  jsr INIT   (INIT arma o contador e troca o estado pra ESPERA)
  ESPERA = espera original (anim acabar) + contador, depois volta ao neutro como o original.

Saída: build/patches/extra_pulinho.json
"""
import json
import os
import struct

from variante import CAVE as BASE, MEM as ROM, VARIANTE

EXTRA_QUADROS = 6
SITE = 0x0153B4
ORIGINAL = "28bc000153ba"
INIT = BASE + 0x60
ESPERA = BASE + 0x80
CONTADOR = 0x1F8

ROTINA_INIT = bytes.fromhex(
    f"422c{CONTADOR:04x}"                  # +00 clr.b   CONTADOR(a4)
    "0c6c002c0072"                         # +04 cmpi.w  #$2c,$72(a4)   pouso do pulinho?
    "660c"                                 # +0A bne.s   +18
    "4a2c01d5"                             # +0C tst.b   $1d5(a4)       modo EXTRA?
    "6706"                                 # +10 beq.s   +18
    f"197c{EXTRA_QUADROS:04x}{CONTADOR:04x}"  # +12 move.b #N,CONTADOR(a4)
    "28bc" + f"{ESPERA:08x}"               # +18 move.l  #ESPERA,(a4)
    "4e75"                                 # +1E rts
)

ROTINA_ESPERA = bytes.fromhex(
    "4a2c007c"                             # +00 tst.b   $7c(a4)        animação acabou?
    "6b06"                                 # +04 bmi.s   +0C
    "4ef9000139e6"                         # +06 jmp     $139e6         (fim de quadro normal)
    f"4a2c{CONTADOR:04x}"                  # +0C tst.b   CONTADOR(a4)
    "670a"                                 # +10 beq.s   +1C
    f"532c{CONTADOR:04x}"                  # +12 subq.b  #1,CONTADOR(a4)
    "4ef9000139e6"                         # +16 jmp     $139e6
    "28ac0036"                             # +1C move.l  $36(a4),(a4)   volta ao neutro (original)
    "2054"                                 # +20 movea.l (a4),a0
    "4ed0"                                 # +22 jmp     (a0)
)


def main():
    assert ROM[SITE:SITE + 6].hex() == ORIGINAL, "bytes inesperados no pouso do pulinho"
    for addr, rot in ((INIT, ROTINA_INIT), (ESPERA, ROTINA_ESPERA)):
        assert ROM[addr:addr + len(rot)] == b"\xff" * len(rot), f"área {addr:06x} não está livre"
    assert INIT + len(ROTINA_INIT) <= ESPERA
    patch = [[INIT, list(ROTINA_INIT)], [ESPERA, list(ROTINA_ESPERA)],
             [SITE, list(bytes.fromhex("4eb9") + struct.pack(">I", INIT))]]
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches")
    os.makedirs(pasta, exist_ok=True)
    json.dump(patch, open(os.path.join(pasta, f"extra_pulinho_{VARIANTE}.json"), "w"))
    print(f"extra_pulinho: +{EXTRA_QUADROS} quadros no pouso do pulinho do EXTRA")


if __name__ == "__main__":
    main()
