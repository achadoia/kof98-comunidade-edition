-- Despeja a struct do P2 (0x200 bytes) em cada quadro entre KOF_T0 e KOF_T1, rodando o teste_jirai.
local T0, T1 = tonumber(os.getenv("KOF_T0") or "118"), tonumber(os.getenv("KOF_T1") or "134")
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local t, out = -1, {}
emu.register_periodic(function()
  t = t + 1
  if t >= T0 and t <= T1 then
    local d = {}
    for i = 0, 0x1FF do d[#d + 1] = string.format("%02x", mem:read_u8(0x108300 + i)) end
    out[#out + 1] = t .. " " .. table.concat(d)
  end
  if t == T1 + 1 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. (os.getenv("KOF_TAG") or "dq") .. "_dump.txt", "w")
    f:write(table.concat(out, "\n"), "\n"); f:close()
  end
end)
dofile("D:/Projects/kof98-recomp/lua/teste_jirai.lua")
