-- Hipótese do infinito do Kyo no EXTRA: ↓C (gancho) acerta o P2 no ar e o pulinho (→→) + ↓C
-- re-acerta antes de ele cair, sem fim. P2 pula em t=12; Kyo solta ↓C em t=T0, depois repete
-- (→→ , ↓C GAP quadros depois do 2º toque) a cada CICLO quadros enquanto o P2 estiver no ar.
-- Env: KOF_T0, KOF_CICLO, KOF_GAP, KOF_PATCHES, KOF_TAG.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local J2 = m.ioport.ports[":edge:joy:JOY2"].fields
local TAG = os.getenv("KOF_TAG") or "juggle"
local T0 = tonumber(os.getenv("KOF_T0") or "30")
local CICLO = tonumber(os.getenv("KOF_CICLO") or "20")
local GAP = tonumber(os.getenv("KOF_GAP") or "2")
local P1, P2 = 0x108100, 0x108300
local t, log, vida0, acertos, ultimo_hit = -1, {}, nil, 0, nil

local function zera(c, p) for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c[p .. n]:set_value(0) end end

emu.register_periodic(function()
  t = t + 1
  if t < 4 then
    mem:write_u8(P1 + 0x1D5, 1); mem:write_u8(P2 + 0x1D5, 1)
    mem:write_u16(P1 + 0x18, 0x1A0); mem:write_u16(P2 + 0x18, 0x1C8)
  end
  if t == 4 then vida0 = mem:read_u16(P2 + 0x138) end
  zera(J, "P1 "); zera(J2, "P2 ")
  if t >= 12 and t < 15 then J2["P2 Up"]:set_value(1) end
  local frente = mem:read_u16(P1 + 0x18) < mem:read_u16(P2 + 0x18) and "Right" or "Left"
  if t >= T0 and t < T0 + 3 then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
  if t >= T0 + 6 then
    local c = (t - T0 - 6) % CICLO
    if c < 2 or (c >= 3 and c < 5) then J["P1 " .. frente]:set_value(1) end
    if c >= 5 + GAP and c < 8 + GAP then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
  end
  local vida = mem:read_u16(P2 + 0x138)
  if vida0 and vida < (ultimo_hit or vida0) then acertos = acertos + 1 end
  if vida0 then ultimo_hit = vida end
  log[#log + 1] = string.format("t=%d vidaP2=%d y2=%x x1=%x x2=%x p1anim=%04x p2anim=%04x",
    t, vida, mem:read_u16(P2 + 0x20), mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18), mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72))
  if t >= 400 or (vida0 and vida == 0) then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d acertos=%d quadros=%d\n", vida0 - vida, acertos, t), table.concat(log, "\n"), "\n")
    f:close(); m:exit()
  end
end)
