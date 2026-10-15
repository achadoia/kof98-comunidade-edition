-- Conta leituras da ROM fixa (0x000000-0x0FFFFF) e do banco (0x200000-0x2FFFFF) por PC entre os
-- quadros A0 e A1 (seleção -> carga da luta). Rotina de checksum = PC com leituras em massa.
dofile("D:/Projects/kof98-recomp/lua/salvar_vs.lua")
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local A0, A1 = tonumber(os.getenv("KOF_A0") or "1000"), tonumber(os.getenv("KOF_A1") or "1400")
local cont, ativo = {}, false
local function conta() if ativo then local pc = cpu.state["PC"].value; cont[pc] = (cont[pc] or 0) + 1 end end
TAP_F = mem:install_read_tap(0x000000, 0x0FFFFF, "rom_fixa", function() conta() end)
TAP_B = mem:install_read_tap(0x200000, 0x2FFFFF, "rom_banco", function() conta() end)
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  ativo = f >= A0 and f < A1
  if f >= A1 and not FEITO then
    FEITO = true
    local l = {}
    for k, v in pairs(cont) do l[#l + 1] = {k, v} end
    table.sort(l, function(a, b) return a[2] > b[2] end)
    for i = 1, math.min(12, #l) do print(string.format("LEITOR %06x x%d", l[i][1], l[i][2])) end
    m:exit()
  end
end)
