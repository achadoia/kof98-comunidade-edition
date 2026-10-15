"""Resume um log do lua/roteiro_livre.lua: uma linha por mudança de anim/vida dos dois lutadores."""
import re
import sys

for arq in sys.argv[1:]:
    linhas = open(arq).read().splitlines()
    print(f"== {arq}: {linhas[0]}")
    ant = None
    for l in linhas[1:]:
        t = re.search(r"t=(\d+)", l).group(1)
        x = re.search(r"x1=(\d+) x2=(\d+)", l).groups()
        p = re.findall(r"anim=(\w+) y=(\w+) vida=(\d+) e0-e7=(\w+) 7c=(\w+)", l)
        chave = (p[0][0], p[0][2], p[1][0], p[1][2])
        if chave != ant:
            print(f"t={t:>4} x={x[0]}/{x[1]}  P1 anim={p[0][0]} y={p[0][1]} vida={p[0][2]} e={p[0][3]}  |  "
                  f"P2 anim={p[1][0]} vida={p[1][2]} e={p[1][3]}")
            ant = chave
