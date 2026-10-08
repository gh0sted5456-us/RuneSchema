-- Loaded from BPModLoaderMod only when UE4SS mods.txt enables RuneSchema.
-- The native RuneSchema DLL writes this list from enabled runeschema.txt rows
-- before UE4SS starts its Lua mods. The BP loader remains the actor owner.
return function(mods, ordered, defaults, helpers, runeRoot)
    local list = io.open(runeRoot .. "/settings/logicmods.generated.txt", "r")
    if not list then
        print("[RuneSchema][LOGIC-PAK][PARTIAL] Enabled LogicMods list is unavailable.\n")
        return
    end

    local submitted, existing = 0, 0
    for name in list:lines() do
        if name:match("^[A-Za-z0-9_]+$") then
            if mods[name] then
                existing = existing + 1
            else
                local info = {
                    Name = name,
                    Priority = #ordered + 1,
                    AssetName = defaults.AssetName,
                    AssetNameAsFName = defaults.AssetNameAsFName,
                    AssetPath = "/Game/Mods/" .. name .. "/ModActor",
                    RuneSchemaOwner = true,
                }
                mods[name] = info
                ordered[#ordered + 1] = info
                submitted = submitted + 1
            end
        end
    end
    list:close()
    print(string.format("[RuneSchema][LOGIC-PAK][SUMMARY] submitted=%d already_owned=%d.\n", submitted, existing))
end
