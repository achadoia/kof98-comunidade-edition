local sp = manager.machine.devices[":maincpu"].spaces["program"]
local t, out = -1, {}
emu.register_periodic(function() t = t + 1
  if (t >= 20 and t <= 70) or t == 100 then local d = {} for i = 0x88, 0xAB do d[#d+1] = string.format("%02x", sp:read_u8(0x108100 + i)) end; out[#out+1] = string.format("t=%d 7c=%02x P1[88..ab]=%s", t, sp:read_u8(0x10817C), table.concat(d)) end
  if t == 108 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/dir_" .. os.getenv("KOF_TAG") .. ".txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/roteiro_livre.lua")
