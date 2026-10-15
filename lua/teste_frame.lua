local n,v=0,0
local mem = manager.machine.devices[":maincpu"].spaces["program"]
TAPV = mem:install_read_tap(0x64, 0x67, "vbl", function(o,d,m) v=v+1 end)
emu.register_periodic(function() n=n+1 end)
STOPN = emu.add_machine_stop_notifier(function() print("periodic", n, "vbl reads", v, manager.machine.time:as_double()) end)
