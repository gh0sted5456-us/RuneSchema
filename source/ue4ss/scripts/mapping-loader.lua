-- Native UE4SS capability handoff. Never implements a replacement dumper.
return function()
    if type(RuneSchemaTakeMappingRequest) ~= "function" then return end
    local request, nonce = RuneSchemaTakeMappingRequest()
    if not request then return end
    nonce = nonce .. "\n"
    local function request_is_current()
        local file = io.open(request, "rb")
        if not file then return false end
        local contents = file:read(128)
        file:close()
        return contents == nonce
    end
    if not request_is_current() then return end
    local function finish(status)
        local output = io.open(request .. ".result", "wb")
        if output then output:write(nonce, status, "\n"); output:close() end
    end
    if type(DumpUSMAP) ~= "function" or type(ExecuteInGameThreadWithDelay) ~= "function" then
        finish("unavailable"); return
    end
    local scheduled = pcall(ExecuteInGameThreadWithDelay, 15000, function()
        if not request_is_current() then return end
        local success = pcall(DumpUSMAP, true)
        finish(success and "ok" or "error")
    end)
    if not scheduled then finish("unavailable") end
end
