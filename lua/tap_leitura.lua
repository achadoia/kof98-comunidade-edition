-- Loga (pc, offset) de leituras na struct do P1 entre KOF_T0 e KOF_T1. Depois roda o roteiro_livre.
local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local T0, T1 = tonumber(os.getenv("KOF_T0")), tonumber(os.getenv("KOF_T1"))
local q, out = {t = -1}, {}
TAPL = sp:install_read_tap(0x108100, 0x1082FF, "leit", function(off, data, mask)
  if q.t >= T0 and q.t <= T1 then out[#out + 1] = string.format("%d %06x %03x", q.t, cpu.state["PC"].value, off - 0x108100) end
end)
emu.register_periodic(function() q.t = q.t + 1
  if q.t == T1 + 1 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/leit_" .. os.getenv("KOF_TAG") .. ".txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/roteiro_livre.lua")
