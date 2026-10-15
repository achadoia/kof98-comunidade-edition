"""Monta roms_ae_mod/neogeo/kof98ae2016.zip (formato da lista local do MAME) a partir de dist/mod_ae."""
import os
import zipfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
lab = zipfile.ZipFile(os.path.join(RAIZ, "roms_ae", "neogeo", "kof98ae2016.zip"))
mod = zipfile.ZipFile(os.path.join(RAIZ, "dist", "mod_ae", "kof98ae2016.zip"))
os.makedirs(os.path.join(RAIZ, "roms_ae_mod", "neogeo"), exist_ok=True)
with zipfile.ZipFile(os.path.join(RAIZ, "roms_ae_mod", "neogeo", "kof98ae2016.zip"), "w", zipfile.ZIP_STORED) as z:
    for n in lab.namelist():
        z.writestr(n, mod.read(n) if n in mod.namelist() else lab.read(n))
