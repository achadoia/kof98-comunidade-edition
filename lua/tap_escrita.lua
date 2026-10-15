-- Loga escritas (pc, endereço, dado, máscara) em KOF_TAP_INI..KOF_TAP_FIM entre KOF_T0 e KOF_T1; depois roda roteiro_livre.
local cpu = manager.machine.devices[":maincpu"]
local sp = cpu.spaces["program"]
local A0, A1 = tonumber(os.getenv("KOF_TAP_INI")), tonumber(os.getenv("KOF_TAP_FIM"))
local T0, T1 = tonumber(os.getenv("KOF_T0")), tonumber(os.getenv("KOF_T1"))
local q, out = {t = -1}, {}
TAPE = sp:install_write_tap(A0 & ~1, A1 | 1, "esc", function(off, data, mask)
  if q.t >= T0 and q.t <= T1 then
    local s = cpu.state["SP"].value
    out[#out + 1] = string.format("t=%d pc=%06x end=%06x dado=%04x masc=%04x pilha=%08x %08x", q.t, cpu.state["PC"].value, off, data, mask, sp:read_u32(s), sp:read_u32(s + 4))
  end
end)
emu.register_periodic(function() q.t = q.t + 1
  if q.t == T1 + 1 then local f = io.open("D:/Projects/kof98-recomp/build/roteiro/esc_" .. os.getenv("KOF_TAG") .. ".txt", "w"); f:write(table.concat(out, "\n")); f:close() end
end)
dofile("D:/Projects/kof98-recomp/lua/roteiro_livre.lua")
