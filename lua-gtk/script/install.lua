#! /usr/bin/env lua
-- vim:sw=4:sts=4
-- Simple installation script for Lua-Gtk.
-- by Wolfgang Oertl

---
-- Install a file or directory into the first applicable target directory from
-- the Lua search path.
--
-- @param ar  String with ";" delimited search paths
-- @param pattern  What to replace in the search path
-- @param source  File or directory in the current directory to install
--
function do_install(ar, pattern, source)
    local dest, cmd

    for path in string.gmatch(ar, "[^;]+") do
	-- if too short, then it's ./?.so or something.
	if path:len() > 10 then
	    dest = path:gsub(pattern, source)
	    if dest ~= path then
		cmd = string.format("cp -a %s %s", source, dest)
		print(cmd)
		os.execute(cmd)
		break
	    end
	end
    end
end

do_install(package.cpath, "%?.so", "gtk.so")
do_install(package.path, "%?.lua", "gtk")
