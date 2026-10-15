-- Pra cada (comando, botão): carrega o estado KOF_ESTADO_TESTE, P1 pula pra frente e faz o comando
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
-- no ar; depois de 150 quadros vê se travou (PC na BIOS, timer parado ou P1 sumiu) e fotografa.
-- Saída: build/crash_<TAG>.txt
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local ESTADO = os.getenv("KOF_ESTADO_TESTE") or "ae_eiji"
local TAG = os.getenv("KOF_TAG") or "x"
local CMDS = { "2,3,6", "2,1,4", "6,2,3", "4,2,1", "4,1,2,3,6", "6,3,2,1,4", "2,2" }
if os.getenv("KOF_RAPIDO") then CMDS = { "2,3,6" } end
local BOTOES = { "A", "B", "C", "D" }
if os.getenv("KOF_RAPIDO") then BOTOES = { "A" } end
local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["6"] = {"Right"}, ["8"] = {"Up"}, ["9"] = {"Up", "Right"} }
local casos = {}
for _, c in ipairs(CMDS) do for _, b in ipairs(BOTOES) do casos[#casos + 1] = {c, b} end end

local i, fase, t, seq, bios, timer0 = 0, "carregar", 0, nil, 0, nil
local saida = {}
local function zera() for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end end

local function montar(cmd, botao)
  local s = {}
  for _ = 1, 3 do s[#s + 1] = DIRS["9"] end           -- pulo pra frente
  for _ = 1, 6 do s[#s + 1] = {} end                  -- sobe
  local passos = {}
  for p in cmd:gmatch("%d") do passos[#passos + 1] = p end
  for k, p in ipairs(passos) do
    for _ = 1, 2 do
      local n = {}
      for _, d in ipairs(DIRS[p]) do n[#n + 1] = d end
      if k == #passos then n[#n + 1] = botao end
      s[#s + 1] = n
    end
  end
  return s
end

emu.register_periodic(function()
  zera()
  if fase == "carregar" then
    i = i + 1
    if i > #casos then
      local f = io.open("D:/Projects/kof98-recomp/build/crash_" .. TAG .. ".txt", "w")
      f:write(table.concat(saida, "\n"), "\n"); f:close(); m:exit(); return
    end
    m:load(ESTADO); fase = "rodar"; t = -1; bios = 0; timer0 = nil
    seq = montar(casos[i][1], casos[i][2])
    return
  end
  t = t + 1
  if t == 2 then timer0 = mem:read_u16(0x10a83a) end
  local p = seq[t + 1]
  if p then for _, n in ipairs(p) do J["P1 " .. n]:set_value(1) end end
  local pc = cpu.state["PC"].value
  if t > 30 and pc >= 0xC00000 then bios = bios + 1 end
  if t == 150 then
    local x1 = mem:read_u16(0x108118)
    local anim = mem:read_u16(0x108172)
    local travou = bios > 60
    saida[#saida + 1] = string.format("%s+%s  %s  pc=%06x bios=%d x1=%x anim=%04x",
      casos[i][1], casos[i][2], travou and "TRAVOU" or "ok", pc, bios, x1, anim)
    m.video:snapshot()
    fase = "carregar"
  end
end)
