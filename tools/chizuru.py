"""Chizuru ganha 30% menos barra de power (pedido do usuário, balanceamento).

Toda soma de barra passa por 4 pontos (move.w <ganho>,d0 / add.w d0,$e8(a4)):
  0x0155B4 (+0x1E0)  0x0155DA (+0x1DE)  0x015606 (+0x1DE)  0x015648 (+0x1E0 de a0)
Cada um vira jsr pra uma rotina na área livre (CAVE) que multiplica o ganho por 179/256 (~0,70)
se o lutador que recebe a barra (a4) for a Chizuru (+0x186 == 0xF0).

No kof98ae2016 o hack já trocou o 4º ponto por `jmp $263b50` (rotina dele no banco 1 que só soma
se o byte $7318(a4) for zero). Ali o jmp passa a apontar pra uma cópia dessa rotina na CAVE,
com a escala da Chizuru, e volta pro mesmo lugar ($15650).

Saída: build/patches/chizuru_<variante>.json
"""
import json
import os
import struct

from variante import CAVE as BASE, MEM as ROM, VARIANTE

CHIZURU = 15 * 16  # +0x186 = índice do personagem * 16
FATOR = 179        # /256
CAVE = BASE

ESCALA = (
    "0c6c" + f"{CHIZURU:04x}" + "0186"  # cmpi.w #CHIZURU,$186(a4)
    "6606"                              # bne.s  +6
    "c0fc" + f"{FATOR:04x}"             # mulu.w #FATOR,d0
    "e088"                              # lsr.l  #8,d0
)

ROTINA = bytes.fromhex(
    "302c01e0"      # +00 move.w $1e0(a4),d0
    "600a"          # +04 bra.s  escala (+10)
    "302c01de"      # +06 move.w $1de(a4),d0
    "6004"          # +0A bra.s  escala
    "302801e0"      # +0C move.w $1e0(a0),d0
    + ESCALA +      # +10 escala (14 bytes)
    "d16c00e8"      # +1E add.w  d0,$e8(a4)
    "4e75"          # +22 rts
)
# só no ae: o hack desviou pontos de soma pra rotinas dele no banco 1, todas no formato
#   move.w <ganho>,d0 / cmpi.b #0,$7318(a4) / bne +4 / add.w d0,$e8(a4) / jmp <volta>
# o jmp do ponto passa a ir pra uma cópia na CAVE com a escala da Chizuru.
def rotina_ae(mov_ganho, volta):
    return bytes.fromhex(
        mov_ganho +     # +00 move.w <ganho>,d0
        "0c2c00007318"  # +04 cmpi.b #0,$7318(a4)     (condição do hack)
        "6612"          # +0A bne.s  volta (+1E)
        + ESCALA +      # +0C escala (14 bytes)
        "d16c00e8"      # +1A add.w  d0,$e8(a4)
        "4ef9" + f"{volta:08x}"  # +1E volta: jmp <volta>
    )


SITES = {  # endereço -> (bytes originais, entrada na rotina)
    0x0155B4: ("302c01e0d16c00e8", CAVE + 0x0),
    0x0155DA: ("302c01ded16c00e8", CAVE + 0x6),
    0x015606: ("302c01ded16c00e8", CAVE + 0x6),
    0x015648: ("302801e0d16c00e8", CAVE + 0xC),
}


def livre(addr, n):
    assert ROM[addr:addr + n] == b"\xff" * n, f"área {addr:06x} não está livre"


def main():
    livre(CAVE, len(ROTINA))
    patch = [[CAVE, list(ROTINA)]]
    proxima_ae = BASE + 0x100  # depois das rotinas do pulinho (BASE+0x60..0xA3)
    for addr, (orig, entrada) in SITES.items():
        atual = ROM[addr:addr + 8].hex()
        if VARIANTE == "ae" and atual.startswith("4ef9") and atual.endswith("4e71"):
            alvo = int(atual[4:12], 16)
            hack = ROM[alvo:alvo + 22].hex()
            assert hack[8:28] == "0c2c000073186604d16c", f"rotina do hack inesperada em {alvo:06x}: {hack}"
            assert hack[32:36] == "4ef9"
            rot = rotina_ae(hack[:8], int(hack[36:44], 16))
            livre(proxima_ae, len(rot))
            patch.append([proxima_ae, list(rot)])
            patch.append([addr, list(bytes.fromhex("4ef9") + struct.pack(">I", proxima_ae))])
            proxima_ae += 0x30
            continue
        assert atual == orig, f"bytes inesperados em {addr:06x}: {atual}"
        patch.append([addr, list(bytes.fromhex("4eb9") + struct.pack(">I", entrada) + bytes.fromhex("4e71"))])
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches")
    os.makedirs(pasta, exist_ok=True)
    json.dump(patch, open(os.path.join(pasta, f"chizuru_{VARIANTE}.json"), "w"))
    print(f"chizuru ({VARIANTE}): {len(patch)} blocos")


if __name__ == "__main__":
    main()
