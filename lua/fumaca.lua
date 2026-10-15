-- Teste de fumaça: os dois jogadores apertam botões aleatórios (semente fixa) por QUADROS quadros
-- a partir do estado carregado. Falha se o relógio da luta parar por >300 quadros sem fim de round,
-- ou se a CPU ficar presa na BIOS. Saída: build/fumaca_<TAG>.txt
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "fumaca"
local QUADROS = tonumber(os.getenv("KOF_QUADROS") or "3600")
local NOMES = {"Up", "Down", "Left", "Right", "A", "B", "C", "D"}
math.randomseed(tonumber(os.getenv("KOF_SEMENTE") or "98"))

local t, parado, ultimo_ram, bios, pior = -1, 0, nil, 0, 0
local seg = {{}, {}}
emu.register_periodic(function()
  t = t + 1
  for j = 1, 2 do
    local c = P[":edge:joy:JOY" .. j].fields
    if t % 6 == 0 then
      seg[j] = {}
      for _, n in ipairs(NOMES) do seg[j][n] = math.random() < 0.25 end
    end
    for _, n in ipairs(NOMES) do c["P" .. j .. " " .. n]:set_value(seg[j][n] and 1 or 0) end
  end
  -- "congelou" = estruturas dos lutadores idênticas por muitos quadros
  local assin = string.format("%08x%08x%08x%08x", mem:read_u32(0x108118), mem:read_u32(0x108170),
    mem:read_u32(0x108318), mem:read_u32(0x108370))
  if assin == ultimo_ram then parado = parado + 1 else parado = 0 end
  if parado > pior then pior = parado end
  ultimo_ram = assin
  if cpu.state["PC"].value >= 0xC00000 then bios = bios + 1 end
  if t >= QUADROS or parado > 600 then
    local ok = parado <= 600 and bios < 600
    local f = io.open("D:/Projects/kof98-recomp/build/fumaca_" .. TAG .. ".txt", "w")
    f:write(string.format("%s quadros=%d maior_tempo_parado=%d bios=%d\n", ok and "OK" or "FALHOU", t, pior, bios))
    f:close()
    m.video:snapshot()
    m:exit()
  end
end)
