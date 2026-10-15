-- Testa um input no estado carregado (-state vs), com os patches de KOF_PATCHES.
-- Env: KOF_SEQ="2,3,3A" (numpad + botões, 3 quadros cada), KOF_TAG=nome
-- Saída: build/golpes/<tag>.txt (flags de comando que acenderam) + snapshots.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local TAG = os.getenv("KOF_TAG") or "teste"
local RAIZ = "D:/Projects/kof98-recomp/build/"

dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")

local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["5"] = {}, ["6"] = {"Right"}, ["7"] = {"Up", "Left"}, ["8"] = {"Up"}, ["9"] = {"Up", "Right"} }
local seq = {}
for passo in (os.getenv("KOF_SEQ") or "2,3,3A"):gmatch("[^,]+") do
  local nomes = {}
  for _, d in ipairs(DIRS[passo:sub(1, 1)]) do nomes[#nomes + 1] = d end
  for b in passo:sub(2):gmatch("[ABCD]") do nomes[#nomes + 1] = b end
  for _ = 1, 3 do seq[#seq + 1] = nomes end
end

local inicio, log, fotos = nil, {}, {}
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  inicio = inicio or f
  local t = f - inicio
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end
  local p = seq[t - 9]
  if p then for _, n in ipairs(p) do J["P1 " .. n]:set_value(1) end end
  local flags = string.format("%02x%02x%02x%02x", mem:read_u8(0x10DA4A), mem:read_u8(0x10DA4B),
    mem:read_u8(0x10DA4C), mem:read_u8(0x10DA4D))
  if flags ~= log.ult then log[#log + 1] = string.format("t=%d flags=%s", t, flags); log.ult = flags end
  for _, s in ipairs({#seq + 18, #seq + 30}) do
    if t >= s and not fotos[s] then fotos[s] = true; m.video:snapshot() end
  end
  if t >= #seq + 32 then
    local out = io.open(RAIZ .. "golpes/" .. TAG .. ".txt", "w")
    out:write(table.concat(log, "\n"), "\n"); out:close()
    m:exit()
  end
end)
