# KOF98 Anniversary — port nativo pra PC

EXE próprio (`bin/kof98.exe`) que roda o KOF98 Anniversary 2016 sem emulador externo. A ROM **não**
vem dentro do EXE: ele lê os zips do usuário ao abrir (`roms/kof98ae2016.zip` + `roms/kof98.zip` + `roms/neogeo.zip`
ao lado do EXE, ou zips passados na linha de comando). O `kof98.zip` original é obrigatório: o Anniversary é clone e
pega c3/c4 e v1-v4 dele (18/10: sem eles a Yuri ficava invisível e o chip tocava vozes trocadas/ruído — `rom.c`
agora recusa set incompleto, posiciona cada V ROM pelo índice e prefere os arquivos `242ae-*` quando o nome repete).

## Controles (padrão — tudo muda no F1)
| | P1 | P2 |
|---|---|---|
| Direcional | setas | I J K L |
| A B C D | Z X C V | Num4 Num5 Num1 Num2 |
| A+B / C+D / A+B+C | A / S / D | Num7 / Num8 / Num9 |
| Start / Moeda | 1 / 5 | 2 / 6 |

Controle (qualquer um que o Windows veja: XInput, DirectInput, arcade stick): direcional e analógico,
X=A, Y=B, A=C, B=D, LB=A+B, RB=C+D, Start, Back=moeda. Cima+baixo ou esquerda+direita juntos = neutro.

## Teclas
F1 configurações · F6 treino avançado · F4 troca o filtro · F11/Alt+Enter tela cheia · F5/F9 salva/carrega
estado · F7/F8 grava/toca o boneco · F3 reset · F12 foto · Esc sai.

### Configurações (F1)
Salvas em `kof98.ini` ao lado do EXE. Abas (Tab, ou LB/RB no controle). Desde 19/10 o F1 e o F6 são desenhados em
alta resolução (1672x941, `src/hd.c`, artes `arte/hd_*.png` embutidas no EXE único) por cima do jogo: o F1 com a
arte `hd_cfg_fundo.png` de fundo, o F6 como painel translúcido com o título `hd_treino_titulo.png`. Os dois menus
navegam com as setas E com as teclas/botões configurados dos 2 jogadores (direcional, A/Start = ok) e com os
botões fixos do controle (direcional/analógico, A, B volta, LB/RB abas) — `entrada_evento_menu()` em `src/entrada.c`.
Teste: build `-DKOF_TESTE` com `KOF_CFG=1..4` / `KOF_TREINO=menu=1`, `KOF_JANELA=1 KOF_TAM=1600,900 KOF_AUTOFOTO=0,x.bmp`
(`KOF_SEM_CONTROLE=1` ignora um controle com botão preso; testes com KOF_AUTOFOTO nunca gravam o kof98.ini).
- **Vídeo**: filtro (xBR 4x na CPU em 4 threads — `src/xbr.c`, padrão —, nítido, suave, linhas de TV, tubo CRT, fliperama — scanlines e grade RGB feitas na GPU
  por textura em blend multiplicativo), proporção (4:3, pixel inteiro, tela toda), laterais borradas,
  tela cheia, vsync, FPS.
- **Controles**: por jogador; Enter numa ação grava a próxima tecla/botão/eixo/direcional (Del apaga);
  "Configurar tudo" pede uma por uma; a ação acende em verde quando apertada (teste ao vivo);
  até 3 ligações por ação (teclado + controle); restaurar padrão.
- **Som**: volume, mudo.
- **Melhorias**: pausar sem foco, início rápido (pula o logo), dica do F6, **run-ahead** (0-2 quadros).
- **Fluidez além do original (18/10)**: `run_ahead` emula N quadros à frente com a entrada atual, mostra o último e
  volta ao estado real por snapshot em memória (`estado_mem_salvar/carregar`, `estado_leve` = sem a trilha trocável,
  `musica_silenciosa` = os quadros à frente não disparam arquivo nenhum e mandam o mesmo pro Z80); o som sai do
  quadro real. Teste: `KOF_RUNAHEAD=N` no modo sem janela tem que dar `--salvar-estado` idêntico ao sem run-ahead.
  `ritmo_monitor`: o jogo roda na taxa do monitor (60, 120 = 2 apresentações por quadro) com vsync e o YM é
  reamostrado 1,4% acima (`audio_fator_ritmo`); a fila de som só corrige a deriva (quadro a mais/a menos, ±1,5%).
  Monitor sem múltiplo de 59,19 Hz (144) = opção fica "incompatível" e vale o ritmo original.
  `sprites_sem_limite`: `video_limite_linha` = 381 em vez dos 96 do hardware (nada some em cena cheia).

