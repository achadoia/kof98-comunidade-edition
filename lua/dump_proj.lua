local sp = manager.machine.devices[":maincpu"].spaces["program"]
local t, out = -1, {}
emu.register_periodic(function() t = t + 1
  if t >= 16 and t <= 26 then local d = {} for i = 0, 0x5F, 2 do d[#d+1] = string.format("%04x", sp:read_u16(0x100B00 + i)) end
    out[#out+1] = t .. " P1x=" .. sp:read_u16(0x108118) .. " " .. table.concat(d, " ") end
  if t == 27 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/dump_proj.txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/teste_projetil_some.lua")
