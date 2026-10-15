-- Confere se a ROM carregada tem os patches e chega numa luta.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
emu.register_periodic(function()
  local fr = math.floor(m.time:as_double() * 59.1856)
  if fr == 2990 then
    print(string.format("PATCH inbloqueavel@148b8=%04x%08x chizuru@155b4=%04x pulinho@153b4=%04x facil@b2b3a=%02x chizuru@15606=%04x%08x",
      mem:read_u16(0x148b8), mem:read_u32(0x148ba), mem:read_u16(0x155b4), mem:read_u16(0x153b4), mem:read_u8(0xb2b3a), mem:read_u16(0x15606), mem:read_u32(0x15608)))
  end
end)
