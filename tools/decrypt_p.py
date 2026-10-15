"""Desembaralha o código 68000 do KOF98 (242-p1 + 242-p2) num binário linear.

Porta direta de kof98_prot_device::decrypt_68k do MAME
(src/devices/bus/neogeo/prot_kof98.cpp, BSD-3-Clause, S. Smith/D. Haywood/F. Priuli).

Saída: 0x500000 bytes big-endian (ordem do 68000):
  0x000000-0x0FFFFF  ROM fixa (mapeada em 0x000000 do 68000)
  0x100000-0x4FFFFF  4 bancos de 1 MB (janela 0x200000 do 68000)

Uso: python -I decrypt_p.py <kof98.zip> <saida.bin>
"""
import sys
import zipfile

SEC = (0x000000, 0x100000, 0x000004, 0x100004, 0x10000A, 0x00000A, 0x10000E, 0x00000E)
POS = (0x000, 0x004, 0x00A, 0x00E)


def decrypt(rom: bytearray) -> bytearray:
    dst = bytes(rom[:0x200000])

    def cp(d, s):
        rom[d:d + 2] = dst[s:s + 2]

    for i in range(0x800, 0x100000, 0x200):
        for j in range(0, 0x100, 0x10):
            for k in range(0, 16, 2):
                cp(i + j + k, i + j + SEC[k // 2] + 0x100)
                cp(i + j + k + 0x100, i + j + SEC[k // 2])
            if 0x080000 <= i < 0x0C0000:
                for p in POS:
                    cp(i + j + p, i + j + p)
                    cp(i + j + p + 0x100, i + j + p + 0x100)
            elif i >= 0x0C0000:
                for p in POS:
                    cp(i + j + p, i + j + p + 0x100)
                    cp(i + j + p + 0x100, i + j + p)
        cp(i + 0x000, i + 0x000)
        cp(i + 0x002, i + 0x100000)
        cp(i + 0x100, i + 0x100)
        cp(i + 0x102, i + 0x100100)
    rom[0x100000:0x500000] = rom[0x200000:0x600000]
    rom = rom[:0x500000]
    # os dumps vêm com cada word invertida; o 68000 é big-endian
    rom[0::2], rom[1::2] = rom[1::2], rom[0::2]
    return rom


def main():
    src, out = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(src) as z:
        p1 = z.read("242-p1.p1")
        p2 = z.read("242-p2.sp2")
    assert len(p1) == 0x200000 and len(p2) == 0x400000, "tamanhos inesperados"
    rom = decrypt(bytearray(p1 + p2))
    with open(out, "wb") as f:
        f.write(rom)
    print(f"ok: {len(rom):#x} bytes -> {out}")


if __name__ == "__main__":
    main()
