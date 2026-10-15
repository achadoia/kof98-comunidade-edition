-- aplica o mesmo roteiro do --seg-entradas do port (formato ini-fim:alvo=bits) desde o boot e fotografa
if _G.ROTEIRO_AES_ATIVO then return end
_G.ROTEIRO_AES_ATIVO = true
local m = manager.machine
local P = m.ioport.ports
local fotos = {}
for q in (os.getenv("KOF_FOTOS") or ""):gmatch("%d+") do fotos[tonumber(q)] = true end
local ult = 0
for q in pairs(fotos) do if q > ult then ult = q end end
local passos = {}
for ini, fim, alvo, bits in (os.getenv("KOF_ROTEIRO") or ""):gmatch("(%d+)%-(%d+):(%a%w)=(%x+)") do
  passos[#passos + 1] = {tonumber(ini), tonumber(fim), alvo, tonumber(bits, 16)}
end
local NOMES = {"Up", "Down", "Left", "Right", "A", "B", "C", "D"}
local t = 0
emu.register_frame_done(function()
  t = t + 1
  local p1, co, st = 0, 0, 0
  for _, p in ipairs(passos) do
    if t >= p[1] and t <= p[2] then
      if p[3] == "p1" then p1 = p1 | p[4] elseif p[3] == "co" then co = co | p[4] elseif p[3] == "st" then st = st | p[4] end
    end
  end
  for i, n in ipairs(NOMES) do P[":ctrl1:joy:JOY"].fields["P1 " .. n]:set_value((p1 >> (i - 1)) & 1) end

  P[":ctrl1:joy:START_SELECT"].fields["P1 Start"]:set_value(st & 1)
  if fotos[t] then m.video:snapshot() end
  if t > ult then m:exit() end
end)
