# Bugs do KOF98 — status das correções

Pesquisa completa (fontes japonesas KCE/atwiki + Yosha) resumida aqui; detalhes no Obsidian.

## Corrigido
### Projétil + teleporte/agarrão de comando = projétil indefensável
Athena (Psycho Ball + Teleport/Psychic Throw), Iori (Yami Barai + Kuzukaze), Takuma (Ko-ou Ken +
Shouran Kyaku), Kensou, Heidern, Rugal, Ura Yuri.
- Causa: decisão de guarda em `0x0148AA` testa `+0xE5` bit 2 do atacante ("golpe indefensável",
  ligado por teleporte/agarrão) antes do `+0xE1` bit 2 ("projétil defensável vivo", ligado em
  `0x024A7C`, desligado em `0x024A5E`).
- Correção: `tools/fix_inbloqueavel.py` (jsr em `0x0148B8` → rotina em `0x0F6540`).
- Teste: `lua/teste_inbloqueavel.lua`, estado `athena`, `KOF_X1=272 KOF_X2=560`.
  Original: 11 de dano sem guarda. Corrigido: guarda + 1 de dano raspado.

### Daimon: Jirai Shin fraco garantido no levantar (vale pra qualquer golpe "meaty")
- Causa: estado de levantar (corrotina em `0x01AC28`). Quando a animação acaba (`+0x7C` bit 7) ele
  chama `0x017140` (tira a invencibilidade: `andi.b #$9F,$E3(a4)`), marca o próximo estado = neutro
  (`0x0165F4`) e **cede o quadro** (`bra $16526` em `0x01AC62`). A guarda só é decidida no neutro,
  no quadro seguinte; no meio-tempo a colisão acontece com o lutador vulnerável e sem guarda.
  O tremor do Jirai Shin fica ativo muitos quadros, então acertar esse quadro é fácil.
- Correção: `tools/fix_levantar.py` — `bra $16526` vira `bra $165F4` (entra no neutro no mesmo
  quadro; o jogo já faz isso no caminho de `0x01AC42`). Mesmo endereço no kof98 e no ae.
- Teste: `lua/teste_jirai.lua`, estado `ae_daimon` (Daimon x Kyo), Jirai → 2º Jirai com
  `KOF_ATRASO` 70..98. Original: atraso 86 = 19 de dano com o Kyo segurando ↙. Corrigido: 0 em todos
  (guarda 1 quadro mais cedo). Sem segurar defesa o Jirai continua acertando normal.
- Vida no ae fica em `+0x150` (no kof98 original `+0x138`).

### Daimon: "frota invencível" (corrigido 15/10)
- Causa: quadros de ataque com `+0x7C` bit 6 (ex.: J.B do Daimon) ligam `+0xE2` bit 7 no atacante quando
  conectam (colisão em `0x003B2A`). Todo quadro `0x017004` apaga a caixa de dano (`+0x7C` bit 0) enquanto
  esse bit estiver ligado; ele só é limpo no reset de estado (`0x0170D8`, fim do pouso). Se o oponente
  encosta um golpe no quadro do pouso, o lutador entra direto na guarda (`0x014928` em pé / `0x014B72`
  agachado), que não faz o reset → sem caixa de dano até mexer o direcional.
- Correção: `tools/fix_frota.py` — as duas entradas de guarda limpam `+0xE2` bit 7 (rotina em CAVE+0x200).
- Teste: `lua/roteiro_livre.lua` (inputs por quadro pros 2 jogadores) estado `ae_daimon`:
  `KOF_X1=300 KOF_X2=345 KOF_P1="10-12:9,17-19:5B,20-260:1"` + P2 apertando 2A a cada 6 quadros.
  Original: o 2A do Kyo atravessa o Daimon (sem contato). Corrigido: Daimon defende (anim 0024).
  Resumo do log: `tools/resumo_livre.py`.

