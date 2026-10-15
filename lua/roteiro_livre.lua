-- Roteiro de inputs por quadro pros dois jogadores + log do estado.
-- KOF_P1 / KOF_P2 = "ini-fim:entrada,..." (entrada = direção numpad + botões, ex. "10-12:9", "14-16:5B", "30-200:1").
-- Direções são do ponto de vista do jogador (6 = frente): o P2 começa à direita, então o lado é espelhado pra ele.
-- KOF_X1/KOF_X2 posicionam no início; KOF_FIM = último quadro. Saída: build/roteiro/<KOF_TAG>.txt
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local P = m.ioport.ports
local TAG = os.getenv("KOF_TAG") or "livre"
local FIM = tonumber(os.getenv("KOF_FIM") or "200")
local VIDA = tonumber(os.getenv("KOF_VIDA") or "0x150")
local P1, P2 = 0x108100, 0x108300
local X1, X2 = tonumber(os.getenv("KOF_X1") or ""), tonumber(os.getenv("KOF_X2") or "")

local function ler(txt)
  local r = {}
  for ini, fim, ent in (txt or ""):gmatch("(%d+)%-(%d+):([^,]+)") do r[#r + 1] = {tonumber(ini), tonumber(fim), ent} end
  return r
end
local R = { ler(os.getenv("KOF_P1")), ler(os.getenv("KOF_P2")) }
local VERT = { ["1"] = "Down", ["2"] = "Down", ["3"] = "Down", ["7"] = "Up", ["8"] = "Up", ["9"] = "Up" }
local TRAS = { ["1"] = true, ["4"] = true, ["7"] = true }
local FRENTE = { ["3"] = true, ["6"] = true, ["9"] = true }

local function aperta(j, ent)
  local c = P[":edge:joy:JOY" .. j].fields
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do c["P" .. j .. " " .. n]:set_value(0) end
  if not ent then return end
  local d = ent:sub(1, 1)
  local frente, tras = "Right", "Left"
  if j == 2 then frente, tras = "Left", "Right" end
  if VERT[d] then c["P" .. j .. " " .. VERT[d]]:set_value(1) end
  if TRAS[d] then c["P" .. j .. " " .. tras]:set_value(1) end
  if FRENTE[d] then c["P" .. j .. " " .. frente]:set_value(1) end
  for b in ent:sub(2):gmatch("[ABCD]") do c["P" .. j .. " " .. b]:set_value(1) end
end

local t, log, v0 = -1, {}, {}
emu.register_periodic(function()
  t = t + 1
  if t < 3 and X1 then mem:write_u16(P1 + 0x18, X1); mem:write_u16(P2 + 0x18, X2) end
  if t == 3 then v0 = { mem:read_u16(P1 + VIDA), mem:read_u16(P2 + VIDA) } end
  for j = 1, 2 do
    local ent
    for _, r in ipairs(R[j]) do if t >= r[1] and t <= r[2] then ent = r[3] end end
    aperta(j, ent)
  end
  local function st(b)
    return string.format("anim=%04x y=%04x vida=%d e0-e7=%02x%02x%02x%02x%02x%02x%02x%02x 7c=%02x",
      mem:read_u16(b + 0x72), mem:read_u16(b + 0x20), mem:read_u16(b + VIDA), mem:read_u8(b + 0xE0), mem:read_u8(b + 0xE1),
      mem:read_u8(b + 0xE2), mem:read_u8(b + 0xE3), mem:read_u8(b + 0xE4), mem:read_u8(b + 0xE5), mem:read_u8(b + 0xE6),
      mem:read_u8(b + 0xE7), mem:read_u8(b + 0x7C))
  end
  log[#log + 1] = string.format("t=%d x1=%d x2=%d | P1 %s | P2 %s", t, mem:read_u16(P1 + 0x18), mem:read_u16(P2 + 0x18), st(P1), st(P2))
  if t >= FIM then
    local f = io.open("D:/Projects/kof98-recomp/build/roteiro/" .. TAG .. ".txt", "w")
    f:write(string.format("RESULTADO dano_P1=%d dano_P2=%d\n", v0[1] - mem:read_u16(P1 + VIDA), v0[2] - mem:read_u16(P2 + VIDA)),
      table.concat(log, "\n"), "\n")
    f:close()
    m:exit()
  end
end)
