-- Reproduz o bug "projétil + golpe que zera a guarda" (estado com P1 Athena x P2 Chizuru).
-- Env: KOF_SEGUNDO = input do 2º golpe (ex. "2,3,6B" teleporte; vazio = sem 2º golpe)
--      KOF_PATCHES = patches a aplicar; KOF_TAG = nome da saída.
-- P1 solta Psycho Ball fraca; quando a bola está viva (+0xE1 bit5) e a Athena voltou ao neutro,
-- entra o 2º golpe. P2 só começa a segurar pra trás depois do 2º golpe.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "inbloq"
local P1, P2 = 0x108100, 0x108300

local DIRS = { ["1"] = {"Down", "Left"}, ["2"] = {"Down"}, ["3"] = {"Down", "Right"}, ["4"] = {"Left"},
  ["5"] = {}, ["6"] = {"Right"} }
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
local bola = montar("5,5,2,1,4A")
local segundo = montar(os.getenv("KOF_SEGUNDO") or "")

local t, fase, t_fase, log = -1, "bola", 0, {}
local vida0
local X1, X2 = tonumber(os.getenv("KOF_X1") or ""), tonumber(os.getenv("KOF_X2") or "")
local function aperta(j, nomes)
  local c = P[":edge:joy:JOY" .. j].fields
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c["P" .. j .. " " .. n]:set_value(0) end
  for _, n in ipairs(nomes or {}) do c["P" .. j .. " " .. n]:set_value(1) end
end

emu.register_periodic(function()
  t = t + 1
  if t < 3 and X1 then mem:write_u16(P1 + 0x18, X1); mem:write_u16(P2 + 0x18, X2) end
  if t == 3 then vida0 = mem:read_u16(P2 + 0x138) end
  local viva = mem:read_u8(P1 + 0xE1) & 0x20 ~= 0
  local guarda = mem:read_u8(P1 + 0xE1) & 0x04 ~= 0
  local neutro = mem:read_u16(P1 + 0x72) == 0
  local p1, p2 = nil, nil
  if fase == "bola" then
    p1 = bola[t - t_fase + 1]
    if t - t_fase > #bola and viva and neutro then fase = "segundo"; t_fase = t end
  elseif fase == "segundo" then
    p1 = segundo[t - t_fase + 1]
    if t - t_fase >= #segundo then fase = "defende"; t_fase = t end
  end
  if fase == "defende" then p2 = {"Right"} end  -- P2 está à direita: trás = direita
  aperta(1, p1); aperta(2, p2)
  log[#log + 1] = string.format("t=%d fase=%s bola_viva=%s flag_guarda=%s p1anim=%04x p2anim=%04x vidaP2=%d e0=%02x e1=%02x e3=%02x e5=%02x dist=%04x p2e6=%02x",
    t, fase, tostring(viva), tostring(guarda), mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72), mem:read_u16(P2 + 0x138),
    mem:read_u8(P1 + 0xE0), mem:read_u8(P1 + 0xE1), mem:read_u8(P1 + 0xE3), mem:read_u8(P1 + 0xE5), mem:read_u16(P2 + 0xBC), mem:read_u8(P2 + 0xE6))
  if t >= 150 then
    local vida = mem:read_u16(P2 + 0x138)
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d\n", vida0 - vida), table.concat(log, "\n"), "\n"); f:close()
    m:exit()
  end
end)
