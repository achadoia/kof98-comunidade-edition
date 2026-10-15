dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
local A0 = tonumber(os.getenv("KOF_A0") or "2400")
local A1 = tonumber(os.getenv("KOF_A1") or "2700")
-- Amostra o PC do 68000 por alguns quadros depois do boot (pra achar laço de trava).
local m = manager.machine
local cpu = m.devices[":maincpu"]
local cont, amostras = {}, 0
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if f > A0 and f < A1 then
    local pc = cpu.state["PC"].value
    local k = string.format("%06x", pc & 0xfffff0)
    cont[k] = (cont[k] or 0) + 1
    amostras = amostras + 1
  end
  if f >= A1 then
    local l = {}
    for k, v in pairs(cont) do l[#l + 1] = {k, v} end
    table.sort(l, function(a, b) return a[2] > b[2] end)
    for i = 1, math.min(8, #l) do print("PC " .. l[i][1] .. " x" .. l[i][2]) end
    m:exit()
  end
end)
