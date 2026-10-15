# Interpretador de comandos (especiais) do KOF98

Rotina em `0x01720E` (P1 via `0x01719E`, P2 via `0x0171D6`), chamada todo quadro na luta.

## RAM
| Endereço | O quê |
|---|---|
| `$10DA46` / `$10DA5E` | input do P1 / P2 estável (bits 0-3 ↑↓←→, 4-7 A B C D) |
| `$10E79E` / `$10E9AA` | histórico de 60 quadros do P1 / P2 + direção atual + contador (word) de quadros segurando |
| `$10E760`,`$10E764` | lista de comandos do P1 (uma por lado em que está virado) |
| `$10E768`,`$10E76C` | idem P2 |
| `$10DA4A..4D` / `$10DA62..65` | flags "comando N detectado": byte `N>>2`, bit `(N&3)*2` |

## Formato
Lista = longwords até um valor negativo. Comando = `[n][janela][word limite]` + `n` × `(tipo, valor)`.
`n` com bit 7 = slot vazio. Os passos são conferidos do último para o primeiro, para trás no histórico,
dentro de `janela` quadros. O último passo (gatilho) usa a tabela `0x0172D6`, os outros `0x017312`.

| Tipo | Condição (d = byte do histórico, v = valor) |
|---|---|
| 00 | d == v |
| 04 | d & v ≠ 0 |
| 08 | botões(d) == v |
| 0C / 10 | carga (usa `limite` = quadros segurando) |
| 14 | direção(d) == v |
| 18 | sem botão e d & v ≠ 0 |
| 1C | sem direção e d & v ≠ 0 |
| 20 | direção ∩ v e botão ∩ v |
| 24 | direção == v e botão ∩ v |
| 28 | sem botão e direção == v |
| 2C | direção ∩ v == 0 (direções proibidas) e botão ∩ v |

## Provado (14/10, MAME)
Kyo ↓↘→+P = comando em `0x0B2B3A`: `04 10 0000 14 02 28 0A 14 08 2C F6`.
Reescrito para `03 10 0000 14 02 28 0A 2C F6` → só ↓↘+A solta o 108 Shiki Yami Barai.
Teste: `lua/teste_facil.lua` com `-state vs` (estado versus criado por `lua/salvar_vs.lua`).

## Outros endereços (14/10)
| Endereço | O quê |
|---|---|
| lutador `+0x0E8` (P1 `$1081E8`) | barra de power (word; máx `0x8000` ADV) |
| lutador `+0x1E3` | estoques de power |
| lutador `+0x1DE` / `+0x1E0` | ganho de barra do golpe atual (cada golpe grava constantes) |
| lutador `+0x186` | índice do personagem × 16 (Kyo 0, Yuri 0x80, Chizuru 0xF0) |
| lutador `+0x1D6` | 1 = versão EX/alternativa |
| `$10A84E-50` / `$10A85F-61` | times do P1 / P2 (índices); `$10A85E` = contagem de escolhas |

Ordem dos índices: Kyo, Benimaru, Daimon, Terry, Andy, Joe, Ryo, Robert, Yuri, Leona, Ralf, Clark,
Athena(12), Kensou, Chin, Chizuru(15), Mai, King, Kim, Chang, Choi, Yashiro, Shermie, Chris,
Yamazaki, Mary, Billy, Iori(27), Mature, Vice, Heidern, Takuma(31), Saisyu, Heavy D!, Lucky, Brian, Rugal, Shingo.

Montar luta com qualquer time: `lua/salvar_times.lua` com `KOF_TIME1="12,27,2" KOF_TIME2=...`
(injeta os times entre os quadros 1005 e 1150, logo depois da seleção).
