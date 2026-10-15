// Implementações dos decodificadores de áudio (domínio público): stb_vorbis, dr_mp3, dr_wav.
#include "stb_vorbis.c"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
