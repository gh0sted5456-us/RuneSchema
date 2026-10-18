-- Native UE4SS capability handoff. Never implements a replacement dumper.
return function()
    local request = os.getenv("RUNESCHEMA_AUTO_MAPPING_REQUEST")
    if not request then return end
    local file = io.open(request, "rb")
    if not file then return end
    local nonce = file:read(128); file:close()
    if not nonce or not nonce:match("^[0-9a-f]+\n$") or #nonce ~= 17 then return end
    local function finish(status)
        local output = io.open(request .. ".result", "wb")
        if output then output:write(nonce, status, "\n"); output:close() end
    end
    if type(DumpUSMAP) ~= "function" or type(ExecuteInGameThread) ~= "function" or type(ExecuteWithDelay) ~= "function" then
        finish("unavailable"); return
    end
    ExecuteWithDelay(15000, function()
        ExecuteInGameThread(function()
            local success = pcall(DumpUSMAP, true)
            finish(success and "ok" or "error")
        end)
    end)
end
