#!/bin/sh
# Compila o port nativo do KOF98 com o Zig (cc/c++ do clang) + SDL2 (mingw).
set -e
cd "$(dirname "$0")"
ZIG=${ZIG:-/d/Tools/zig/zig.exe}
SDL=${SDL:-/d/Tools/SDL2-2.30.9/x86_64-w64-mingw32}
ALVO="-target x86_64-windows-gnu"
OPT=${OPT:--O2}
# SOLO=1: EXE de arquivo único (SDL2 estática, arte/retratos/patches embutidos via tools/embutir.py, lê a ROM original)
if [ -n "$SOLO" ]; then OPT="$OPT -DKOF_SOLO"; OBJ=${OBJ:-obj_solo}; SAIDA=${SAIDA:-KOF98-Comunidade-Edition.exe}; python ../tools/embutir.py || exit 1; fi
TP=third_party
INC="-Isrc -I$TP/font8x8 -I$TP/Musashi -I$TP/z80 -I$TP/ymfm/src -I$TP/miniz -I$TP/dec -I$SDL/include/SDL2"
mkdir -p ${OBJ:-obj} bin
compila() { # $1 = compilador, $2 = fonte
	o="${OBJ:-obj}/$(echo "$2" | tr '/' '_' | sed 's/\.[^.]*$//').o"
	if [ ! -f "$o" ] || [ "$2" -nt "$o" ] || [ src/neo.h -nt "$o" ]; then
		$ZIG $1 $ALVO $OPT $INC -w -c "$2" -o "$o"
	fi
	OBJS="$OBJS $o"
}
OBJS=""
for f in src/main.c ${SOLO:+src/embutido.c} src/neo.c src/video.c src/rom.c src/estado.c src/m68k_estado.c src/treino.c src/ui.c src/config.c src/entrada.c src/menu_cfg.c src/musica.c src/efeitos.c src/frontend.c src/ordem.c src/dec_impl.c \
         $TP/Musashi/m68kcpu.c $TP/Musashi/m68kops.c $TP/Musashi/m68kdasm.c $TP/Musashi/softfloat/softfloat.c \
         $TP/z80/z80.c $TP/miniz/miniz.c; do compila cc "$f"; done
for f in src/audio.cpp $TP/ymfm/src/ymfm_opn.cpp $TP/ymfm/src/ymfm_adpcm.cpp $TP/ymfm/src/ymfm_ssg.cpp; do compila c++ "$f"; done
if [ -n "$SOLO" ]; then
	$ZIG rc kof98.rc && mv -f kof98.res $OBJ/kof98.res   # ícone do EXE (arte/icone.ico); zig rc grava ao lado do .rc
	$ZIG c++ $ALVO $OBJS $OBJ/kof98.res $SDL/lib/libSDL2.a -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -lgdi32 -luser32 -lkernel32 -o bin/$SAIDA
else
	$ZIG c++ $ALVO $OBJS $SDL/lib/libSDL2.dll.a -o bin/${SAIDA:-kof98.exe}
	cp -u $SDL/bin/SDL2.dll bin/
fi
mkdir -p bin/arte   # arte do menu inicial: PNG no git, BMP ao lado do EXE (SDL lê BMP sem biblioteca extra)
if [ arte/menu_fundo.png -nt bin/arte/menu_fundo.bmp ] && command -v ffmpeg >/dev/null; then ffmpeg -y -loglevel error -i arte/menu_fundo.png -pix_fmt bgr24 bin/arte/menu_fundo.bmp; fi
echo "ok: bin/${SAIDA:-kof98.exe}"
