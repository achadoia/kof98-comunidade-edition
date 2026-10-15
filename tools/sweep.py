"""Varredura linear de toda a ROM -> build/sweep.txt (grep-ável).

Varredura linear pega lixo nas áreas de dados, mas serve pra achar referências.
Linha: <banco>:<endereco>  <instrucao>
"""
import capstone

ROOT = __file__.rsplit("tools", 1)[0]
ROM = open(ROOT + "build/kof98_p.bin", "rb").read()
md = capstone.Cs(capstone.CS_ARCH_M68K, capstone.CS_MODE_M68K_000)
md.skipdata = True

regioes = [("F", 0x000000, 0x000000)] + [(str(b), 0x100000 + b * 0x100000, 0x200000) for b in range(4)]
with open(ROOT + "build/sweep.txt", "w") as out:
    for nome, off, base in regioes:
        for ins in md.disasm(ROM[off:off + 0x100000], base):
            out.write(f"{nome}:{ins.address:06x}  {ins.mnemonic} {ins.op_str}\n")
print("ok")
