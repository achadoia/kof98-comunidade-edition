-- Lê o id de personagem (+0x1D6) e a variante (+0x186) dos dois lutadores no estado carregado.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
emu.register_periodic(function()
  for _, b in ipairs({0x108100, 0x108300}) do
    print(string.format("obj %06x id=%02x variante=%04x", b, mem:read_u8(b + 0x1D6), mem:read_u16(b + 0x186)))
  end
  m:exit()
end)