## Investigados sem reproduzir (15/10) — precisa de teste jogando
- **Projétil sumindo apaga a guarda (tipo 2):** a flag de ameaça `+0xE0` bit 0 é compartilhada; projéteis
  ligam em `0x024A7C` (todo quadro) e desligam em `0x024A5E` ao sumir. Testado com o Yami Barai da Iori
  sumindo no meio de C em pé e de pulo+D (varredura quadro a quadro): o oponente SEMPRE defende (a guarda
  no impacto não depende da flag). O bug deve ficar restrito aos golpes citados (Ebony Tears, Voltic
  Launcher, clones do Sanrai da Chizuru, Chris Orochi, Ura Robert). Sem correção aplicada.
- **Perder caixa de arremesso ao anular projétil:** Aragami do Kyo anulando o Yami Barai deixa `+0xE5`
  bit 1 ligado, mas a Iori agarra o Kyo normalmente depois (estado `ae_kyo_iori`). Sem correção.
- Pendentes ainda não estudados: Athena (Phoenix Arrow+A, flutuar, fuga da tela), Chizuru (limitador,
  GCCD no Sanrai).

## Balanceamento
- Chizuru: 30% menos barra (`tools/chizuru.py`).
- Golpes facilitados + carga 20% menor (`tools/facil.py`).

## Pendentes (ver pesquisa)
- Athena: Phoenix Arrow com A junto = dano maior; bug de flutuar; fuga pra fora da tela.
- Iori: Yami Barai indefensável por timing exato (fim da animação = chegada do projétil).
- Projétil/clone sumindo apaga a guarda de outro golpe (Chizuru Sanrai, Mature, Leona, Chris...).
- Chizuru: "limitador desligado" e invencibilidade após GCCD durante o Sanrai.
- Perder a caixa de arremesso ao anular projétil com golpe que não é projétil.

## Modo EXTRA: laço pulinho (→→) + ↓C no canto (Kyo, também Mai)
- Vídeo de referência do usuário: Kyo EX x Robert no canto; o Robert fica preso defendendo até tontear.
- Medido: o pulinho leva 25 quadros até poder agir; com o ↓C apertado no fim do pulinho o golpe
  sai no pouso, e o oponente que tenta fugir pulando é pego pelo ↓C (antiaéreo).
- Correção aplicada: `tools/fix_extra_pulinho.py` (+6 quadros no pouso do pulinho, só no EXTRA;
  25 → 31 quadros). **Não foi possível provar 100% com roteiro de input**: validar jogando.

## ROM jogável
`tools/gerar_rom.py <kof98.zip> facil chizuru inbloqueavel extra_pulinho levantar` → `dist/mod/kof98.zip`
(código reembaralhado no formato MVS; conferido no MAME). Atalho: `dist/jogar_kof98_mod.bat`.
Lentidão: no MAME, Tab → Slider Controls → "Overclock CPU :maincpu" em 200%. No port nativo some.

## KOF98 Anniversary 2016 (kof98ae2016, hack EGCG/GSC2007) — base padrão a partir de 15/10
- Mesmo motor do kof98 nos mesmos endereços (interpretador 0x01720E, guarda 0x0148AA, pulinho
  0x0153A4...). Código sem criptografia: p1 1MB + p2 6MB; na luta a janela 0x200000 = banco 1.
- Mais personagens via variantes e IDs novos (ex.: vaga 12 = Kasumi no ae); Chizuru continua 15.
- Os 2 pontos de soma de barra 0x015606/0x015648 foram desviados pelo hack (rotinas no banco 1
  com condição `$7318(a4)`); o patch da Chizuru copia essas rotinas com a escala.
- Área livre usada pelos patches: 0x09F000+ (0x0F6500 o hack usa).
- **Proteção do hack** em 0x09D8E6: soma das words de 0x000000-0x0FFFFF tem que dar 7, senão
  tela preta ao carregar a luta. `gerar_rom.py` compensa em 0x0A7E00.
