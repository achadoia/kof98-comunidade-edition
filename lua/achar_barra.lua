-- Kyo anda até a Yuri e bate; despeja a RAM de trabalho em 3 momentos pra achar a barra de power.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local RAIZ = "D:/Projects/kof98-recomp/build/"

local function dump(nome)
  local f = io.open(RAIZ .. "ram_" .. nome .. ".bin", "wb")
  for a = 0x100000, 0x10FFFF, 2 do f:write(string.pack(">I2", mem:read_u16(a))) end
  f:close()
end

local inicio, feitos = nil, {}
emu.register_periodic(function()
  local t = math.floor(m.time:as_double() * 59.1856)
  inicio = inicio or t
  t = t - inicio
  for _, n in ipairs({"Up", "Down", "Left", "Right", "A", "B", "C", "D"}) do J["P1 " .. n]:set_value(0) end
  if t >= 10 and t < 50 then J["P1 Right"]:set_value(1) end          -- anda (sem bater)
  if t >= 60 and t < 200 and t % 14 < 3 then J["P1 C"]:set_value(1) end  -- soco forte repetido
  if t >= 60 and t < 200 then J["P1 Right"]:set_value(t % 14 >= 3 and 1 or 0) end
  for _, p in ipairs({{5, "a"}, {55, "b"}, {130, "c"}, {205, "d"}}) do
    if t >= p[1] and not feitos[p[2]] then feitos[p[2]] = true; dump(p[2]) end
  end
  if t == 206 then m.video:snapshot() end
  if t >= 210 then m:exit() end
end)
