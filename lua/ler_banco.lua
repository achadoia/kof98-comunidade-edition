local mem = manager.machine.devices[":maincpu"].spaces["program"]
emu.register_periodic(function()
  local s = {}
  for a = 0x263b50, 0x263b6f do s[#s + 1] = string.format("%02x", mem:read_u8(a)) end
  local t = {}
  for a = 0x200000, 0x20000f do t[#t + 1] = string.format("%02x", mem:read_u8(a)) end
  print("BANCO 263b50=" .. table.concat(s) .. " 200000=" .. table.concat(t))
  manager.machine:exit()
end)