- Ferramentas: `KOF_VARIANTE=ae` em todos os tools/*.py; laboratório no MAME via lista local
  `hash/neogeo.xml` (`neogeo -cart1 kof98ae2016`), sem o v5 (MAME limita ADPCM-A a 16MB).
- Gerar: `KOF_VARIANTE=ae python tools/gerar_rom.py D:/kof98ae2016.zip chizuru_ae inbloqueavel_ae extra_pulinho_ae levantar_ae frota_ae nome_terry_ae` (origem: `D:/Tools/FBNeo/roms/kof98ae.zip`, o D:/kof98ae2016.zip saiu da raiz)
  (**sem `facil_ae`** — pedido do usuário 15/10: o Anniversary fica com os comandos originais; o facil
  fazia meia-lua pra trás do Iori sair outro golpe)
  → `dist/mod_ae/kof98ae2016.zip` (formato FBNeo/Frame Perfect).

### Crash do Eiji (15/10) — erro meu, corrigido
Na versão ae a rotina extra da Chizuru (CAVE+0xA0) se sobrepunha ao fim da rotina do pulinho
(CAVE+0x80..0xA3); ao ganhar barra o jogo pulava pra código corrompido e congelava (relatado:
Eiji pulando + especial). Agora: rotinas da Chizuru ae em CAVE+0x100, `gerar_rom.py` recusa
patches sobrepostos, e todo build passa pelo teste de fumaça `lua/fumaca.lua` (botões aleatórios
1 min em 5 confrontos: ae_vs, ae_eiji, ae_chizuru, ae_athena, ae_daimon).

Elenco alternativo do ae (variante 1): 1 Orochi, 10 Geese, 12 Kasumi, 13 Mr. Big, 18 Krauser,
24 Goenitz, 32 Eiji (ninja). Vagas 9/20/30/31 com variante 1 não carregam luta.

## Jogar sem o aviso de "versão do jogo" (15/10)
O Frame Perfect (e o arcade.exe dele, um FBNeo personalizado) recusa ROM com checksum diferente.
Solução: FBNeo oficial em `D:\Tools\FBNeo` com as ROMs ORIGINAIS (roms/kof98.zip, roms/kof98ae.zip =
cópia do kof98ae2016.zip, roms/neogeo.zip) + mods como patch IPS aplicado ao carregar:
- `tools/gerar_ips.py <zip original> <zip modificado> <jogo> D:/Tools/FBNeo` → support/ips/<jogo>/kof98mod.dat + .ips
- lista de ativos pré-escrita em config/ips/<jogo>.ini
- atalhos na Área de Trabalho (OneDrive\Desktop): "KOF98 Anniversary MOD.bat" / "KOF98 MOD.bat"
  (`fbneo64.exe kof98ae -ips kof98mod`). Som completo do Anniversary (v5) no FBNeo.

### Atalho do Anniversary no FBNeo (corrigido 15/10)
O driver `kof98ae` do FBNeo traz as revisões 2016 e 2007 e exige os `.dif` da 2007 como essenciais
→ "essential program ROM 242-p1ae.dif was not found". IPS não resolve. Agora o Anniversary roda como
**romdata**: `tools/gerar_romdata.py dist/mod_ae/kof98ae2016.zip D:/kof98.zip D:/Tools/FBNeo` gera
`roms/kof98aemod.zip` (autocontido, p1 já modificado) + `support/romdata/-wkof98aemod.dat`.
Atalho: `fbneo64.exe -romdata -wkof98aemod.dat` (o "-w" no nome = abrir em janela; o FBNeo lê os 2
caracteres depois de -romdata como opção). KOF98 MOD: `fbneo64.exe kof98 -w -ips kof98mod`.
Tela cheia direto pela linha de comando falha às vezes ("Problem setting 1280x720x16bpp display mode");
por isso janela + `bVidDX9WinFullscreen 1` (Alt+Enter = tela cheia sem trocar o modo do monitor).

### Config do FBNeo não salvava + lentidão (15/10)
- Pelo romdata o FBNeo LÊ `config/games/kof98ae.ini` e GRAVA `config/games/kof98aemod.ini` → controles e
  velocidade da CPU nunca voltavam. Atalho agora chama `D:/Tools/FBNeo/KOF98 Anniversary MOD.bat`
  (cópia em `tools/kof98_anniversary_fbneo.bat`), que copia aemod → ae antes de abrir. Testado: troca de
  botão sobrevive a abrir/fechar.
- Lentidão (estouro de barra + combos com muitos efeitos) é do hardware Neo Geo emulado fielmente.
  `cpu 0x0200` nas configs por jogo (kof98.ini, kof98aemod.ini) = 68000 a 200%. No menu: Game → Adjust CPU speed.
