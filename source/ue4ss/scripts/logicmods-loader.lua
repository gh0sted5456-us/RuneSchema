-- Experimental RuneSchema-only ModActor loader. Enabled by
-- settings.jsonc bpModLoader.luaActorLoader; BPModLoaderMod is not modified.
return function(runeRoot)
    local list = io.open(runeRoot .. "/settings/logicmods.lua.generated.txt", "r")
    if not list then
        print("[RuneSchema][LOGIC-PAK][RUNE-LUA][PARTIAL] Generated list unavailable.\n")
        return
    end

    local ordered, enabled = {}, {}
    for name in list:lines() do
        if name:match("^[A-Za-z0-9_]+$") and not enabled[name] then
            enabled[name] = true
            ordered[#ordered + 1] = name
        end
    end
    list:close()
    if #ordered == 0 then return end

    local function valid(object)
        return object ~= nil and object:IsValid()
    end
    local function log(message)
        print("[RuneSchema][LOGIC-PAK][RUNE-LUA] " .. message .. "\n")
    end
    local helpers = StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers")
    if not valid(helpers) then
        log("[PARTIAL] AssetRegistryHelpers unavailable; no ModActors started.")
        return
    end

    local started = {}
    local function startWorld(world)
        if not valid(world) then return end
        for _, name in ipairs(ordered) do
            if not started[name] then
                local ok, failure = xpcall(function()
                    local asset = {
                        PackageName = UEHelpers.FindOrAddFName("/Game/Mods/" .. name .. "/ModActor"),
                        AssetName = UEHelpers.FindOrAddFName("ModActor_C"),
                    }
                    local actorClass = helpers:GetAsset(asset)
                    if not valid(actorClass) then
                        error("ModActor class did not resolve")
                    end
                    local actor = world:SpawnActor(actorClass, {}, {})
                    if not valid(actor) then error("SpawnActor returned no actor") end
                    started[name] = true
                    local pre = actor.PreBeginPlay
                    if valid(pre) then pre() end
                    log("[STARTED][" .. name .. "] ModActor started for this world.")
                end, debug.traceback)
                if not ok then log("[FAILED][" .. name .. "] " .. tostring(failure)) end
            end
        end
    end

    RegisterBeginPlayPostHook(function(contextParameter)
        local ok, failure = xpcall(function()
            local actor = contextParameter:get()
            if not valid(actor) then return end
            local actorClass = actor:GetClass()
            if not valid(actorClass) then return end
            local className = actorClass:GetFullName()
            local package = className:match("^BlueprintGeneratedClass /Game/Mods/([A-Za-z0-9_]+)/ModActor%.ModActor_C$")
            if not package or not enabled[package] then return end
            local post = actor.PostBeginPlay
            if valid(post) then post() end
        end, debug.traceback)
        if not ok then log("[POST-BEGINPLAY][FAILED] " .. tostring(failure)) end
    end)

    RegisterLoadMapPostHook(function(_, worldParameter)
        started = {}
        local ok, failure = xpcall(function() startWorld(worldParameter:get()) end, debug.traceback)
        if not ok then log("[MAP][FAILED] " .. tostring(failure)) end
    end)

    ExecuteInGameThread(function()
        local ok, failure = xpcall(function() startWorld(UEHelpers.GetWorld()) end, debug.traceback)
        if not ok then log("[INITIAL][FAILED] " .. tostring(failure)) end
    end)
    log("[READY] " .. #ordered .. " RuneSchema ModActor package(s) queued; BPModLoaderMod untouched.")
end
