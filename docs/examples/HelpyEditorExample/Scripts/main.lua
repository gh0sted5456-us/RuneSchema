-- External UE4SS demonstration. This file never changes an actor, save or network state.
local api={Values={},Revision=nil,Active=false}
local spec={
    Enabled={default=true,kind="boolean"},
    Height={default=1,kind="number",min=0.5,max=1.5},
    Build={default=0.5,kind="number",min=0,max=1},
    Hip={default=0.5,kind="number",min=0,max=1},
    Style={default="Balanced",kind="choice",options={Balanced=true,Athletic=true,Soft=true,Angular=true}},
    SkinColor={default="#DDB49A",kind="color",length=7},
    AccentColor={default="#BFA35AFF",kind="color",length=9}
}
local function log(message)print("[HelpyEditorExample] "..message.."\n")end
local function valid(rule,value)
    if rule.kind=="boolean" then return type(value)=="boolean" end
    if rule.kind=="number" then return type(value)=="number" and value==value and value>=rule.min and value<=rule.max end
    if rule.kind=="choice" then return type(value)=="string" and rule.options[value]==true end
    return type(value)=="string" and #value==rule.length and value:match("^#%x+$")~=nil
end
for key,rule in pairs(spec)do api.Values[key]=rule.default end
local script=debug.getinfo(1,"S").source:gsub("^@","")
local directory=script:match("^(.*)[/\\]Scripts[/\\][^/\\]+$")
local file=directory and io.open(directory.."/config.txt","r")
if file then
    for line in file:lines()do
        local key,value=line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
        local rule=key and spec[key]
        if value then value=value:gsub("%s+[;#].*$",""):gsub("%s+$","")end
        if rule then
            if rule.kind=="number" then value=tonumber(value)
            elseif rule.kind=="boolean" then if value=="true" then value=true elseif value=="false" then value=false else value=nil end end
            if valid(rule,value)then api.Values[key]=value end
        end
    end
    file:close()
end
-- Replace this callback with your own mod's implementation; keep actor/save ownership in that mod.
function api.OnSettingsChanged(values,revision)
    log("Settings received (revision "..tostring(revision).."): "..values.Style..", height "..tostring(values.Height))
end
local handle
function api.Stop()
    api.Active=false
    if handle and type(CancelDelayedAction)=="function" then pcall(CancelDelayedAction,handle)end
    handle=nil
end
function api.SelectTarget(token)
    if type(token)~="string" or #token>128 or token:find("[%z\r\n]")then return false end
    if ModRef and type(ModRef.SetSharedVariable)=="function" then
        local ok=pcall(function()ModRef:SetSharedVariable("HelpyEditorExample.target",token)end)
        return ok
    end
    return false
end
local function tick()
    if not api.Active or not ModRef then return end
    local ok,problem=pcall(function()
        if not ModRef:GetSharedVariable("ModMenu.version")then return end
        local revision=ModRef:GetSharedVariable("ModMenu.HelpyEditorExample.rev")
        if type(revision)~="number" or revision<0 or revision%1~=0 or revision==api.Revision then return end
        local values={}
        for key,rule in pairs(spec)do
            local value=ModRef:GetSharedVariable("ModMenu.HelpyEditorExample."..key)
            if value==nil then value=api.Values[key]end
            if not valid(rule,value)then error("Invalid value for "..key)end
            values[key]=value
        end
        api.Revision=revision;api.Values=values;api.OnSettingsChanged(values,revision)
    end)
    if not ok then log(tostring(problem));api.Stop()end
end
if ModRef and type(LoopInGameThreadWithDelay)=="function" then
    api.SelectTarget("example-preview")
    api.Active=true
    local ok,result=pcall(LoopInGameThreadWithDelay,250,tick)
    if ok then handle=result else api.Active=false;log("Game-thread scheduling unavailable: "..tostring(result))end
else log("Loaded local defaults/config. Live menu callbacks need UE4SS delayed actions and a Mod Menu provider.")end
return api