Abre em **modo console (AES)**: MODE SELECT do jogo de casa (Team Play, Team VS, Single, Survivor,
Practice, Option). `--arcade` volta ao modo fliperama (MVS, com fichas).

### Menu inicial "Ultimate Match - Comunidade Edition" (`src/frontend.c`)
**O MODE SELECT original nunca fica na tela** (18/10): `frontend_vigia()` lê a RAM todo quadro e, quando o jogo
chega sozinho no MODE SELECT (Start no título, volta do OPTION, fim de jogo), abre o nosso menu por cima sem reset,
já no estado "pronto pra escolher" (`q_rel = Q_MENU`). Bytes do Anniversary achados por diff de dumps: `$10A790` tela
(0 título, 1 MODE SELECT/OPTION, 2 jogo), `$10A789` 00 parado no MODE SELECT / 0x80 ao confirmar e no OPTION / 03-83 no
título, `$10051C` 1 dentro do OPTION, `$1085D2` cursor do MODE SELECT (0-6; o piloto sobe ou desce a partir dele, então
voltando do OPTION também funciona). Teste: `KOF_VIGIA_TESTE=1` sem janela imprime os quadros em que abriria.
Sons do menu: cursor = comando 70 e confirmar = 71 do próprio MODE SELECT (o avanço rápido espera 30 quadros pra o som sair).
**Nenhuma tela do jogo entre o menu e a seleção** (18/10): depois de confirmar o modo, o piloto continua em avanço
rápido apertando A a cada 24 quadros enquanto `$10A790 == 1` (tela CONTROLS etc.) e só entrega o jogo 40 quadros
depois de `$10A790 == 2` (seleção de personagem). OPTION é exceção (fica no MODE SELECT). Teste: `KOF_DEBUG_PILOTO=1`.

### ORDER SELECT em duas etapas (`src/ordem.c`)
18/10 noite: etapa 1 "ESCOLHA O MODO" (cursor nos 3 retratos, <> troca o modo de cada um, A confirma) e etapa 2 "ESCOLHA A ORDEM"
(A/B/C secreto). O modo no Anniversary é POR PERSONAGEM: cada confirmação grava o modo da caixa naquela posição e a caixa
volta pra ADVANCED (esquerda 1x = SPECIAL, 2x = EXTRA); a confirmação final grava o do 3º. Provado: módulo e sequência manual
equivalente dão os mesmos bytes ($10D938 e $10A854-56). Detalhes antigos abaixo.

#### Versão anterior (v1.2.0)
No ORDER SELECT cada jogador aperta UMA vez: **A** = o 1º personagem que escolheu começa, **B** = o 2º, **C** = o 3º
(os outros seguem na ordem da escolha). O EXE traduz na sequência original (cima/baixo giram a coluna, A confirma o de
baixo, 2 confirmações; a 2ª só vale ~40 quadros depois) e congela as duas colunas na imagem de antes de qualquer
escolha até os dois confirmarem o modo, então o adversário não vê nada. Depois da ordem a tela original segue:
esquerda/direita = ADVANCED/EXTRA/SPECIAL, A confirma (3ª confirmação). RAM: `$1005C7 == 0x10` só nessa tela,
`$1005CE` fase (5 = aceita entrada), `$10A84D/$10A85E` confirmações P1/P2, `$10A854-56/$10A865-67` ordem final
(índice na escolha). Opção "Ordem do trio por botão" no F1 → Melhorias (`ordem_botao`). Lab: estado da tela de ordem
= Team VS por roteiro (`700-704:st=1,960-963:p1=2,1080-1084:p1=10,1300-1303:p1=10,...`), pronta a partir de ~1712.
Arte 1672x941 em `arte/menu_fundo.png` (gerada no ChatGPT pelo usuário; o build converte pra `bin/arte/menu_fundo.bmp`).
O jogo roda por trás: um piloto aperta Start no título (quadro 700 após o reset) e espera no MODE SELECT (pronto no 960;
não volta sozinho pro título). Ao escolher, desce até o item e aperta A em avanço rápido (12 quadros por giro, som
descartado) com "CARREGANDO..." e só então mostra o jogo. ARCADE PLAY = Single Play, TEAM PLAY = Team Play, 2P VS = Team VS,
SURVIVAL = Survivor, PRACTICE = Practice, OPTIONS = Option do jogo, WATCH = abertura/demonstração, EXIT sai, COOP "em breve"
(o KOF98 não tem). **Esc no jogo volta pro menu**; Esc no menu fecha. Sem a arte, abre direto no jogo.
Teste (build KOF_TESTE): `KOF_MENU_ESCOLHA=<índice do item>` + `KOF_AUTOFOTO`; `KOF_RESET=1` no modo sem janela.

