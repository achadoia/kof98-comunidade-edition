-- Igual ao teste_jirai, mas registra o PC de quem escreve em campos do P2 entre KOF_T0 e KOF_T1.
local T0, T1 = tonumber(os.getenv("KOF_T0") or "125"), tonumber(os.getenv("KOF_T1") or "136")
local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local quadro = { t = -1 }
_G.TAP_QUADRO = quadro
local saida = {}
local CAMPOS = {}
for c in (os.getenv("KOF_CAMPOS") or "0xE4,0x72,0x7C"):gmatch("[^,]+") do CAMPOS[#CAMPOS + 1] = tonumber(c) end
TAPS = {}
for _, c in ipairs(CAMPOS) do
  local a = 0x108300 + c
  TAPS[#TAPS + 1] = sp:install_write_tap(a & ~1, (a & ~1) + 1, "t" .. c, function(off, data, mask)
    if quadro.t >= T0 and quadro.t <= T1 then
      local a7 = cpu.state["SP"].value
      saida[#saida + 1] = string.format("t=%d campo=%x off=%06x data=%04x mask=%04x pc=%06x pilha=%08x %08x %08x", quadro.t, c, off, data, mask, cpu.state["PC"].value, sp:read_u32(a7), sp:read_u32(a7 + 4), sp:read_u32(a7 + 8))
    end
  end)
end
emu.register_periodic(function()
  quadro.t = quadro.t + 1
  if quadro.t == T1 + 1 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. (os.getenv("KOF_TAG") or "tap") .. "_tap.txt", "w")
    f:write(table.concat(saida, "\n"), "\n"); f:close()
  end
end)
dofile("D:/Projects/kof98-recomp/lua/teste_jirai.lua")
