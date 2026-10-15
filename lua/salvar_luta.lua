-- Entra numa luta, salva o estado "luta" (pra carregar com -state luta) e despeja
-- os ponteiros da lista de comandos dos dois jogadores.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local ports = m.ioport.ports
local COIN = ports[":AUDIO_COIN"].fields["Coin 1"]
local START = ports[":edge:joy:START"].fields["1 Player Start"]
local A = ports[":edge:joy:JOY1"].fields["P1 A"]

local ja, solta = {}, {}
local function aperta(f, ate) f:set_value(1); solta[#solta + 1] = {f, ate} end

emu.register_periodic(function()
  local frame = math.floor(m.time:as_double() * 59.1856)
  for i = #solta, 1, -1 do
    if frame >= solta[i][2] then solta[i][1]:set_value(0); table.remove(solta, i) end
  end
  if frame >= 600 and not ja.coin then ja.coin = true; aperta(COIN, frame + 6) end
  if frame >= 700 and not ja.start then ja.start = true; aperta(START, frame + 6) end
  if frame > 800 and frame < 3200 and frame % 40 < 2 and not solta[1] then aperta(A, frame + 5) end
  if frame >= 3350 and not ja.salvo then
    ja.salvo = true
    m:save("luta")
    m.video:snapshot()
    local f = io.open("D:/Projects/kof98-recomp/build/ponteiros_comando.txt", "w")
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
  if frame >= 3400 then m:exit() end
end)
