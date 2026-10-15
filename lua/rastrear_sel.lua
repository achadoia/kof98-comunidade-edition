-- Roda o boot do versus registrando toda escrita nos bytes de time ($10A84E-50, $10A85F-61)
-- e no índice de personagem dos lutadores (+0x186).
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local frame = 0
local log = {}
local function reg(nome) return function(off, data, mask)
  log[#log + 1] = string.format("f=%d %s %06x <- %04x (mask %04x) pc=%06x", frame, nome, off, data, mask, cpu.state["PC"].value)
end end
TAP1 = mem:install_write_tap(0x10a840, 0x10a87f, "times", reg("sel"))
TAP2 = mem:install_write_tap(0x1082d6, 0x1082d7, "p1", reg("p1_1d6"))
TAP3 = mem:install_write_tap(0x108486, 0x108487, "p2", reg("p2_186"))
dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
emu.register_periodic(function()
  frame = math.floor(m.time:as_double() * 59.1856)
  if frame >= 3040 and not FEITO then
    FEITO = true
    local f = io.open("D:/Projects/kof98-recomp/build/rastro_sel.txt", "w")
    f:write(table.concat(log, "\n")); f:close()
  end
end)
