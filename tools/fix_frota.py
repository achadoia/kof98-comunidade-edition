"""Corrige a "frota invencível" do Daimon (e de qualquer golpe com a mesma flag de quadro).

Causa (achada no MAME, 15/10): alguns quadros de ataque (ex.: J.B do Daimon) têm +0x7C bit 6; quando
conectam, a colisão (0x003B2A) liga +0xE2 bit 7 no atacante. Todo quadro, 0x017004 testa +0xE2 bit 7 e
apaga a caixa de dano (+0x7C bit 0). O bit só é limpo no reset de estado (0x0170D8), que roda no fim do
pouso. Se o oponente encosta um golpe NO QUADRO DO POUSO, o lutador entra direto na guarda (0x014928 em
pé / 0x014B72 agachado), que não faz o reset: fica sem caixa de dano enquanto segura a guarda, até mexer
o direcional.

Correção: nas duas entradas de guarda do chão, o `jsr $16B4C` (vira pro oponente) passa por uma rotina
que antes limpa +0xE2 bit 7. Fora desse caso o comportamento é idêntico.

Saída: build/patches/frota_<variante>.json
"""
import json
import os
import struct

from variante import CAVE as BASE, MEM as ROM, VARIANTE

SITES = (0x014928, 0x014B72)
ORIGINAL = "4eb900016b4c"   # jsr $16b4c.l
CAVE = BASE + 0x200

ROTINA = bytes.fromhex(
    "022c007f00e2"    # andi.b #$7f,$e2(a4)   tira a "sem caixa de dano depois do acerto"
    "4ef900016b4c"    # jmp    $16b4c          (vira pro oponente, como antes)
)


def main():
    for s in SITES:
        assert ROM[s:s + 6].hex() == ORIGINAL, f"bytes inesperados na entrada de guarda {s:06x}"
    assert ROM[0x017004:0x017012].hex() == "4a2c00e26a000008022c00fe007c", "teste de +0xE2 diferente"
    assert ROM[CAVE:CAVE + len(ROTINA)] == b"\xff" * len(ROTINA), "área da rotina não está livre"
    jsr = list(bytes.fromhex("4eb9") + struct.pack(">I", CAVE))
    patch = [[CAVE, list(ROTINA)]] + [[s, jsr] for s in SITES]
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches")
    os.makedirs(pasta, exist_ok=True)
    json.dump(patch, open(os.path.join(pasta, f"frota_{VARIANTE}.json"), "w"))
    print("frota: guarda limpa +0xE2 bit 7 ao entrar")


if __name__ == "__main__":
    main()
