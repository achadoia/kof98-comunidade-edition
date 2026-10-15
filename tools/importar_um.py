"""Importa músicas, vozes e efeitos do KOF '98 Ultimate Match Final Edition (instalação do usuário, GOG)
pras pastas trocáveis do port. Nada disso vai pro git: é áudio da SNK, só da máquina de quem tem o UM.

Músicas (BGM_wavebank.xwb): as faixas 07XX usam o MESMO código XX do driver de som do KOF98
(0721 abertura, 0723 seleção, 0728 menu, 0732 luta do Practice...) -> som/musica/<XX> UM.ogg com o
LOOPSTART/LOOPLENGTH do UM. As outras (TITLE_N, KOF96GEES...) são extras do UM -> som/musica_um_extras/.

Vozes e efeitos (WaveBank.xwb): o nome termina no comando do nosso jogo ("015D_A1E" = 1A 1E, "30A3_0CC" =
efeito CC). Mesmo assim cada som é CONFERIDO: o kof98_dev.exe gera o som do chip pra aquele comando
(KOF_SO_SOM, sem o 68k) e só entra o do UM que tiver o espectrograma igual (o Anniversary mexeu em alguns
códigos e o UM tem versões repetidas). Volume ajustado som a som pelo do chip. -> som/efeitos/<PXX>.wav

Uso: python tools/importar_um.py [--musicas] [--efeitos] ["D:/The King of Fighters 98UM"]   (padrão: os dois)
Requer vgmstream-cli (D:/Tools/vgmstream), ffmpeg no PATH e port/bin/kof98_dev.exe (build KOF_TESTE).
"""
import os, re, subprocess, sys, tempfile, wave, shutil
from concurrent.futures import ThreadPoolExecutor
import numpy as np

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
UM = ARGS[0] if ARGS else 'D:/The King of Fighters 98UM'
JOGO = os.path.join(UM, 'Data', 'game')
VGM = os.environ.get('VGMSTREAM', 'D:/Tools/vgmstream/vgmstream-cli.exe')
BIN = os.path.join(RAIZ, 'port', 'bin')
DEV = os.path.join(BIN, 'kof98_dev.exe')
TMP = os.path.join(RAIZ, 'build', 'importar_um')
GANHO_MUSICA = 0.38     # UM é ~2,6x mais alto que o chip (mediana de 8 faixas, 15/10)
VINHETAS = {0x24, 0x2c, 0x2e, 0x2f, 0x49, 0x50, 0x51, 0x56}   # tocam uma vez (ver port/src/musica.c)
NOTA_MIN = 0.8          # semelhança mínima de espectrograma chip x UM (1291 de 1389 deram > 0,9 em 15/10)
QPS = 59.1856


def meta(xwb, i):
    out = subprocess.run([VGM, '-m', '-s', str(i), xwb], capture_output=True, text=True).stdout
    m = {}
    for chave, rx in (('nome', r'stream name: (?:\S+/)?(\S+)'), ('total', r'stream count: (\d+)'),
                      ('ls', r'loop start: (\d+)'), ('le', r'loop end: (\d+)'), ('n', r'stream total samples: (\d+)')):
        r = re.search(rx, out)
        if r:
            m[chave] = r.group(1)
    return m


def le_wav(f):
    w = wave.open(f)
    sr, ch = w.getframerate(), w.getnchannels()
    a = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64).reshape(-1, ch).mean(1)
    w.close()
    return a, sr


# ---------------------------------------------------------------- músicas
def musicas():
    xwb = os.path.join(JOGO, 'BGM_wavebank.xwb')
    saida = os.path.join(BIN, 'som', 'musica')
    extras = os.path.join(BIN, 'som', 'musica_um_extras')
    os.makedirs(saida, exist_ok=True)
    os.makedirs(extras, exist_ok=True)
    wav = os.path.join(TMP, 'm.wav')
    for i in range(1, int(meta(xwb, 1)['total']) + 1):
        m = meta(xwb, i)
        nome = m['nome']
        cod = int(nome[2:], 16) if re.fullmatch(r'07[0-9A-F]{2}', nome) else None
        musica = cod is not None and 0x21 <= cod <= 0x56
        destino = os.path.join(saida, '%02x UM.ogg' % cod) if musica else os.path.join(extras, nome + '.ogg')
        subprocess.run([VGM, '-s', str(i), '-i', '-o', wav, xwb], capture_output=True, check=True)   # -i: sem repetir
        ls, le = int(m.get('ls', 0)), int(m.get('le', 0))
        tags = []
        if 'le' in m and le > ls and cod not in VINHETAS:
            tags = ['-metadata', 'LOOPSTART=%d' % ls, '-metadata', 'LOOPLENGTH=%d' % (le - ls)]
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-af', 'volume=%.2f' % GANHO_MUSICA,
                        '-c:a', 'libvorbis', '-q:a', '5', '-metadata', 'TITLE=KOF98UM %s' % nome] + tags + [destino],
                       check=True)
        print('música %-14s -> %s' % (nome, os.path.relpath(destino, RAIZ)), flush=True)


# ---------------------------------------------------------------- vozes e efeitos
FB = None


