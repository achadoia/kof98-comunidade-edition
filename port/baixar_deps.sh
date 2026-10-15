#!/bin/sh
# Baixa as bibliotecas de terceiros do port (todas MIT/BSD) em port/third_party.
set -e
cd "$(dirname "$0")"
mkdir -p third_party && cd third_party
[ -d Musashi ] || git clone -q --depth 1 https://github.com/kstenerud/Musashi.git
[ -d font8x8 ] || git clone -q --depth 1 https://github.com/dhepper/font8x8.git
[ -d z80 ] || git clone -q --depth 1 https://github.com/superzazu/z80.git
[ -d ymfm ] || git clone -q --depth 1 https://github.com/aaronsgiles/ymfm.git
if [ ! -f miniz/miniz.c ]; then
	mkdir -p miniz && cd miniz
	curl -sfL -o m.zip https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip && unzip -qo m.zip && rm m.zip
	cd ..
fi
if [ ! -f dec/dr_wav.h ]; then   # decodificadores da trilha trocável (domínio público)
	mkdir -p dec
	curl -sfL -o dec/stb_vorbis.c https://raw.githubusercontent.com/nothings/stb/f58f558c120e9b32c217290b80bad1a0729fbb2c/stb_vorbis.c
	curl -sfL -o dec/dr_mp3.h https://raw.githubusercontent.com/mackron/dr_libs/da35f9d6c7374a95353fd1df1d394d44ab66cf01/dr_mp3.h
	curl -sfL -o dec/dr_wav.h https://raw.githubusercontent.com/mackron/dr_libs/da35f9d6c7374a95353fd1df1d394d44ab66cf01/dr_wav.h
fi
cd Musashi && [ -f m68kops.c ] || { ${ZIG:-/d/Tools/zig/zig.exe} cc -O1 -o m68kmake.exe m68kmake.c && ./m68kmake.exe . m68k_in.c; }
echo "deps ok"
