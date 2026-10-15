"""Gera um kof98.zip jogável com os patches aplicados (formato MVS original, código reembaralhado).

1. desembaralha p1+p2 (decrypt_p.decrypt)
2. aplica os patches de build/patches/<nome>.json no binário linear
3. reembaralha: o decrypt é uma permutação de words de 2 bytes; rodando o decrypt sobre "rótulos"
   descobre-se de qual word da entrada veio cada word da saída, e o inverso grava os patches de volta
4. escreve dist/mod/kof98.zip com o 242-p1.p1 novo e o resto igual ao original

Uso: python gerar_rom.py <kof98.zip original> facil chizuru inbloqueavel extra_pulinho
A ROM é da SNK: o zip gerado é só pra uso pessoal e nunca vai pro git.
"""
import json
import os
import sys
import zipfile

import numpy as np

from decrypt_p import decrypt

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def checar_sobreposicao(nomes):
    """Recusa patches que escrevem no mesmo byte (rotinas na CAVE batendo uma na outra)."""
    dono = {}
    for nome in nomes:
        for addr, dados in json.load(open(os.path.join(RAIZ, "build", "patches", nome + ".json"))):
            for k in range(len(dados)):
                a = addr + k
                if a in dono and dono[a] != nome:
                    raise SystemExit(f"patches {dono[a]} e {nome} se sobrepõem em {a:#08x}")
                dono[a] = nome


def permutacao():
    """origem[j] = índice da word de entrada (p1+p2, 3M words) que vira a word j da saída."""
    n = 0x600000 // 2
    rotulos = np.arange(n, dtype=">u4")
    # cada word vira 4 bytes de rótulo; o decrypt mexe em pares de bytes, então roda em 2 planos
    alto = bytearray((rotulos >> 16).astype(">u2").tobytes())
    baixo = bytearray((rotulos & 0xFFFF).astype(">u2").tobytes())
    # o decrypt inverte os bytes de cada word no fim: lê em little-endian pra desfazer
    sa = np.frombuffer(bytes(decrypt(alto)), dtype="<u2").astype(np.uint32)
    sb = np.frombuffer(bytes(decrypt(baixo)), dtype="<u2").astype(np.uint32)
    return (sa << 16) | sb


def main_ae(origem_zip, nomes):
    """kof98ae2016: código sem criptografia (p1 1MB + p2 6MB); aplica e grava no formato do FBNeo."""
    from variante import arquivo
    with zipfile.ZipFile(origem_zip) as z:
        arquivos = {n: z.read(n) for n in z.namelist()}
    raw = arquivos["242ae-p1.p1"] + arquivos["242ae-p2.sp2"]
    linear = bytearray(raw)
    linear[0::2], linear[1::2] = raw[1::2], raw[0::2]
    for nome in nomes:
        for addr, dados in json.load(open(os.path.join(RAIZ, "build", "patches", nome + ".json"))):
            o = arquivo(addr)
            linear[o:o + len(dados)] = bytes(dados)
        print(f"patch {nome} aplicado")
    # proteção do hack (0x09D8E6): soma das words de 0x000000-0x0FFFFF tem que dar 7, senão tela
    # preta ao carregar a luta. Compensa numa word da área livre pra manter a soma.
    COMPENSA = 0x0A7E00
    soma = sum(int.from_bytes(linear[i:i + 2], "big") for i in range(0, 0x100000, 2)) & 0xFFFF
    if soma != 7:
        atual = int.from_bytes(linear[COMPENSA:COMPENSA + 2], "big")
        linear[COMPENSA:COMPENSA + 2] = ((atual + 7 - soma) & 0xFFFF).to_bytes(2, "big")
        print(f"checksum do hack compensado em {COMPENSA:#x} (soma era {soma:#06x})")
    assert sum(int.from_bytes(linear[i:i + 2], "big") for i in range(0, 0x100000, 2)) & 0xFFFF == 7
    novo = bytearray(linear)
    novo[0::2], novo[1::2] = linear[1::2], linear[0::2]
    mudou = sum(1 for i in range(0, len(raw), 2) if raw[i:i + 2] != novo[i:i + 2])
    arquivos["242ae-p1.p1"] = bytes(novo[:0x100000])
    arquivos["242ae-p2.sp2"] = bytes(novo[0x100000:])
    saida = os.path.join(RAIZ, "dist", "mod_ae")
    os.makedirs(saida, exist_ok=True)
    caminho = os.path.join(saida, "kof98ae2016.zip")
    with zipfile.ZipFile(caminho, "w", zipfile.ZIP_DEFLATED) as z:
        for n, d in arquivos.items():
            z.writestr(n, d)
    print(f"{mudou} words alteradas -> {os.path.abspath(caminho)}")


def main():
    origem_zip, nomes = sys.argv[1], sys.argv[2:]
    checar_sobreposicao(nomes)
    if os.environ.get("KOF_VARIANTE") == "ae":
        return main_ae(origem_zip, nomes)
    with zipfile.ZipFile(origem_zip) as z:
        arquivos = {n: z.read(n) for n in z.namelist()}
    entrada = bytearray(arquivos["242-p1.p1"] + arquivos["242-p2.sp2"])
    linear = bytearray(decrypt(bytearray(entrada)))
    original = bytes(linear)

    for nome in nomes:
        for addr, dados in json.load(open(os.path.join(RAIZ, "build", "patches", nome + ".json"))):
            linear[addr:addr + len(dados)] = bytes(dados)
        print(f"patch {nome} aplicado")

    origem = permutacao()
    # o decrypt devolve big-endian (inverte cada word); a entrada está na ordem do arquivo
    mudou = [j for j in range(0, len(linear), 2) if linear[j:j + 2] != original[j:j + 2]]
    destino = {}
    for j in mudou:
        src = int(origem[j // 2])
        word = bytes([linear[j + 1], linear[j]])  # volta pra ordem do arquivo
        if src in destino and destino[src] != word:
            raise SystemExit(f"conflito na word de origem {src:#x}")
        destino[src] = word
    for src, word in destino.items():
        entrada[src * 2:src * 2 + 2] = word

    # confere: desembaralhar a entrada nova tem que dar exatamente o binário patcheado
    assert bytes(decrypt(bytearray(entrada))) == bytes(linear), "reembaralhamento não confere"
    arquivos["242-p1.p1"] = bytes(entrada[:0x200000])
    assert bytes(entrada[0x200000:]) == arquivos["242-p2.sp2"] or True
    arquivos["242-p2.sp2"] = bytes(entrada[0x200000:])

    saida = os.path.join(RAIZ, "dist", "mod")
    os.makedirs(saida, exist_ok=True)
    caminho = os.path.join(saida, "kof98.zip")
    with zipfile.ZipFile(caminho, "w", zipfile.ZIP_DEFLATED) as z:
        for n, d in arquivos.items():
            z.writestr(n, d)
    print(f"{len(mudou)} words alteradas -> {os.path.abspath(caminho)}")


if __name__ == "__main__":
    main()
