"""Disassembler rápido do kof98_p.bin (68000).

Uso: python m68dis.py <endereco_hex> [qtd_instrucoes]
Endereços do 68000: 0x000000-0x0FFFFF = ROM fixa; 0x200000-0x2FFFFF = banco 0
(arquivo 0x100000). Outros bancos: use --banco N.
"""
import sys
import capstone

import os
ROM = open(os.environ.get("KOF_BIN") or (__file__.rsplit("tools", 1)[0] + "build/kof98_p.bin"), "rb").read()


def file_off(addr, banco=0):
    if addr < 0x100000:
        return addr
    if 0x200000 <= addr < 0x300000:
        return 0x100000 + banco * 0x100000 + (addr - 0x200000)
    raise ValueError(f"endereço fora da ROM: {addr:#x}")


def dis(addr, n=40, banco=0):
    md = capstone.Cs(capstone.CS_ARCH_M68K, capstone.CS_MODE_M68K_000)
    off = file_off(addr, banco)
    out = []
    for ins in md.disasm(ROM[off:off + n * 10], addr):
        out.append(f"{ins.address:06x}  {ins.bytes.hex():<20} {ins.mnemonic:<8} {ins.op_str}")
        if len(out) >= n:
            break
    return "\n".join(out)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    banco = int(next((a.split("=")[1] for a in sys.argv if a.startswith("--banco=")), 0))
    print(dis(int(args[0], 16), int(args[1]) if len(args) > 1 else 40, banco))
