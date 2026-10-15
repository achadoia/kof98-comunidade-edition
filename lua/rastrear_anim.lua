-- Registra o PC que grava cada valor novo em +0x72 (animação) do P1 durante um pulinho do EXTRA.
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local P1 = 0x108100
local t, log = -1, {}
TAPA = mem:install_write_tap(P1 + 0x72, P1 + 0x73, "anim", function(off, data, mask)
  if data ~= 0 then log[#log + 1] = string.format("t=%d anim<-%04x pc=%06x", t, data, cpu.state["PC"].value) end
end)
emu.register_periodic(function()
  t = t + 1
  if t < 4 then mem:write_u8(P1 + 0x1D5, 1) end
  J["P1 Right"]:set_value(0)
  if t >= 10 and (t < 13 or (t >= 15 and t < 18)) then J["P1 Right"]:set_value(1) end
  if t >= 50 then print(table.concat(log, "\n")); m:exit() end
end)
