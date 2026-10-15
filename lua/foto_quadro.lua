-- tira foto no quadro KOF_QUADRO (contando do boot) e sai
local m = manager.machine
local alvo = tonumber(os.getenv("KOF_QUADRO") or "600")
local t = 0
emu.register_frame_done(function() t = t + 1; if t == alvo then m.video:snapshot() elseif t == alvo + 2 then m:exit() end end)
