-- RuneSchema-owned nested Lua add-ons on an unmodified UE4SS host.
-- Native/hybrid packages and retired host-owned manifest rows must never run.
return function(root)
    local function log(message) print("[RuneSchema][UE4SS] " .. message .. "\n") end
    local expected = os.getenv("RUNESCHEMA_UE4SS_PACKAGE_SESSION")
    if not expected or expected == "" then return false end
    local file = io.open(root .. "/settings/ue4ss.generated.txt", "r")
    if not file then log("[BLOCKED] Current-session package manifest unavailable."); return true end
    local first = file:read("*l")
    if first ~= "# Session\t" .. expected then file:close(); log("[BLOCKED] Stale package manifest refused."); return true end
    local function relative(path)
        if path == "" or path:find("[:\\\t\r\n]") or path:sub(1,1) == "/" then return false end
        for part in path:gmatch("[^/]+") do if part == "." or part == ".." then return false end end
        return true
    end
    RuneSchemaUE4SS = RuneSchemaUE4SS or {}
    local registry = RuneSchemaUE4SS
    registry.Loaded, registry.Packages = registry.Loaded or {}, registry.Packages or {}
    registry.Root = root
    local entries = {}
    for line in file:lines() do
        if line:sub(1,1) ~= "#" then
            local owner,name,path,main,kind = line:match("^([^\t]+)\t([^\t]+)\t([^\t]+)\t([^\t]*)\t([^\t]+)$")
            if owner and relative(path) and path:sub(1,#owner+1) == owner .. "/" and (main == "" or relative(main)) then
                entries[#entries+1] = {Owner=owner,Name=name,Root=root .. "/mods/" .. path,Main=main,Kind=kind}
            elseif line ~= "" then log("[REJECTED] Invalid package manifest row.") end
        end
    end
    file:close()
    local started,failed,unsupported = 0,0,0
    for _,entry in ipairs(entries) do
        local key = (entry.Owner .. "/" .. entry.Name):lower()
        registry.Packages[key] = entry
        if entry.Kind ~= "lua" then
            unsupported=unsupported+1
            log("[MOD:" .. entry.Owner .. "][UNSUPPORTED] " .. entry.Name .. "; only Lua add-ons are supported, package skipped.")
        elseif entry.Kind == "lua" and entry.Main ~= "" and not registry.Loaded[key] then
            local main_path = entry.Root .. "/" .. entry.Main
            local scripts_root = main_path:match("^(.*)/[^/]+$")
            local context = {Owner=entry.Owner,Name=entry.Name,Root=entry.Root,Scripts=scripts_root}
            local environment = setmetatable({}, {__index=_G})
            environment._G = environment
            environment.RuneSchemaUE4SS = setmetatable({Current=context}, {__index=registry})
            local own_package = {}; for name,value in pairs(package) do own_package[name]=value end
            own_package.path = scripts_root .. "/?.lua;" .. scripts_root .. "/?/init.lua;" .. entry.Root .. "/?.lua;" .. package.path
            own_package.loaded = {}; environment.package=own_package
            local loading = {}
            environment.require = function(name)
                if own_package.loaded[name] ~= nil then return own_package.loaded[name] end
                if loading[name] then error("circular companion require: " .. tostring(name)) end
                if type(name) ~= "string" or not name:match("^[%w_][%w_.%-]*$") then error("invalid companion module name") end
                local path = package.searchpath(name,scripts_root .. "/?.lua;" .. scripts_root .. "/?/init.lua;" .. entry.Root .. "/?.lua")
                if not path then return require(name) end -- UE4SS shared helpers retain their real host implementation.
                local chunk,problem=loadfile(path,"t",environment); if not chunk then error(problem) end
                loading[name]=true
                local ok,result=pcall(chunk,name,path);loading[name]=nil;if not ok then error(result) end
                if result == nil then result=true end;own_package.loaded[name]=result;return result
            end
            local ok,problem=xpcall(function()
                local chunk,error_message=loadfile(main_path,"t",environment);if not chunk then error(error_message) end
                chunk()
            end,debug.traceback)
            if ok then registry.Loaded[key]=true;started=started+1;log("[MOD:" .. entry.Owner .. "][OK] Lua package '" .. entry.Name .. "' started from its original folder.")
            else failed=failed+1;log("[MOD:" .. entry.Owner .. "][ERROR] '" .. entry.Name .. "': " .. tostring(problem)) end
        end
    end
    log(string.format("Package summary: %d Lua started, %d failed, %d unsupported.",started,failed,unsupported))
    return true
end
