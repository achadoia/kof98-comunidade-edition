-- Grava as estruturas dos dois lutadores ($108100-$1084FF) a cada quadro em build/roteiro/<TAG>.bin
-- (roda junto com roteiro.lua: dofile depois).
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. (os.getenv("KOF_TAG") or "x") .. ".bin", "wb")
emu.register_periodic(function()
  local t = {}
  for a = 0x108100, 0x1084FF, 2 do t[#t + 1] = string.pack(">I2", mem:read_u16(a)) end
  f:write(table.concat(t)); f:flush()
end)
