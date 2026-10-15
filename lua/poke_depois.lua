dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local Q = tonumber(os.getenv("KOF_Q") or "100")
local ja, foto = false, false
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if f >= Q and not ja then ja = true; mem:write_direct_u8(0x0A7000, 0) end
  if f >= 2700 and not foto then foto = true; m.video:snapshot() end
end)
