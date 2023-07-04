#!/usr/bin/lua
--! Minimalistic ls for testing purpuses in openwrt 
if type(arg[2]) ~= "string" or arg[2]:len() < 1 then
	print (arg[0], "needs a second argument")
	os.exit(-22)
end

function scandir(directory)
    local i, t, popen = 0, {}, io.popen
    local pfile = popen('ls -a "'..directory..'"')
    for filename in pfile:lines() do
        i = i + 1
        t[i] = filename
        print(filename)
    end
    pfile:close()
    return t
end

if arg[1] == "ls" then 
    print(scandir(arg[2]))
else
	print(arg[0], "needs an operation name to be specified as first argument")
	os.exit(-22)
end
