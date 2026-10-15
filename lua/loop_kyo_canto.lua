-- Infinito do Kyo EX no EXTRA (vídeo do usuário): Robert encurralado no canto e agachado;
-- Kyo faz →→ (pulinho) e ↓C ao pousar, repetindo. Depois do 1º acerto o P2 segura ↙ (defesa
-- agachada): se o dano continuar entrando, é infinito real.
-- Env: KOF_POUSO (quadros após pousar até o ↓C), KOF_PATCHES, KOF_TAG.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local J2 = m.ioport.ports[":edge:joy:JOY2"].fields
local TAG = os.getenv("KOF_TAG") or "canto"
local POUSO = tonumber(os.getenv("KOF_POUSO") or "0")
local ESPERA = tonumber(os.getenv("KOF_ESPERA") or "0")
local NOAR = tonumber(os.getenv("KOF_NOAR") or "-1")
local P1, P2 = 0x108100, 0x108300
local t, log, vida0, vida_ant, acertos, maxcombo, combo, livres = -1, {}, nil, nil, 0, 0, 0, 0
local fase, tf = "posicionar", 0

local function zera(c, p) for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c[p .. n]:set_value(0) end end

emu.register_periodic(function()
  t = t + 1
  if t < 4 then mem:write_u8(P1 + 0x1D5, 1) end   -- Kyo no EXTRA
  zera(J, "P1 "); zera(J2, "P2 ")
  local x1, x2 = mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18)
  local frente = x1 < x2 and "Right" or "Left"
  local tras2 = x2 > x1 and "Right" or "Left"
  local vida = mem:read_u16(P2 + 0x138)
  if fase == "posicionar" then
    J2["P2 " .. tras2]:set_value(1)
    if x2 - x1 > 0x40 then J["P1 Right"]:set_value(1) end
    if t >= 200 then fase, tf = "dash", t; vida0 = vida; vida_ant = vida end
  else
    -- P2: agachado até o 1º acerto, depois defesa agachada
    local FUGA = os.getenv("KOF_FUGA") == "1"
    if FUGA then
      -- tenta escapar o tempo todo: pulo pra trás (se estiver preso na defesa, só defende)
      J2["P2 Up"]:set_value(1); J2["P2 " .. tras2]:set_value(1)
      if mem:read_u16(P2 + 0x20) > 0 then FUGIU = (FUGIU or 0) + 1 end
    else
      J2["P2 Down"]:set_value(1)
      if acertos >= 1 then J2["P2 " .. tras2]:set_value(1) end
    end
    if vida < vida_ant then acertos = acertos + 1; combo = combo + 1; if combo > maxcombo then maxcombo = combo end end
    vida_ant = vida
    if acertos >= 1 and mem:read_u16(P2 + 0x72) == mem:read_u16(P2 + 0x72) & 0 + 0x1C then end
    local d = t - tf
    local y1 = mem:read_u16(P1 + 0x20)
    if fase == "dash" then
      local c = d % 9
      if c < 3 or (c >= 5 and c < 8) then J["P1 " .. frente]:set_value(1) end
      if y1 > 0 then fase, tf = "ar", t end
      if d > 60 then fase, tf = "dash", t end
    elseif fase == "ar" then
      if NOAR >= 0 and d >= NOAR and d < NOAR + 3 then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
      if NOAR >= 0 and d > 3 and y1 == 0 and d >= NOAR + 3 then fase, tf = "dash", t end
      if d > 3 and y1 == 0 and mem:read_u16(P1 + 0x72) == 0 then fase, tf = "pousou", t end
    elseif fase == "pousou" then
      if d >= POUSO and d < POUSO + 7 then J["P1 Down"]:set_value(1) end
      if d >= POUSO + 2 and d < POUSO + 5 then J["P1 C"]:set_value(1) end
      local a1 = mem:read_u16(P1 + 0x72)
      if d >= POUSO + 7 and (a1 == 0x62 or a1 == 0x63 or a1 == 0x64) and d >= POUSO + 7 + ESPERA then fase, tf = "dash", t end
      if d > 60 then fase, tf = "dash", t end
    end
  end
  log[#log + 1] = string.format("t=%d fase=%s vidaP2=%d x1=%x x2=%x y1=%x p1anim=%04x p2anim=%04x",
    t, fase, vida, x1, x2, mem:read_u16(P1 + 0x20), mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72))
  if t == 260 or t == 320 then m.video:snapshot() end
  if t >= 800 or vida == 0 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d acertos=%d maior_combo=%d\n", (vida0 or vida) - vida, acertos, maxcombo),
      table.concat(log, "\n"), "\n")
    f:close(); m:exit()
  end
end)
