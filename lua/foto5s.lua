local m = manager.machine
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if f == 300 or f == 900 then m.video:snapshot() end
  if f >= 905 then m:exit() end
end)
