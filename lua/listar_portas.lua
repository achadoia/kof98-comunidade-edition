local f = io.open("D:/Projects/kof98-recomp/build/portas_aes.txt", "w")
for tag, port in pairs(manager.machine.ioport.ports) do
  for nome, _ in pairs(port.fields) do f:write(tag .. " | " .. nome .. "\n") end
end
f:close(); manager.machine:exit()
