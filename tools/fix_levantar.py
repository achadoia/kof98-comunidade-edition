"""Corrige o quadro vulnerável e sem guarda no fim do levantar (bug do Jirai Shin do Daimon no levantar).

Causa (achada no MAME, 15/10): o estado "levantando" (corrotina em 0x01AC28) espera a animação
acabar (+0x7C bit 7), chama 0x017140 (tira a invencibilidade: andi.b #$9F,$E3) e marca o próximo
estado = neutro (0x0165F4), mas CEDE o quadro (bra $16526). O neutro, que é quem decide a guarda,
só roda no quadro seguinte; nesse meio-tempo a colisão do quadro acontece com o lutador já
vulnerável e ainda sem poder defender. Golpes com área ativa longa (o tremor do Jirai Shin) acertam
esse quadro com facilidade e o jogo conta como combo.

Correção: em 0x01AC62 o `bra.w $16526` vira `bra.w $165F4` — entra no neutro no MESMO quadro,
igual ao caminho que o próprio jogo já usa em 0x01AC42 (bne.w $165F4). Vale pra todo personagem.

Saída: build/patches/levantar_<variante>.json
"""
import json
import os

from variante import MEM as ROM, VARIANTE

SITE = 0x01AC62
ORIGINAL = "6000b8c2"   # bra.w $16526 (cede o quadro)
NOVO = "6000b990"       # bra.w $165f4 (estado neutro já neste quadro)
CONTEXTO = 0x01AC5C     # move.l #$165f4,(a4) logo antes


def main():
    assert ROM[CONTEXTO:CONTEXTO + 6].hex() == "28bc000165f4", "estado de levantar diferente do esperado"
    assert ROM[SITE:SITE + 4].hex() == ORIGINAL, "bytes inesperados no fim do levantar"
    assert 0x165F4 - (SITE + 2) == -0x4670
    patch = [[SITE, list(bytes.fromhex(NOVO))]]
    pasta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "patches")
    os.makedirs(pasta, exist_ok=True)
    json.dump(patch, open(os.path.join(pasta, f"levantar_{VARIANTE}.json"), "w"))
    print("levantar: entra no neutro no mesmo quadro em que perde a invencibilidade")


if __name__ == "__main__":
    main()
