local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local t = 0
local out = {}
TAPS = sp:install_write_tap(0xD00100, 0xD00101, "s", function(off, data, mask)
  out[#out + 1] = string.format("quadro=%d D00100<-%04x pc=%06x", t, data, cpu.state["PC"].value)
end)
emu.register_frame_done(function() t = t + 1
  if t == 400 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/sram100_" .. os.getenv("KOF_TAG") .. ".txt", "w"); f:write(table.concat(out, "\n")); f:close(); manager.machine:exit() end
end)
