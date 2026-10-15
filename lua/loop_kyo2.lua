-- Infinito do Kyo no EXTRA, versão reativa: ↓C; ao acertar, espera K quadros, →→ (pulinho),
-- e solta ↓C no quadro em que o Kyo pousa. Mede se o P2 fica preso (nunca volta ao neutro).
-- Env: KOF_K, KOF_CANTO=1 (P2 encostado no canto), KOF_PATCHES, KOF_TAG.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local TAG = os.getenv("KOF_TAG") or "loop2"
local K = tonumber(os.getenv("KOF_K") or "0")
local CANTO = os.getenv("KOF_CANTO") == "1"
local P1, P2 = 0x108100, 0x108300
local t, log, vida0, vida_ant, acertos, livres = -1, {}, nil, nil, 0, 0
local estado, t_est = "golpe", 0
local livre_desde_1o = false

local function zera() for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end end

emu.register_periodic(function()
  t = t + 1
  if t < 4 then
    mem:write_u8(P1 + 0x1D5, 1); mem:write_u8(P2 + 0x1D5, 1)
    if CANTO then mem:write_u16(P1 + 0x18, 0x400); mem:write_u16(P2 + 0x18, 0x430)
    else mem:write_u16(P1 + 0x18, 0x1A0); mem:write_u16(P2 + 0x18, 0x1C0) end
  end
  if t == 4 then vida0 = mem:read_u16(P2 + 0x138); vida_ant = vida0 end
  zera()
  if t < 10 then return end
  local frente = mem:read_u16(P1 + 0x18) < mem:read_u16(P2 + 0x18) and "Right" or "Left"
  local vida = mem:read_u16(P2 + 0x138)
  local acertou = vida < vida_ant
  vida_ant = vida
  if acertou then acertos = acertos + 1 end
  if acertos >= 1 and mem:read_u16(P2 + 0x72) == 0 then livres = livres + 1 end
  local d = t - t_est
  if estado == "golpe" then
    if d < 3 then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
    if acertou then estado, t_est = "espera", t end
    if d > 60 then estado, t_est = "golpe", t end
  elseif estado == "espera" then
    if d >= K then estado, t_est = "dash", t end
  elseif estado == "dash" then
    if d < 2 or (d >= 3 and d < 5) then J["P1 " .. frente]:set_value(1) end
    local a = mem:read_u16(P1 + 0x72)
    if d > 6 and (a == 0 or a == 0x2c) then estado, t_est = "golpe", t end
    if d > 40 then estado, t_est = "golpe", t end
  end
  log[#log + 1] = string.format("t=%d est=%s vidaP2=%d x1=%x x2=%x p1anim=%04x p2anim=%04x",
    t, estado, vida, mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18), mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72))
  if t >= 500 or vida == 0 then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d acertos=%d quadros_livres_do_P2=%d\n", vida0 - vida, acertos, livres),
      table.concat(log, "\n"), "\n")
    f:close(); m:exit()
  end
end)
