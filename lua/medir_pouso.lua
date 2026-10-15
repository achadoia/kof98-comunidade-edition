-- Mede quantos quadros o Kyo leva do 1º toque do pulinho (EXTRA) até poder agir de novo (anim 0).
dofile("D:/Projects/kof98-recomp/lua/aplicar_patches.lua")
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local J = m.ioport.ports[":edge:joy:JOY1"].fields
local P1 = 0x108100
local t, saiu, anims = -1, nil, {}
emu.register_periodic(function()
  t = t + 1
  if t < 4 then mem:write_u8(P1 + 0x1D5, 1) end
  for _, n in ipairs({"Left", "Right"}) do J["P1 " .. n]:set_value(0) end
  if t >= 10 and (t < 13 or (t >= 15 and t < 18)) then J["P1 Right"]:set_value(1) end
  local a = mem:read_u16(P1 + 0x72)
  if t >= 10 then anims[#anims + 1] = string.format("%x", a) end
  if t > 20 and a == 0 and not saiu then saiu = t - 10 end
  if t >= 70 then print("QUADROS_ATE_AGIR=" .. tostring(saiu) .. " anims=" .. table.concat(anims, ",")); m:exit() end
end)
