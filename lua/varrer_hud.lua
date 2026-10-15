-- Pra cada byte de BASE..BASE+N: carrega o estado "vs", escreve 0x7F por 8 quadros e fotografa.
-- Depois tools/comparar_hud.py vê qual foto mudou a barra de power.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local BASE = tonumber(os.getenv("KOF_BASE") or "0x108100")
local N = tonumber(os.getenv("KOF_N") or "0x200")
local i, fase, cont = -1, "carregar", 0

emu.register_periodic(function()
  if fase == "carregar" then
    i = i + 1
    if i > N then m:exit() return end
    m:load("vs"); fase = "poke"; cont = 0
  elseif fase == "poke" then
    if i > 0 then mem:write_u8(BASE + i - 1, 0x7F) end  -- i=0 = foto de referência
    cont = cont + 1
    if cont >= 8 then m.video:snapshot(); fase = "carregar" end
  end
end)
