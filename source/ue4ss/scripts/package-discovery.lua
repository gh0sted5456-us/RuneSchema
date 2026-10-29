-- Present active Lua packages through the host's directory API without moving files.
return function(root)
    if RuneSchemaUE4SSDiscovery then return true end
    if type(IterateGameDirectories) ~= "function" then return false end
    local expected = os.getenv("RUNESCHEMA_UE4SS_PACKAGE_SESSION")
    if not expected or expected == "" then return false end
    local file = io.open(root .. "/settings/ue4ss.generated.txt", "r")
    if not file then return false end
    if file:read("*l") ~= "# Session\t" .. expected then file:close(); return false end
    local text = file:read(1024 * 1024 + 1) or ""
    file:close()
    if #text > 1024 * 1024 then return false end
    local function relative(path)
        if path == "" or path:find("[:\\\t\r\n]") or path:sub(1,1) == "/" then return false end
        for part in path:gmatch("[^/]+") do if part == "." or part == ".." then return false end end
        return true
    end
    local entries = {}
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        if line ~= "" and line:sub(1,1) ~= "#" then
            local owner,name,path,main,kind = line:match("^([^\t]+)\t([^\t]+)\t([^\t]+)\t([^\t]*)\t([^\t]+)$")
            if not owner or not relative(path) or path:sub(1,#owner+1) ~= owner .. "/"
                or not relative(name) or name:find("/",1,true) then return false end
            if kind == "lua" and main ~= "" and relative(main) then
                local parts = {}
                for part in path:gmatch("[^/]+") do parts[#parts+1] = part end
                entries[#entries+1] = {Name=name,Parts=parts}
                if #entries > 4096 then return false end
            end
        end
    end
    if #entries == 0 then return false end
    local normalized = root:gsub("\\", "/")
    local platform,host = normalized:match("/[Bb]inaries/([^/]+)/([^/]+)/[Mm]ods/[^/]+$")
    if not platform then platform = normalized:match("/[Bb]inaries/([^/]+)/[Mm]ods/[^/]+$") end
    if not platform then return false end
    local function child(node,name)
        if type(node) ~= "table" then return nil end
        if node[name] ~= nil then return node[name] end
        local found
        for key,value in pairs(node) do
            if type(key) == "string" and key:lower() == name:lower() then
                if found then return nil end
                found = value
            end
        end
        return found
    end
    local function expose(dirs)
        local binaries = child(child(dirs,"Game"),"Binaries")
        local current = child(binaries,platform)
        local mods = child(host and child(current,host) or current,"Mods")
        local owners = child(child(mods,"RuneSchema"),"mods")
        if type(mods) ~= "table" or type(owners) ~= "table" then return end
        local claimed = {}
        for name in pairs(mods) do if type(name) == "string" then claimed[name:lower()] = true end end
        for _,entry in ipairs(entries) do
            if not claimed[entry.Name:lower()] and entry.Name:sub(1,2) ~= "__" then
                local node = owners
                for _,part in ipairs(entry.Parts) do node = child(node,part); if not node then break end end
                if type(node) == "table" then
                    mods[entry.Name] = node -- Preserve the complete native node and its original absolute paths.
                    claimed[entry.Name:lower()] = true
                end
            end
        end
    end
    local original, warned = IterateGameDirectories, false
    IterateGameDirectories = function(...)
        local results = table.pack(original(...))
        local ok,problem = pcall(expose,results[1])
        if not ok and not warned then
            warned = true
            print("[RuneSchema][UE4SS][DISCOVERY][DEFERRED] " .. tostring(problem) .. "\n")
        end
        return table.unpack(results,1,results.n)
    end
    RuneSchemaUE4SSDiscovery = {Packages=#entries}
    return true
end
