-- roda salvar_vs (boot até a luta) e fotografa a cada 200 quadros de 2400 a 4000
dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
local m = manager.machine
local ja = {}
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if f >= 2400 and f % 200 == 0 and not ja[f] then ja[f] = true; m.video:snapshot() end
end)
