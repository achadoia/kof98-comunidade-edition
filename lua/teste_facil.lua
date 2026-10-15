-- Carrega o estado "luta" (rodar com -state luta), opcionalmente aplica o patch
-- de golpe facilitado no comando 9 do Kyo (↓↘→+P vira ↓↘+P) e aperta só ↓ ↘ A.
-- Registra se o bit do comando 9 ($10DA4C bit 2) acendeu.
local PATCH = os.getenv("KOF_PATCH") == "1"
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local function campo(n) return J["P1 " .. n] end

if PATCH then
  -- comando em 0x0B2B3A: 04 10 0000 | 14 02 | 28 0A | 14 08 | 2C F6
  -- vira:                  03 10 0000 | 14 02 | 28 0A | 2C F6
  local novo = {0x0310, 0x0000, 0x1402, 0x280A, 0x2CF6}
  for i, w in ipairs(novo) do mem:write_direct_u16(0x0B2B3A + (i - 1) * 2, w) end
end

local inicio
local seq = { {"Down"}, {"Down"}, {"Down", "Right"}, {"Down", "Right"}, {"Down", "Right", "A"}, {"A"}, {} }
local log = {}
local fotos = {}

emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  inicio = inicio or f
  local t = f - inicio
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do campo(n):set_value(0) end
  local passo = seq[t // 2 + 1 - 10]
  if passo then for _, n in ipairs(passo) do campo(n):set_value(1) end end
  local flag = mem:read_u8(0x10DA4C)
  if flag ~= 0 then log[#log + 1] = string.format("t=%d $10DA4C=%02x", t, flag) end
  for _, s in ipairs({30, 45, 60}) do
    if t >= s and not fotos[s] then fotos[s] = true; m.video:snapshot() end
  end
  if t >= 70 then
    local out = io.open("D:/Projects/kof98-recomp/build/teste_facil_" .. (PATCH and "patch" or "orig") .. ".txt", "w")
    out:write(#log > 0 and table.concat(log, "\n") or "nenhum comando 9 detectado", "\n")
    out:close()
    m:exit()
  end
end)
