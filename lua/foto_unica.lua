dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
local m = manager.machine
local ja = false
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if f >= 2700 and not ja then ja = true; m.video:snapshot() end
end)
