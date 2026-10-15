"""Monta o KOF98 Anniversary MOD como "romdata" do FBNeo oficial (sem IPS).

Por quê: o driver kof98ae do FBNeo atual traz as revisões 2016 e 2007 juntas e exige os arquivos
.dif da 2007 como essenciais, mesmo pra rodar a 2016. Nosso set é só a 2016, então o FBNeo recusa.
O romdata substitui a lista de ROMs do driver: declaramos só a 2016, com o p1 já modificado.

Uso: python gerar_romdata.py <kof98ae2016.zip modificado> <kof98.zip original> <pasta do FBNeo>
Saída: <FBNeo>/roms/kof98aemod.zip (autocontido, sem a BIOS) + <FBNeo>/support/romdata/kof98aemod.dat
Abrir: fbneo64.exe -romdata -wkof98aemod.dat  (em janela; Alt+Enter = tela cheia)
"""
import os
import sys
import zipfile
import zlib

ZIP = "kof98aemod"
# (seção do romdata, arquivo no zip gerado, de onde vem: "ae" ou "kof98", nome na origem)
ROMS = [
    ("Program", "242-p1ae.p1", "ae", "242ae-p1.p1"),
    ("Program", "242-p2ae.sp2", "ae", "242ae-p2.sp2"),
    ("Text", "242-s1ae.s1", "ae", "242ae-s1.s1"),
    ("Graphics", "242-c1ae.c1", "ae", "242ae-c1.c1"),
    ("Graphics", "242-c2ae.c2", "ae", "242ae-c2.c2"),
    ("Graphics", "242-c3.c3", "kof98", "242-c3.c3"),
    ("Graphics", "242-c4.c4", "kof98", "242-c4.c4"),
] + [("Graphics", f"242-c{n}ae.c{n}", "ae", f"242ae-c{n}.c{n}") for n in range(5, 13)] + [
    ("Z80", "242-m1ae.m1", "ae", "242ae-m1.m1"),
] + [("Samples", f"242-v{n}.v{n}", "kof98", f"242-v{n}.v{n}") for n in range(1, 5)] + [
    ("Samples", "242-v5ae.v5", "ae", "242ae-v5.v5"),
]


def main():
    mod_ae, kof98, fbneo = sys.argv[1:4]
    origens = {"ae": zipfile.ZipFile(mod_ae), "kof98": zipfile.ZipFile(kof98)}
    destino = os.path.join(fbneo, "roms", ZIP + ".zip")
    linhas = [
        "// KOF98 Anniversary 2016 + mods do kof98-recomp (gerado por tools/gerar_romdata.py)",
        f"ZipName:{ZIP}",
        "DrvName:kof98ae",
        'FullName:"KOF98 Anniversary MOD (kof98-recomp)"',
    ]
    secao = None
    with zipfile.ZipFile(destino, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as z:
        for sec, nome, origem, nome_origem in ROMS:
            dados = origens[origem].read(nome_origem)
            z.writestr(nome, dados)
            if sec != secao:
                linhas += ["", f"[{sec}]"]
                secao = sec
            linhas.append(f"{nome}, 0x{len(dados):08x}, 0x{zlib.crc32(dados):08x}")
    # "-w" no nome: o FBNeo lê os 2 primeiros caracteres depois de -romdata como opção (-w = janela)
    dat = os.path.join(fbneo, "support", "romdata", "-w" + ZIP + ".dat")
    os.makedirs(os.path.dirname(dat), exist_ok=True)
    open(dat, "w", encoding="utf-8").write("\n".join(linhas) + "\n")
    print(f"{destino}\n{dat}")


if __name__ == "__main__":
    main()