def espec(a, sr):
    """log-espectrograma de 24 bandas (100-7500 Hz) a 16 kHz, quadros de 10 ms"""
    global FB
    a = np.interp(np.linspace(0, len(a) - 1, int(len(a) * 16000 / sr)), np.arange(len(a)), a)
    N, H = 512, 160
    if len(a) < N:
        a = np.pad(a, (0, N - len(a)))
    fr = np.lib.stride_tricks.sliding_window_view(a, N)[::H] * np.hanning(N)
    S = np.abs(np.fft.rfft(fr, axis=1)) ** 2
    if FB is None:
        f = np.fft.rfftfreq(N, 1 / 16000)
        ed = np.geomspace(100, 7500, 25)
        FB = np.array([(f >= ed[i]) & (f < ed[i + 1]) for i in range(24)], float)
    return np.log(S @ FB.T + 1e3)


def nota(Sc, Su):
    """correlação dos log-espectrogramas, alinhando até ±150 ms"""
    n = min(len(Sc), len(Su))
    if n < 5:
        return 0.0
    melhor = -1.0
    for d in range(-15, 16):
        A, B = (Sc[d:d + n], Su) if d >= 0 else (Sc, Su[-d:-d + n])
        m = min(len(A), len(B))
        if m < 5:
            continue
        A = A[:m] - A[:m].mean()
        B = B[:m] - B[:m].mean()
        melhor = max(melhor, float((A * B).sum() / np.sqrt((A * A).sum() * (B * B).sum() + 1e-9)))
    return melhor


def corta(a, lim=300):
    nz = np.nonzero(np.abs(a) > lim)[0]
    return a[nz[0]:nz[-1] + 1] if len(nz) else a[:0]


def chip(cod):
    """som do chip pro comando (KOF_SO_SOM, só Z80+YM, depois da inicialização do driver)"""
    p, x = cod[0], cod[1:]
    seq = '60:%s' % x if p == '0' else '60:1%s,61:%s' % (p, x)
    wav = os.path.join(TMP, 'chip_%s.wav' % cod)
    env = dict(os.environ, KOF_SO_SOM='1:3,10:3,12:3,18:7,' + seq, KOF_SO_CHIP='1')
    subprocess.run([DEV, '--sem-janela', '--quadros', '330', '--wav', wav], env=env, cwd=BIN, capture_output=True)
    a, sr = le_wav(wav)
    os.remove(wav)
    return corta(a[int(60 / QPS * sr):]), sr


def efeitos():
    xwb = os.path.join(JOGO, 'WaveBank.xwb')
    saida = os.path.join(BIN, 'som', 'efeitos')
    um_dir = os.path.join(TMP, 'um')
    os.makedirs(um_dir, exist_ok=True)
    total = int(meta(xwb, 1)['total'])
    print('extraindo %d sons do UM...' % total, flush=True)

    def extrai(i):
        f = os.path.join(um_dir, '%d.wav' % i)
        if not os.path.exists(f):
            subprocess.run([VGM, '-s', str(i), '-o', f, xwb], capture_output=True)
        return i, meta(xwb, i)['nome']
    por_cod = {}
    with ThreadPoolExecutor(8) as ex:
        for i, nome in ex.map(extrai, range(1, total + 1)):
            suf = nome.split('_')[-1]
            if re.fullmatch(r'[06-9A-E][0-9A-F]{2}', suf):
                por_cod.setdefault(suf, []).append(i)
    print('%d códigos de voz/efeito; gerando o som do chip de cada um...' % len(por_cod), flush=True)

    def avalia(cod):
        a, sr = chip(cod)
        if len(a) < sr * 0.03:
            return cod, None, 'chip mudo'
        Sc, rc = espec(a, sr), np.sqrt((a ** 2).mean())
        melhor = (-1.0, None, 0.0)
        for i in por_cod[cod]:
            u, su = le_wav(os.path.join(um_dir, '%d.wav' % i))
            u = corta(u)
            if len(u) < su * 0.03:
                continue
            s = nota(Sc, espec(u, su))
            if s > melhor[0]:
                melhor = (s, i, rc / np.sqrt((u ** 2).mean()))
        if melhor[0] < NOTA_MIN:
            return cod, None, 'diferente (%.2f)' % melhor[0]
        return cod, melhor, 'ok'

    if os.path.isdir(saida):
        shutil.rmtree(saida)
    os.makedirs(saida)
    ok = 0
    with ThreadPoolExecutor(8) as ex:
        for cod, m, motivo in ex.map(avalia, sorted(por_cod)):
            if not m:
                print('  %s fica com o chip: %s' % (cod, motivo), flush=True)
                continue
            s, i, g = m
            g = min(max(g, 0.2), 0.6)                 # volume do chip (ganho típico 0,28-0,42)
            u, su = le_wav(os.path.join(um_dir, '%d.wav' % i))
            pcm = np.clip(np.round(u * g), -32768, 32767).astype('<i2')
            w = wave.open(os.path.join(saida, '%s.wav' % cod), 'wb')
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(su)
            w.writeframes(pcm.tobytes())
            w.close()
            ok += 1
    print('vozes/efeitos: %d de %d trocados pelo UM -> %s' % (ok, len(por_cod), os.path.relpath(saida, RAIZ)))


if __name__ == '__main__':
    os.makedirs(TMP, exist_ok=True)
    flags = [a for a in sys.argv[1:] if a.startswith('--')]
    if not flags or '--musicas' in flags:
        musicas()
    if not flags or '--efeitos' in flags:
        efeitos()
