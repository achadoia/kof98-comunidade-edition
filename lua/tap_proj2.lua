-- registra todos os objetos que chamam 0x024A7C (projétil vivo) e 0x024A5E (projétil morreu) pro dono P1
local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local q, out = {t = -1}, {}
TAPP = sp:install_write_tap(0x1081E0, 0x1081E3, "p2", function(off, data, mask)
  local pc = cpu.state["PC"].value
  if (pc >= 0x024A5E and pc <= 0x024A98) then out[#out + 1] = string.format("t=%d pc=%06x obj=%06x", q.t, pc, cpu.state["A4"].value) end
end)
emu.register_periodic(function() q.t = q.t + 1
  if q.t == 160 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/tap_proj2_" .. (os.getenv("KOF_TAG") or "") .. ".txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/teste_projetil_some.lua")
