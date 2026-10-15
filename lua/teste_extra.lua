local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local t = 0
emu.register_periodic(function()
  t = t + 1
  if t < 5 then mem:write_u8(0x1082D5, 1); mem:write_u8(0x1084D5, 1) end
  if t == 30 then m.video:snapshot() end
  if t >= 31 then m:exit() end
end)
