@echo off
rem Lancador do KOF98 Anniversary MOD (kof98-recomp).
rem Pelo romdata o FBNeo LE config\games\kof98ae.ini mas GRAVA config\games\kof98aemod.ini;
rem copia a config salva antes de abrir pra controles e velocidade da CPU persistirem.
cd /d "%~dp0"
if exist config\games\kof98aemod.ini copy /y config\games\kof98aemod.ini config\games\kof98ae.ini >nul
start "" fbneo64.exe -romdata -wkof98aemod.dat
