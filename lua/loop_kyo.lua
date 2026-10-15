-- Infinito do Kyo no EXTRA: repete (frente, frente) + ↓C, com "frente" = na direção do P2.
-- Env: KOF_PATCHES, KOF_TAG, KOF_CICLO (quadros entre ciclos, padrão 26), KOF_GAP (quadros entre
-- o dash e o ↓C, padrão 6). Grava vida do P2 e posições por quadro.
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local J2 = m.ioport.ports[":edge:joy:JOY2"].fields
local DEFENDE = os.getenv("KOF_DEFENDE") == "1"
local TAG = os.getenv("KOF_TAG") or "loop_kyo"
local CICLO = tonumber(os.getenv("KOF_CICLO") or "26")
local GAP = tonumber(os.getenv("KOF_GAP") or "6")
local P1, P2 = 0x108100, 0x108300
local t, log, fotos = -1, {}, {}
local vida0

local function solta() for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end end

emu.register_periodic(function()
  t = t + 1
  if t < 4 then
    mem:write_u8(P1 + 0x1D5, 1); mem:write_u8(P2 + 0x1D5, 1)   -- modo EXTRA
    mem:write_u16(P1 + 0x18, 0x1A0); mem:write_u16(P2 + 0x18, 0x1D0)
  end
  if t == 4 then vida0 = mem:read_u16(P2 + 0x138) end
  solta()
  for _, n in ipairs({"Up", "Down", "Left", "Right"}) do J2["P2 " .. n]:set_value(0) end
  if DEFENDE and t >= 10 then
    local tras = mem:read_u16(P2 + 0x18) > mem:read_u16(P1 + 0x18) and "Right" or "Left"
    J2["P2 " .. tras]:set_value(1); J2["P2 Down"]:set_value(1)
  end
  if t >= 10 then
    local frente = mem:read_u16(P1 + 0x18) < mem:read_u16(P2 + 0x18) and "Right" or "Left"
    local c = (t - 10) % CICLO
    if c < 2 or (c >= 4 and c < 6) then J["P1 " .. frente]:set_value(1) end
    if c >= 6 + GAP and c < 9 + GAP then J["P1 Down"]:set_value(1); J["P1 C"]:set_value(1) end
  end
  log[#log + 1] = string.format("t=%d vidaP2=%d x1=%x x2=%x y1=%x p1anim=%04x p2anim=%04x",
    t, mem:read_u16(P2 + 0x138), mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18), mem:read_u16(P1 + 0x20),
    mem:read_u16(P1 + 0x72), mem:read_u16(P2 + 0x72))
  if t % 60 == 0 and t > 0 then m.video:snapshot() end
  if t >= 600 or (vida0 and mem:read_u16(P2 + 0x138) == 0) then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano=%d em %d quadros\n", vida0 - mem:read_u16(P2 + 0x138), t), table.concat(log, "\n"), "\n")
    f:close(); m:exit()
  end
end)
