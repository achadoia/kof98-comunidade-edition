-- Bug: projétil sumindo (fora da tela) apaga a flag de ameaça (+0xE0 bit0) de OUTRO golpe do dono,
-- e o oponente perde a guarda. Estado ae_athena (P1 Athena x P2 Chizuru).
-- P1 solta Psycho Ball (236A); a bola fica "estacionada" no alto (não acerta ninguém) até o P1 começar
-- um C em pé colado; KOF_K quadros depois do C apertado a bola é jogada pra fora da tela e some.
-- P2 segura trás o tempo todo. KOF_K=-1: bola nunca some (controle).
-- Env: KOF_K, KOF_PATCHES, KOF_TAG, KOF_GOLPE2 (padrão "5C")
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "projsome"
local K = tonumber(os.getenv("KOF_K") or "4")
local VIDA = tonumber(os.getenv("KOF_VIDA") or "0x150")
local P1, P2 = 0x108100, 0x108300

local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["5"] = {}, ["6"] = {"Right"}, ["7"] = {"Up", "Left"}, ["8"] = {"Up"}, ["9"] = {"Up", "Right"} }
local function montar(txt)
  local seq = {}
  for passo in (txt or ""):gmatch("[^,]+") do
    local nomes = {}
    for _, d in ipairs(DIRS[passo:sub(1, 1)]) do nomes[#nomes + 1] = d end
    for b in passo:sub(2):gmatch("[ABCD]") do nomes[#nomes + 1] = b end
    for _ = 1, 3 do seq[#seq + 1] = nomes end
  end
  return seq
end
local bola = montar(os.getenv("KOF_BOLA") or "2,3,6A")
local golpe2 = montar(os.getenv("KOF_GOLPE2") or "5C")
local function aperta(j, nomes)
  local c = P[":edge:joy:JOY" .. j].fields
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c["P" .. j .. " " .. n]:set_value(0) end
  for _, n in ipairs(nomes or {}) do c["P" .. j .. " " .. n]:set_value(1) end
end

-- o projétil liga +0xE1 bit5 no dono (rotina 0x024A7C, a4 = projétil): pega o ponteiro ali
local proj
TAP = mem:install_write_tap(P1 + 0xE0, P1 + 0xE1, "proj", function(off, data, mask)
  local pc = cpu.state["PC"].value
  if pc >= 0x024A7C and pc <= 0x024A98 and not proj then proj = cpu.state["A4"].value end
end)

local t, fase, t_fase, log, vida0, t_golpe, parado = -1, "bola", 0, {}, nil, nil, nil
emu.register_periodic(function()
  t = t + 1
  if t < 3 then mem:write_u16(P1 + 0x18, tonumber(os.getenv("KOF_X1") or "200")); mem:write_u16(P2 + 0x18, tonumber(os.getenv("KOF_X2") or "600")) end
  if t == 0 then parado = mem:read_u16(P1 + 0x72) end
  if t == 3 then vida0 = mem:read_u16(P2 + VIDA) end
  local p1
  if fase == "bola" then
    p1 = bola[t - t_fase + 1]
    if proj then fase = "espera"; t_fase = t end
  elseif fase == "espera" then
    if t - t_fase == 1 then mem:write_u16(P2 + 0x18, mem:read_u16(P1 + 0x18) + tonumber(os.getenv("KOF_DIST") or "45")) end  -- traz o P2 pra perto
    if mem:read_u16(P1 + 0x72) == parado and t - t_fase > 5 then fase = "golpe"; t_fase = t; t_golpe = t end
  elseif fase == "golpe" then
    p1 = golpe2[t - t_fase + 1]
  end
  -- estaciona a bola no alto, longe do P2, até a hora de sumir
  local some = K >= 0 and t_golpe and t >= t_golpe + K
  if proj and not some then
    mem:write_u16(proj + 0x18, mem:read_u16(P1 + 0x18) - 40)
    mem:write_u16(proj + 0x20, 0xC0)
  elseif proj and some then
    mem:write_u16(proj + 0x18, 0x0800)
  end
  aperta(1, p1); aperta(2, {"Right"})
  log[#log + 1] = string.format("t=%d fase=%s p1anim=%04x p2anim=%04x vida=%d p1e0=%02x p1e1=%02x p1e3=%02x proj=%s",
    t, fase, mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72), mem:read_u16(P2 + VIDA),
    mem:read_u8(P1 + 0xE0), mem:read_u8(P1 + 0xE1), mem:read_u8(P1 + 0xE3), proj and string.format("%06x", proj) or "-")
  if (t_golpe and t >= t_golpe + 60) or t > 400 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO K=%d dano=%d golpe_em=%s\n", K, vida0 - mem:read_u16(P2 + VIDA), tostring(t_golpe)),
      table.concat(log, "\n"), "\n")
    f:close()
    m:exit()
  end
end)
