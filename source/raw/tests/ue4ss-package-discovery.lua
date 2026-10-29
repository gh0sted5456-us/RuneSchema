local source = assert(arg[1])
local fixture = "C:/Game/Binaries/WinGDK/ue4ss/Mods/RuneSchema"
local manifest = "# Session\tcurrent\nBootylicious\tMorphSliders\tBootylicious/ue4ss/MorphSliders\tScripts/main.lua\tlua\n"
    .. "Legacy\tLegacy\tLegacy/ue4ss\tscripts/main.lua\tlua\nN\tNative\tN/ue4ss/Native\tScripts/main.lua\tnative\n"
    .. "H\tRetired\tH/ue4ss/Retired\tScripts/main.lua\thost\n"
local function node(path,files)
    return {__absolute_path=path,__files=files or {}}
end
local function setup(text,platform)
    local active = node(fixture .. "/mods/Bootylicious/ue4ss/MorphSliders",{{__name="modmenu.json"},{__name="config.txt"}})
    active.Scripts = {__files={{__name="main.lua"},{__name="helper.lua"}}}
    active.Assets = {marker="complete package"}
    local stock = {marker="ordinary mod"}
    local mods = {RuneSchema={mods={Bootylicious={ue4ss={MorphSliders=active}},Legacy={ue4ss={marker="legacy"}},
        N={ue4ss={Native={}}},H={ue4ss={Retired={}}},Disabled={ue4ss={Disabled={}}}}},ModMenu=stock}
    local other = {marker="other storefront"}
    local dirs = {Game={Binaries={[platform or "WinGDK"]={ue4ss={Mods=mods}},Win64={ue4ss={Mods=other}}}}}
    local calls = 0
    local original = function(...) calls=calls+1; assert(select('#',...)==0); return dirs,"extra",nil end
    local environment = setmetatable({IterateGameDirectories=original,os={getenv=function() return "current" end},
        io={open=function(path)
            assert(path==fixture .. "/settings/ue4ss.generated.txt")
            return {read=function(_,mode) if mode=="*l" then return text:match("([^\n]*)\n") end; return text:match("\n(.*)") end,close=function() end}
        end}}, {__index=_G})
    environment._G=environment
    local install=assert(loadfile(source,"t",environment))()
    return environment,install,mods,active,stock,other,original,function() return calls end
end
do
    local e,install,mods,active,stock,other,original,calls=setup(manifest)
    assert(install(fixture)); assert(calls()==0) -- No directory scan until a consumer requests one.
    local wrapped=e.IterateGameDirectories;assert(install(fixture));assert(e.IterateGameDirectories==wrapped)
    local values=table.pack(wrapped());assert(values.n==3 and values[2]=="extra" and values[3]==nil)
    assert(calls()==1 and mods.MorphSliders==active and mods.ModMenu==stock)
    assert(mods.MorphSliders.Assets.marker=="complete package" and mods.MorphSliders.Scripts.__files[2].__name=="helper.lua")
    assert(mods.MorphSliders.__absolute_path==active.__absolute_path)
    assert(mods.Legacy.marker=="legacy" and not mods.Native and not mods.Retired and not mods.Disabled)
    assert(not other.MorphSliders)
    -- Preserve an ordinary package even if another provider uses different case.
    mods.MorphSliders=nil;mods.morphsliders=stock;wrapped();assert(not mods.MorphSliders and mods.morphsliders==stock)
end
for _,text in ipairs({manifest:gsub("Session\tcurrent","Session\told"),"# Session\tcurrent\nA\tBad\t../outside\tmain.lua\tlua\n",
    "# Session\tcurrent\nN\tNative\tN/ue4ss/Native\tScripts/main.lua\tnative\n"}) do
    local e,install,_,_,_,_,original=setup(text)
    assert(not install(fixture) and e.IterateGameDirectories==original)
end
do
    local e,install,mods=setup(manifest)
    e.IterateGameDirectories=function() return nil,"fallback" end
    assert(install(fixture)); local a,b=e.IterateGameDirectories();assert(a==nil and b=="fallback" and not mods.MorphSliders)
end
do
    local e,install=setup(manifest)
    local problem={message="native error"}
    e.IterateGameDirectories=function() error(problem) end
    assert(install(fixture));local ok,result=pcall(e.IterateGameDirectories);assert(not ok and result==problem)
end
print("UE4SS_PACKAGE_DISCOVERY_PASSED active_only complete_nodes original_paths native_passthrough collisions idempotence no_polling")
