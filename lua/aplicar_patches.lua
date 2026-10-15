-- Aplica os patches listados em KOF_PATCHES (nomes em build/patches/, separados por vírgula).
local mem = manager.machine.devices[":maincpu"].spaces["program"]
for nome in (os.getenv("KOF_PATCHES") or ""):gmatch("[^,]+") do
  local txt = io.open("D:/Projects/kof98-recomp/build/patches/" .. nome .. ".json"):read("a")
  for addr, bytes in txt:gmatch("%[(%d+), %[([%d, ]+)%]%]") do
    local i = 0
    for b in bytes:gmatch("%d+") do mem:write_direct_u8(tonumber(addr) + i, tonumber(b)); i = i + 1 end
  end
end
