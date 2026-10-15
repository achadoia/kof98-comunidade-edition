-- Bug do Daimon: Jirai Shin fraco (623A) garantido no levantar (estado ae_daimon: P1 Daimon x P2 Kyo).
-- P1 encosta, dá rasteira (2D) e, KOF_ATRASO quadros depois, solta 623A. P2 segura ↙ (defesa baixa)
-- desde o começo. Registra a vida do P2 e o estado dos dois lutadores por quadro.
-- Env: KOF_ATRASO, KOF_PATCHES, KOF_TAG, KOF_DEFESA ("1" agachado, "4" em pé, "5" nada)
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "jirai"
local ATRASO = tonumber(os.getenv("KOF_ATRASO") or "40")
local DEFESA = os.getenv("KOF_DEFESA") or "1"
local GOLPE = os.getenv("KOF_GOLPE") or "6,2,3A"
local P1, P2 = 0x108100, 0x108300
local VIDA = tonumber(os.getenv("KOF_VIDA") or "0x150")

local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["5"] = {}, ["6"] = {"Right"} }
local function montar(txt)
  local seq = {}
  for passo in (txt or ""):gmatch("[^,]+") do
    local nomes = {}
    for _, d in ipairs(DIRS[passo:sub(1, 1)]) do nomes[#nomes + 1] = d end
    for b in passo:sub(2):gmatch("[ABCD]") do nomes[#nomes + 1] = b end
    for _ = 1, 3 do seq[#seq + 1] = nomes end
  end
  return seq
end
local rasteira = montar(os.getenv("KOF_DERRUBA") or GOLPE)
local jirai = montar(GOLPE)
-- P2 está à direita: trás = direita
local DEF = { ["1"] = {"Down", "Right"}, ["4"] = {"Right"}, ["5"] = {} }

local function aperta(j, nomes)
  local c = P[":edge:joy:JOY" .. j].fields
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c["P" .. j .. " " .. n]:set_value(0) end
  for _, n in ipairs(nomes or {}) do c["P" .. j .. " " .. n]:set_value(1) end
end

local t, log, vida0, t_rast, caiu = -1, {}, nil, 10, false
emu.register_periodic(function()
  t = t + 1
  if t < 3 then mem:write_u16(P1 + 0x18, 300); mem:write_u16(P2 + 0x18, 345) end
  if t == 3 then vida0 = mem:read_u16(P2 + VIDA) end
  local p1
  if t >= t_rast and t < t_rast + #rasteira then p1 = rasteira[t - t_rast + 1] end
  local tj = t_rast + ATRASO
  if t >= tj and t < tj + #jirai then p1 = jirai[t - tj + 1] end
  if mem:read_u16(P2 + 0x72) == 0x133 then caiu = true end
  if t == 3 or t == tj + 109 then
    local d = {}
    for i = 0, 0x1FE, 2 do d[#d + 1] = string.format("%04x", mem:read_u16(P2 + i)) end
    log[#log + 1] = "DUMP " .. table.concat(d, " ")
  end
  aperta(1, p1); aperta(2, caiu and DEF[DEFESA] or nil)
  local b = {}
  for _, o in ipairs({0x7C, 0x7D, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xA0, 0xA1, 0xA2, 0xA3}) do
    b[#b + 1] = string.format("%02x", mem:read_u8(P2 + o))
  end
  log[#log + 1] = string.format("t=%d p1anim=%04x p2anim=%04x p2y=%04x vida=%d p2[7c,7d,e0-e7,a0-a3]=%s",
    t, mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72), mem:read_u16(P2 + 0x20), mem:read_u16(P2 + VIDA),
    table.concat(b, " "))
  if t >= tj + 110 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO atraso=%d dano=%d\n", ATRASO, vida0 - mem:read_u16(P2 + VIDA)),
      table.concat(log, "\n"), "\n")
    f:close()
    m:exit()
  end
end)
