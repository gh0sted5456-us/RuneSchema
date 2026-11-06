-- Mod-owned state and callbacks; RuneSchema supplies presentation and input.
if type(RuneSchemaHUD)=="table" then return RuneSchemaHUD end
local api,handlers,published={},{},{}
local timer,generation=nil,0
local function checked(issue) assert(issue=="",tostring(issue)) end
local function identity(owner,id) assert(type(owner)=="string" and type(id)=="string" and not owner:find("\0",1,true) and not id:find("\0",1,true),"Invalid HUD identity");return owner.."/"..id end
local function stop()
    generation=generation+1
    if timer~=nil then pcall(CancelDelayedAction,timer);timer=nil end
end
function api.Tick()
    for _=1,8 do
        local owner,id,control,kind,text,full=RuneSchemaHUDNext()
        if owner=="" then return end
        local row=handlers[full]
        if row and row[control] then
            local value=kind=="slider" and tonumber(text) or kind=="toggle" and text=="true" or text
            if kind=="toggle" then value=text=="true" elseif kind=="button" then value=nil end
            local key=full.."/"..control;local serial=published[key]
            local ok,result=pcall(row[control],value)
            if ok and result~=false and kind~="button" and published[key]==serial then checked(RuneSchemaHUDSet(owner,id,control,tostring(value))) end
            if not ok or result==false then RuneSchemaHUDError(owner,id,not ok and tostring(result) or "The mod rejected this change.") end
        end
    end
end
local function schedule()
    if timer~=nil then return end
    assert(type(LoopInGameThreadWithDelay)=="function" and type(CancelDelayedAction)=="function","UE4SS game-thread scheduling is unavailable")
    generation=generation+1;local current=generation
    local ok,handle=pcall(LoopInGameThreadWithDelay,100,function()
        if generation~=current then return end
        local ran,issue=pcall(api.Tick)
        if not ran then for full in pairs(handlers) do local owner,id=full:match("^(.*)/([^/]+)$");RuneSchemaHUDError(owner,id,tostring(issue)) end end
    end)
    assert(ok,"HUD scheduling failed: "..tostring(handle));timer=handle
end
function api.Register(owner,id,callbacks)
    local full=identity(owner,id);assert(type(callbacks)=="table","HUD callbacks must be a table")
    local names={} for name,callback in pairs(callbacks) do assert(type(name)=="string" and name:match("^[%w_.%-]+$") and type(callback)=="function","Invalid HUD callback");names[#names+1]=name end
    table.sort(names);checked(RuneSchemaHUDRegister(owner,id,table.concat(names,"\n")))
    handlers[full]=callbacks
    local ok,issue=pcall(schedule);if not ok then handlers[full]=nil;RuneSchemaHUDUnregister(owner,id);error(issue,0) end
    return api
end
function api.Set(owner,id,control,value) local key=identity(owner,id).."/"..control;checked(RuneSchemaHUDSet(owner,id,control,tostring(value)));published[key]=(published[key] or 0)+1;return api end
function api.Unregister(owner,id)
    local full=identity(owner,id);checked(RuneSchemaHUDUnregister(owner,id));handlers[full]=nil
    if next(handlers)==nil then stop() end
end
RuneSchemaHUD=api
return api