### Retratos personalizados (`arte/retratos.json` -> `tools/retratos.py` -> `bin/arte/troca_tiles.bin`)
O renderizador pinta tiles da ROM escolhidos (pelo código) com pixels ARGB próprios em vez da paleta (alfa 0 =
pixel original). Terry Urameshi: grade da seleção (tiles 7B66-68/7B70-72, área 26x27 em x118-143 y14-40) e prévia
do P1 (9F86-89 normal, F1EF-F2 segurando Start; x19-44 y184-210). Achar tiles: build de teste com
`KOF_MAPA="quadro,arq.bin"` grava, por pixel, `código<<8 | linha<<4 | coluna` do tile que o pintou.
Nome completo: `tools/renomear.py TERRY BOGARD URAMESHI` (campos fixos de 8 letras) -> patch `nome_terry_ae`.

### Fotos em alta resolução (`tools/fotos.py` -> `bin/arte/overlay.bin` + `fotos.bin`)
18/10: além da troca de tiles (pixelada), o renderizador anota por quadro onde os tiles de cada alvo do `retratos.json`
foram pintados (`video_ovl`: origem exata da arte pelo deslocamento do tile, caixa visível no fim do quadro, espelhado)
e o `main.c` desenha o PNG da foto (recorte x8) por cima, no tamanho da janela, só na parte visível, antes da camada de
interface (`tela_ui`, transparente: treino, F1, tela de ordem). Alvos com `sem_foto` ficam só na troca de tiles; `sem_troca`
só foto. Artes da tela de ordem (96x96, uma por personagem, as 3 posições) estão mapeadas mas desligadas a pedido do
usuário. Lab: `KOF_MAPA` + roteiro Team VS; na seleção o cursor anda sozinho uma casa pra direita a cada escolha.

### Trilha trocável (`bin/som/musica/`)
Arquivo `.ogg`/`.mp3`/`.wav` com o nome começando pelo código da música (`28.ogg`, `"32 Esaka.mp3"`) toca no
lugar da música do chip; sem arquivo, toca a original. F1 → Som: liga/desliga, "Reler a pasta" e
"Tocando: XX" (código atual). Repete o arquivo inteiro, ou o trecho LOOPSTART/LOOPLENGTH (amostras) do OGG.
Vinhetas (24 2C 2E 2F 49 50 51 56) tocam uma vez. Implementação em `src/musica.c` (stb_vorbis, dr_mp3, dr_wav).
- Códigos do driver de som (varredura 15/10): música 0x21-0x56 (0x4B = 0x21); 0x20 para a música; 01/03/04/0D
  param tudo; 02 fade ~6 s; 0A xx fade curto; 05 0A 0E 15 16-1E levam 1 byte de parâmetro (16-1E = vozes), 14 leva 2.
  Com arquivo, o código vira 0x20 pro Z80 e os efeitos continuam no chip.
