-- Infinito do Kyo no EXTRA (descrição do usuário): →→ (pulinho) e ↓C durante o pulinho, repetindo
-- assim que pousa. Mede se o P2 fica preso depois do 1º acerto.
-- Env: KOF_N (quadro do pulinho em que aperta ↓C), KOF_DIST (distância inicial), KOF_PATCHES, KOF_TAG.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local TAG = os.getenv("KOF_TAG") or "loop3"
local N = tonumber(os.getenv("KOF_N") or "6")
local DIST = tonumber(os.getenv("KOF_DIST") or "96")
local P1, P2 = 0x108100, 0x108300
local t, log, vida0, vida_ant, acertos, livres, maxcombo, combo = -1, {}, nil, nil, 0, 0, 0, 0
local fase, tf = "dash", 10

local function zera() for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end end

emu.register_periodic(function()
  t = t + 1
  if t < 4 then
    mem:write_u8(P1 + 0x1D5, 1); mem:write_u8(P2 + 0x1D5, 1)
    mem:write_u16(P1 + 0x18, 0x180); mem:write_u16(P2 + 0x18, 0x180 + DIST)
  end
  if t == 4 then vida0 = mem:read_u16(P2 + 0x138); vida_ant = vida0 end
  zera()
  if t < 10 then return end
  local frente = mem:read_u16(P1 + 0x18) < mem:read_u16(P2 + 0x18) and "Right" or "Left"
  local vida = mem:read_u16(P2 + 0x138)
  if vida < vida_ant then acertos = acertos + 1; combo = combo + 1; if combo > maxcombo then maxcombo = combo end end
  vida_ant = vida
  local p2livre = mem:read_u16(P2 + 0x72) == 0
  if acertos >= 1 and p2livre then livres = livres + 1; combo = 0 end
  local d = t - tf
  local y1 = mem:read_u16(P1 + 0x20)
  if fase == "dash" then
    if d < 2 or (d >= 3 and d < 5) then J["P1 " .. frente]:set_value(1) end
    if y1 > 0 then fase, tf = "ar", t end
    if d > 20 then fase, tf = "dash", t end
  elseif fase == "ar" then
    if d >= N and d < N + 3 then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
    if d > 3 and y1 == 0 then fase, tf = "pouso", t end
  elseif fase == "pouso" then
    local a = mem:read_u16(P1 + 0x72)
    if a == 0 or d > 25 then fase, tf = "dash", t end
  end
  log[#log + 1] = string.format("t=%d fase=%s vidaP2=%d y1=%x x1=%x x2=%x p1anim=%04x p2anim=%04x",
    t, fase, vida, y1, mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18), mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72))
  if t >= 600 or vida == 0 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d acertos=%d maior_combo=%d quadros_livres_P2=%d\n", vida0 - vida, acertos, maxcombo, livres),
      table.concat(log, "\n"), "\n")
    f:close(); m:exit()
  end
end)
