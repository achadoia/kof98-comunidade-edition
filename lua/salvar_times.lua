local function lista(s) if not s then return nil end local t = {} for n in s:gmatch("%d+") do t[#t + 1] = tonumber(n) end return t end
local TIME1 = lista(os.getenv("KOF_TIME1"))
local TIME2 = lista(os.getenv("KOF_TIME2"))
local VAR1 = lista(os.getenv("KOF_VAR1"))
local VAR2 = lista(os.getenv("KOF_VAR2"))
local ESTADO = os.getenv("KOF_ESTADO") or "vs"
local ALVO = tonumber(os.getenv("KOF_ALVO") or "3350")
-- Entra numa luta, salva o estado "luta" (pra carregar com -state luta) e despeja
-- os ponteiros da lista de comandos dos dois jogadores.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local ports = m.ioport.ports
local COIN = ports[":AUDIO_COIN"].fields["Coin 1"]
local START = ports[":edge:joy:START"].fields["1 Player Start"]
local A = ports[":edge:joy:JOY1"].fields["P1 A"]
local A2 = ports[":edge:joy:JOY2"].fields["P2 A"]
local COIN2 = ports[":AUDIO_COIN"].fields["Coin 2"]
local START2 = ports[":edge:joy:START"].fields["2 Players Start"]

local ja, solta = {}, {}
local function aperta(f, ate) f:set_value(1); solta[#solta + 1] = {f, ate} end

emu.register_periodic(function()
  local frame = math.floor(m.time:as_double() * 59.1856)
  for i = #solta, 1, -1 do
    if frame >= solta[i][2] then solta[i][1]:set_value(0); table.remove(solta, i) end
  end
  if frame >= 600 and not ja.coin then ja.coin = true; aperta(COIN, frame + 6) end
  if frame >= 650 and not ja.coin2 then ja.coin2 = true; aperta(COIN2, frame + 6) end
  if frame >= 700 and not ja.start then ja.start = true; aperta(START, frame + 6) end
  if frame >= 760 and not ja.start2 then ja.start2 = true; aperta(START2, frame + 6) end
  if frame > 800 and frame < 3200 and frame % 40 < 2 and not solta[1] then aperta(A, frame + 5); aperta(A2, frame + 5) end
  if TIME1 and frame >= 1005 and frame < 1150 then
    for i, c in ipairs(TIME1) do mem:write_u8(0x10a84d + i, c) end
    for i, c in ipairs(TIME2) do mem:write_u8(0x10a85e + i, c) end
    for i, c in ipairs(VAR1 or {}) do mem:write_u8(0x10a850 + i, c) end
    for i, c in ipairs(VAR2 or {}) do mem:write_u8(0x10a861 + i, c) end
  end
  if frame >= ALVO and not ja.salvo then
    ja.salvo = true
    m:save(ESTADO)
    m.video:snapshot()
    local f = io.open("D:/Projects/kof98-recomp/build/ponteiros_" .. ESTADO .. ".txt", "w")
    for _, a in ipairs({0x10e760, 0x10e764, 0x10e768, 0x10e76c}) do
      f:write(string.format("%06x = %08x\n", a, mem:read_u32(a)))
    end
    for _, base in ipairs({0x108100, 0x108300}) do
      f:write(string.format("obj %06x: ", base))
      for i = 0, 0x1ff, 2 do f:write(string.format("%04x ", mem:read_u16(base + i))) end
      f:write("\n")
    end
    f:close()
  end
  if frame >= ALVO + 50 then m:exit() end
end)
