-- Fotografa as telas entre coin/start e a seleção, sem apertar A (pra ver a tela de modo).
local m = manager.machine
local P = m.ioport.ports
local ja = {}
local solta = {}
local function aperta(f, ate) f:set_value(1); solta[#solta + 1] = {f, ate} end
emu.register_periodic(function()
  local fr = math.floor(m.time:as_double() * 59.1856)
  for i = #solta, 1, -1 do if fr >= solta[i][2] then solta[i][1]:set_value(0); table.remove(solta, i) end end
  if fr >= 600 and not ja.c then ja.c = true; aperta(P[":AUDIO_COIN"].fields["Coin 1"], fr + 6) end
  if fr >= 700 and not ja.s then ja.s = true; aperta(P[":edge:joy:START"].fields["1 Player Start"], fr + 6) end
  if fr >= 720 and fr % 30 == 0 and not ja[fr] then ja[fr] = true; m.video:snapshot() end
  if fr >= 1000 then m:exit() end
end)
