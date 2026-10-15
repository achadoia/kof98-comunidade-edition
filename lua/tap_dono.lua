-- Loga PC + A4 de quem escreve em +0xE0..+0xE3 do P1 (achar rotinas de projétil). Env igual teste_projetil_some.
local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local q, out = {t = -1}, {}
TAPD = sp:install_write_tap(0x1081E0, 0x1081E3, "dono", function(off, data, mask)
  if q.t < 200 then out[#out + 1] = string.format("t=%d off=%x data=%04x mask=%04x pc=%06x a4=%06x", q.t, off, data, mask, cpu.state["PC"].value, cpu.state["A4"].value) end
end)
emu.register_periodic(function() q.t = q.t + 1
  if q.t == 200 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/tap_dono.txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/teste_projetil_some.lua")