- Conhecidos: 21 abertura, 28 menu/como jogar, 23 seleção, 32 luta do Practice.
- **Gravar a música do chip com loop perfeito não dá**: cada canal do driver tem loop próprio com duração um
  pouco diferente (ex.: 7,146 s × 7,152 s na 0x28), então o som nunca se repete exato. Por isso a pasta vem
  vazia e o chip continua sendo o padrão; arquivos são pra trilha oficial ou personalizada.
- Teste (build KOF_TESTE): `KOF_SO_SOM="1:3,10:3,12:3,18:7,60:28"` roda só Z80+YM (sem 68k) mandando comandos
  por quadro; `KOF_DEBUG_SOM=1` lista os comandos do jogo; `KOF_LOG_YM=arq` grava as escritas no YM2610.

### Sons do KOF '98 Ultimate Match (`tools/importar_um.py`)
Importa da instalação GOG do UM (`D:\The King of Fighters 98UM`, via vgmstream em `D:\Toolsgmstream`):
- **Músicas**: `BGM_wavebank.xwb` faixas `07XX` = código `XX` do nosso driver (mesmo jogo). Vão pra
  `som/musica/XX UM.ogg` com o loop do UM e volume 0,38 (UM é ~2,6x mais alto). Extras do UM em `som/musica_um_extras/`.
- **Vozes/efeitos**: `WaveBank.xwb` nome `xxxx_PXX` = comando `1P XX` (P=0: efeito de 1 byte `XX`). Cada um é
  conferido contra o som do chip (espectrograma, nota >= 0,8; 1305 de 1416 batem) e vai pra `som/efeitos/PXX.wav`
  com o volume do chip. Os que não batem ficam no chip (códigos que o Anniversary mudou).
- No EXE (`src/efeitos.c`): prefixo 16-1E fica retido até o parâmetro; com arquivo, toca aqui e nada vai pro Z80
  (o 68k não espera resposta: tela/estado idênticos ao som 100% chip no quadro 2600). Fila de bytes pro Z80 entrega
  um por vez. Um canal por prefixo + 7 em rodízio pros efeitos de 1 byte. F1 → Som liga/desliga cada pasta.
- `KOF_SO_CHIP=1` força o som original (usado pela importação). Áudio do UM é da SNK: nunca vai pro git nem junto do EXE.

### Treino avançado (F6)
Menu do próprio EXE por cima do jogo (pausa enquanto aberto; ^v escolhe, <> muda, Z/Enter confirma):
hitboxes (vermelho = ataque, azul = corpo), boneco P2 (em pé, agachado, defende em pé/agachado, pula),
gravação do adversário (**F7** grava controlando o P2, **F8** toca/para, repetir em loop), histórico de
comandos em notação numérica, vida/barra infinitas, velocidade da CPU, **F5** salva estado / **F9** carrega.
Hitboxes: 4 caixas por lutador em `+0x90` (tipo, x, y, meia-largura, meia-altura), ativas por `+0x7C`
bits 0-3; caixa 0 = ataque; posição na tela em `+0x24`/`+0x26`.

## Opções
- CPU do 68000 **fixa em 200%** (sem a lentidão do Neo Geo; não há como reduzir)
- teste sem janela: `--sem-janela --quadros N --foto x.bmp --wav som.wav --seg-entradas "700-705:co=1,760-765:st=1,900-904:p1=10"`
  (alvos p1/p2/st/co, bits ativos em hexa; mesmo formato do `lua/roteiro_boot.lua` no MAME → comparação quadro a quadro)

## Arquitetura (`src/`)
- `rom.c` — acha os arquivos nos zips pela extensão (.p1/.sp2/.s1/.m1/.vN/.cN + BIOS sp-s2/sfix/sm1/000-lo).
- `neo.c` — mapa de memória do 68000, E/S, trava do sistema (HC259), RAM de backup, RTC mínimo,
  **watchdog** (o jogo depende do reset dele na inicialização) e agendador de 264 linhas/quadro.
- `video.c` — LSPC: VRAM com auto-incremento, sprites (381, zoom, cadeias, animação automática),
  camada fixa, paleta (rede de resistores), timer de raster (IRQ2).
- `audio.cpp` — Z80 + YM2610 (ymfm), latches de comando/resposta, NMI, bancos da M1.
- `main.c` — SDL2: janela, som, teclado/controle, 59,19 Hz.

