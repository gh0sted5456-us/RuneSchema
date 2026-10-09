-- RuneSchema-owned UE4SS Lua companion bootstrap.
--
-- Enabled RuneSchema mods may provide:
--   RuneSchema/mods/<ModName>/ue4ss/scripts/main.lua
--
-- Companion scripts execute with the normal UE4SS Lua globals. They are
-- intentionally hosted by RuneSchema, so disabling RuneSchema in UE4SS's
-- mods.txt also disables every nested companion.

local function log(message)
    print(string.format("[RuneSchema][UE4SS] %s\n", message))
end

local function trim(value)
    return (value:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function fold_ascii(value)
    return (value:gsub("%u", string.lower))
end

local function valid_mod_name(name)
    return name ~= "" and name ~= "." and name ~= ".."
        and not name:find("[/\\:<>\"|%?%*]")
        and not name:find("[%z\1-\31]")
end

local source = debug.getinfo(1, "S").source or ""
if source:sub(1, 1) == "@" then source = source:sub(2) end
source = source:gsub("\\", "/")

local root = source:match("^(.*)/[Ss]cripts/main%.lua$")
if not root then
    log("[FATAL] Bootstrap could not resolve the RuneSchema directory; Lua companions were not started.")
    return
end

local logicOk, logicLoader = pcall(dofile, root .. "/scripts/logicmods-loader.lua")
if logicOk and type(logicLoader) == "function" then
    local started, failure = xpcall(function() logicLoader(root) end, debug.traceback)
    if not started then log("[LOGIC-PAK][RUNE-LUA][PARTIAL] " .. tostring(failure)) end
elseif not logicOk then
    log("[LOGIC-PAK][RUNE-LUA][PARTIAL] Helper unavailable: " .. tostring(logicLoader))
end

local mods_root = root .. "/mods"
local order_path = mods_root .. "/runeschema.txt"
local order = io.open(order_path, "r")
if not order then
    log("No mods/runeschema.txt was available; Lua companions were not started this launch.")
    return
end

local ordered_keys = {}
local entries = {}
for line in order:lines() do
    line = line:gsub("^\239\187\191", "")
    local text = trim(line)
    if text ~= "" and text:sub(1, 1) ~= ";" and text:sub(1, 1) ~= "#" then
        local colon = text:find(":", 1, true)
        if colon then
            local name = trim(text:sub(1, colon - 1))
            local value = trim(text:sub(colon + 1))
            if valid_mod_name(name) then
                local key = fold_ascii(name)
                if not entries[key] then
                    entries[key] = { name = name, enabled = value == "1" }
                    ordered_keys[#ordered_keys + 1] = key
                else
                    -- Match RuneSchema's native load-order rule: a disabled
                    -- duplicate wins regardless of row order or casing.
                    entries[key].enabled = entries[key].enabled and value == "1"
                end
            else
                log(string.format("[WARNING] Ignored unsafe mod name in runeschema.txt: '%s'.", name))
            end
        end
    end
end
order:close()

RuneSchemaUE4SS = RuneSchemaUE4SS or {}
RuneSchemaUE4SS.Loaded = RuneSchemaUE4SS.Loaded or {}
RuneSchemaUE4SS.Root = root

local loaded = 0
local failed = 0
for _, key in ipairs(ordered_keys) do
    local entry = entries[key]
    if entry.enabled then
        local companion_root = mods_root .. "/" .. entry.name .. "/ue4ss"
        local scripts_root = companion_root .. "/scripts"
        local main_path = scripts_root .. "/main.lua"
        local probe = io.open(main_path, "rb")
        if probe then
            probe:close()

            -- UE4SS Lua helpers remain available. Add the companion's own
            -- Lua module paths so main.lua may require additional Lua files.
            package.path = package.path
                .. ";" .. scripts_root .. "/?.lua"
                .. ";" .. scripts_root .. "/?/init.lua"

            RuneSchemaUE4SS.Current = {
                Name = entry.name,
                Root = companion_root,
                Scripts = scripts_root,
            }

            local ok, failure = xpcall(function()
                dofile(main_path)
            end, debug.traceback)
            RuneSchemaUE4SS.Current = nil

            if ok then
                RuneSchemaUE4SS.Loaded[key] = entry.name
                loaded = loaded + 1
                log(string.format("[MOD:%s][OK] Lua companion started.", entry.name))
            else
                failed = failed + 1
                log(string.format("[MOD:%s][ERROR] Lua companion failed: %s", entry.name, tostring(failure)))
            end
        end
    end
end

log(string.format("Companion summary: %d started, %d failed.", loaded, failed))
