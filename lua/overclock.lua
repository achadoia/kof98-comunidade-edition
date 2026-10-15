-- Overclock da CPU 68000 via slider do MAME (tira a lentidão de efeitos pesados).
-- KOF_OC = porcentagem (padrão 200 = 2x).
local fator = tonumber(os.getenv("KOF_OC") or "200")
local feito = false
emu.register_periodic(function()
  if feito then return end
  feito = true
  local ok, err = pcall(function()
    for _, s in pairs(manager.ui:get_slider_list()) do
      if s.name and s.name:find("Overclock") and s.name:find("maincpu") then
        s.value = fator * 10
        print("OVERCLOCK", s.name, s.value)
      end
    end
  end)
  if not ok then print("OVERCLOCK erro", err) end
end)
