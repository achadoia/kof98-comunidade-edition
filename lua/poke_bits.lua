-- A partir do quadro KOF_POKE_T, todo quadro: byte em KOF_POKE_ADDR &= KOF_POKE_AND. Depois roda o roteiro_livre.
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local T, A, M = tonumber(os.getenv("KOF_POKE_T")), tonumber(os.getenv("KOF_POKE_ADDR")), tonumber(os.getenv("KOF_POKE_AND"))
local t = -1
emu.register_periodic(function() t = t + 1
  if t >= T then sp:write_u8(A, sp:read_u8(A) & M) end
end)
dofile("D:/Projects/kof98-recomp/lua/roteiro_livre.lua")
