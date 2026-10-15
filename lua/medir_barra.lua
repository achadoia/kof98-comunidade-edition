-- Aplica patches (KOF_PATCHES), Kyo anda e bate na Yuri; grava a barra do P1 ($1081E8) no fim.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local inicio
emu.register_periodic(function()
  local t = math.floor(m.time:as_double() * 59.1856)
  inicio = inicio or t
  t = t - inicio
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end
  if t >= 10 and t < 50 then J["P1 Right"]:set_value(1) end
  if t >= 60 and t < 400 then
    if t % 14 < 3 then J["P1 C"]:set_value(1) else J["P1 Right"]:set_value(1) end
  end
  if t >= 410 then
    print(string.format("BARRA P1=%04x estoque=%d", mem:read_u16(0x1081E8), mem:read_u8(0x1082E3)))
    m:exit()
  end
end)
