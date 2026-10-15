"""Gera patches IPS do FBNeo a partir da ROM original e da modificada (saída do gerar_rom.py).

A ROM original fica intacta (passa na checagem de versão); o FBNeo aplica o IPS ao carregar:
  fbneo64.exe <jogo> -ips kof98mod
Arquivos em <fbneo>/support/ips/<jogo>/kof98mod.dat + kof98mod_<rom>.ips

Uso: python gerar_ips.py <zip original> <zip modificado> <nome do jogo no fbneo> <pasta fbneo>
"""
import os
import sys
import zipfile
import zlib


def ips(orig, novo):
    """Registros IPS (offset 24 bits, tamanho 16 bits) das diferenças entre orig e novo."""
    assert len(orig) == len(novo) and len(orig) <= 0x1000000
    out = bytearray(b"PATCH")
    i = 0
    while i < len(novo):
        if orig[i] == novo[i]:
            i += 1
            continue
        ini = i
        while i < len(novo) and i - ini < 0xFFFF and (orig[i] != novo[i] or novo[i + 1:i + 3] != orig[i + 1:i + 3]):
            i += 1
        if ini == 0x454F46:  # offset que seria lido como "EOF": começa 1 byte antes
            ini -= 1
        out += ini.to_bytes(3, "big") + (i - ini).to_bytes(2, "big") + novo[ini:i]
    return bytes(out + b"EOF")


def main():
    zorig, zmod, jogo, fbneo = sys.argv[1:5]
    a, b = zipfile.ZipFile(zorig), zipfile.ZipFile(zmod)
    pasta = os.path.join(fbneo, "support", "ips", jogo)
    os.makedirs(pasta, exist_ok=True)
    linhas = []
    for n in a.namelist():
        do, dn = a.read(n), b.read(n)
        if do == dn:
            continue
        nome_ips = f"kof98mod_{n.replace('.', '_')}.ips"
        open(os.path.join(pasta, nome_ips), "wb").write(ips(do, dn))
        # casa pela CRC do arquivo original (o nome dentro do zip pode diferir do nome do driver)
        linhas.append(f"{n} {nome_ips} CRC({zlib.crc32(do):08x})")
        print(f"{n}: {sum(1 for x, y in zip(do, dn) if x != y)} bytes diferentes -> {nome_ips}")
    dat = "\n".join(linhas) + "\n\n[en_US]\nKOF98 mods (kof98-recomp)\n" \
        "Golpes facilitados, Chizuru -30% barra, projetil+agarrao corrigido, pulinho do EXTRA.\n"
    open(os.path.join(pasta, "kof98mod.dat"), "w", encoding="utf-8").write(dat)
    print(f"-> {pasta}")


if __name__ == "__main__":
    main()