Bibliotecas (MIT/BSD, `./baixar_deps.sh`): Musashi (68000), superzazu/z80, ymfm (YM2610), miniz.
Referência de hardware: driver Neo Geo do MAME (BSD-3) em `../ref/mame/`.

## Fases
1. **Runtime próprio** (feito 15/10): o jogo inteiro roda no EXE, gráficos idênticos ao MAME,
   som, 130+ quadros/s com a CPU a 200%. A CPU 68000 ainda é interpretada (Musashi).
2. **Recompilação 68000 → C**: o código do jogo vira C nativo compilado; o Musashi fica só de reserva
   pra endereços não traduzidos.
3. Extras: Pix na live, mudanças direto no código, widescreen etc.

Build: `./build.sh` (Zig 0.13 em D:\Tools\zig + SDL2 2.30.9 mingw em D:\Tools).

## Jogo online (`src/rede.c`)

Ponto a ponto por UDP (porta 7998+). Endereço = IP público + porta (STUN em `stun.l.google.com`) + IP e porta
da rede local, 12 bytes em base32 sem letras ambíguas (20 caracteres) — vai pro **cadastro de salas**
`https://achadoia.com/api/kof/sala` (Next.js no repo achadoia-site, tabela `public.kof_salas` no Supabase pessoal,
validade 2 h), que devolve o código curto de 3 números + 3 letras. O convidado consulta o código, recebe o
endereço do anfitrião e registra o dele; o anfitrião consulta a cada 2 s e passa a mandar OLA pro convidado
(furo do NAT dos dois lados, sem digitação). HTTPS pelo WinHTTP numa thread (`sala_poll`). O anfitrião também
tenta abrir a porta por UPnP (SSDP + SOAP, numa thread). Pareamento: os dois mandam `OLA` pros endereços
candidatos a cada 200 ms; quem recebe responde; PING/PONG mede a latência; o anfitrião manda `INICIO` com o
atraso (automático = ping/2 em quadros + 1, mínimo 2) e o convidado responde `PRONTO`. Os dois então fazem
`frontend_online_comeca()`: reset + piloto pro 2P VS no mesmo quadro lógico.

Lockstep com atraso fixo: a entrada local do quadro Q vale pra Q+atraso; cada pacote `ENTRADA` leva as últimas
entradas (até 64, desde o último ack do outro lado), então perda de pacote não trava. `rede_pode()` diz quantos
quadros seguidos já têm entrada dos dois; o laço de `main.c` limita `passos` por isso. Byte extra por quadro:
bit0 Start, bit1 "escolha do menu" (item nos bits 4-7) — assim a escolha do anfitrião no menu entre lutas chega
nos dois lados no mesmo quadro. A cada 120 quadros vai um hash da RAM de trabalho (0x100000-0x10FFFF); se
diferir em 2 checagens seguidas, `rede_dessincronizou` acende o aviso (uma divergência isolada acontece
no aperto de mão do som e some sozinha). Na partida: run-ahead, treino, cenário fixo e reset desligados;
ordem por botão segue a config do anfitrião nos dois lados. Teste local (build KOF_TESTE):
`KOF_REDE=criar` numa instância e `KOF_REDE=entrar:127.0.0.1:7998` em outra, `KOF_DEBUG_REDE=1` imprime o estado.

## Auto-update (`src/atualiza.c`, versão em `src/versao.h`)

Ao abrir (só no EXE SOLO, ou `KOF_ATUALIZA=1`), uma thread lê `api.github.com/repos/achadoia/kof98-comunidade-edition/releases/latest`;
se `tag_name` > `KOF_VERSAO`, o menu mostra o aviso. ENTER baixa o asset `.exe` pra `<exe>.novo` (WinHTTP segue o
redirect do GitHub), e quando termina o jogo grava `atualizar_kof98.bat` (espera o PID fechar, `move /y`, `start`) e
sai. Teste: `KOF_VERSAO_FAKE=1.0.0` finge versão antiga. **Toda release precisa subir o `.exe` como asset e a tag
`vX.Y.Z` igual a `versao.h`.**
