-- Roteiro de inputs pros dois jogadores a partir do estado carregado.
-- KOF_P1 / KOF_P2: passos separados por vírgula, 3 quadros cada. Passo = numpad + botões,
-- "5" = neutro; "xN" repete o passo N vezes (ex.: "5x10,2,1,4A,6x40").
-- O lado "pra trás" é o jogador quem resolve: use o numpad do ponto de vista da tela.
-- KOF_PATCHES aplica patches. Saída: build/roteiro/<KOF_TAG>.txt (vida/estado por quadro) + fotos.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "roteiro"
local RAIZ = "D:/Projects/kof98-recomp/build/roteiro/"
local FOTOS = {}
for n in (os.getenv("KOF_FOTOS") or ""):gmatch("%d+") do FOTOS[tonumber(n)] = true end

local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["5"] = {}, ["6"] = {"Right"}, ["7"] = {"Up", "Left"}, ["8"] = {"Up"}, ["9"] = {"Up", "Right"} }

local function montar(txt)
  local seq = {}
  for passo in (txt or ""):gmatch("[^,]+") do
    local base, rep = passo:match("^([^x]+)x?(%d*)$")
    local nomes = {}
    for _, d in ipairs(DIRS[base:sub(1, 1)]) do nomes[#nomes + 1] = d end
    for b in base:sub(2):gmatch("[ABCD]") do nomes[#nomes + 1] = b end
    for _ = 1, 3 * (tonumber(rep) or 1) do seq[#seq + 1] = nomes end
  end
  return seq
end
local seqs = { montar(os.getenv("KOF_P1")), montar(os.getenv("KOF_P2")) }
local total = math.max(#seqs[1], #seqs[2]) + 20

local POKES = {}
for a, w, v in (os.getenv("KOF_POKE") or ""):gmatch("(%x+):([bw]):(%x+)") do POKES[#POKES + 1] = {tonumber(a, 16), w, tonumber(v, 16)} end
local inicio, log = nil, {}
emu.register_periodic(function()
  local t = math.floor(m.time:as_double() * 59.1856)
  inicio = inicio or t
  t = t - inicio
  if t < 4 then
    for _, p in ipairs(POKES) do
      if p[2] == "b" then mem:write_u8(p[1], p[3]) else mem:write_u16(p[1], p[3]) end
    end
  end
  for j = 1, 2 do
    local campos = P[":edge:joy:JOY" .. j].fields
    for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do campos["P" .. j .. " " .. n]:set_value(0) end
    local passo = seqs[j][t + 1]
    if passo then for _, n in ipairs(passo) do campos["P" .. j .. " " .. n]:set_value(1) end end
  end
  log[#log + 1] = string.format("t=%3d p1[+138]=%04x p2[+138]=%04x p2[+70]=%04x p2[+72]=%04x p1[+72]=%04x",
    t, mem:read_u16(0x108238), mem:read_u16(0x108438), mem:read_u16(0x108370), mem:read_u16(0x108372), mem:read_u16(0x108172))
  if FOTOS[t] then m.video:snapshot() end
  if t >= total then
    local f = io.open(RAIZ .. TAG .. ".txt", "w"); f:write(table.concat(log, "\n"), "\n"); f:close()
    m:exit()
  end
end)
