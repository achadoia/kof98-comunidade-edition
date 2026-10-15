-- Entra numa luta sozinho e registra quais PCs leem o bloco de input do P1
-- ($10DA40-$10DA7F) durante a luta. Saída: build/leitores_input.txt + snapshots.
local m = manager.machine
local cpu = m.devices[":maincpu"]
local mem = cpu.spaces["program"]
local ports = m.ioport.ports

local function campo(tag, nome) return ports[tag].fields[nome] end
local COIN = campo(":AUDIO_COIN", "Coin 1")
local START = campo(":edge:joy:START", "1 Player Start")
local J = function(n) return campo(":edge:joy:JOY1", "P1 " .. n) end

local OUT = "D:/Projects/kof98-recomp/build/"
local frame = 0
local ja = {}
local apertados = {}
local leitores = {}
local tap

local function aperta(f, frames) f:set_value(1); apertados[#apertados + 1] = {f, frame + frames} end

-- golpe de teste: ↓ ↘ → + A (hadouken), com P1 virado pra direita
local seq_hadouken = { {"Down"}, {"Down", "Right"}, {"Right"}, {"Right", "A"} }

emu.register_periodic(function()
  frame = math.floor(m.time:as_double() * 59.1856)
  if frame // 250 ~= (ja.ult or -1) then ja.ult = frame // 250; print("frame", frame, os.clock()) end
  for i = #apertados, 1, -1 do
    if frame >= apertados[i][2] then apertados[i][1]:set_value(0); table.remove(apertados, i) end
  end
  if frame >= 600 and not ja.coin then ja.coin=true; aperta(COIN, 6) end
  if frame >= 700 and not ja.start then ja.start=true; aperta(START, 6) end
  if frame > 800 and frame < 3200 and frame % 40 < 2 and not apertados[1] then aperta(J("A"), 5) end
  for _, s in ipairs({3000, 3600, 4300}) do
    if frame >= s and not ja["s" .. s] then ja["s" .. s] = true; m.video:snapshot() end
  end
  if frame >= 3600 and not tap then
    tap = mem:install_read_tap(0x10da40, 0x10da7f, "leitores", function(off, data, mask)
      local pc = cpu.state["PC"].value
      local k = string.format("%06x <- %06x", pc, off)
      leitores[k] = (leitores[k] or 0) + 1
    end)
  end
  if frame >= 3600 and frame < 4400 then
    local passo = (frame - 3600) % 60
    if passo < #seq_hadouken * 4 and not apertados[1] then
      local nomes = seq_hadouken[passo // 4 + 1]
      for _, n in ipairs(nomes) do aperta(J(n), 4) end
    end
  end
  if frame >= 4400 then
    local f = io.open(OUT .. "leitores_input.txt", "w")
    local ks = {}
    for k in pairs(leitores) do ks[#ks + 1] = k end
    table.sort(ks)
    for _, k in ipairs(ks) do f:write(k, "  x", leitores[k], "\n") end
    f:close()
    m:exit()
  end
end)
