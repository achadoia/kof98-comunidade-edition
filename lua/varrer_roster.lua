-- Pra cada vaga 0..37: carrega "ae_pre" (logo após a seleção), põe a vaga no 1º lutador do P1
-- (variante KOF_VAR), roda até a luta e fotografa o HUD.
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local VAR = tonumber(os.getenv("KOF_VAR") or "1")
local id, fase, t0 = -1, "carregar", 0
local ATE = 2000
emu.register_periodic(function()
  local f = math.floor(m.time:as_double() * 59.1856)
  if fase == "carregar" then
    id = id + 1
    if id > 37 then m:exit() return end
    m:load("ae_pre"); fase = "rodar"; t0 = nil
  elseif fase == "rodar" then
    t0 = t0 or f
    if f - t0 < 140 then mem:write_u8(0x10a84e, id); mem:write_u8(0x10a851, VAR) end
    if f - t0 >= ATE then m.video:snapshot(); fase = "carregar" end
  end
end)
