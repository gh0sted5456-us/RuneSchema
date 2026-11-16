local run=dofile(assert(arg[1]))
local list,queued,logs,spawned,pre,post={}, {}, {},0,0,0
local mapHook,beginHook
local function validObject(value) value.IsValid=function()return true end;return value end
local callback=validObject(setmetatable({}, {__call=function()pre=pre+1 end}))
local actorClass=validObject({})
local world=validObject({SpawnActor=function(_,class)assert(class==actorClass);spawned=spawned+1;return validObject({PreBeginPlay=callback})end})
local helpers=validObject({GetAsset=function(_,asset)assert(asset.PackageName=='/Game/Mods/Example/ModActor' and asset.AssetName=='ModActor_C');return actorClass end})
local UEHelpers={GetWorld=function()return world end,FindOrAddFName=function(name)return name end}
require=function(name)assert(name=='UEHelpers');return UEHelpers end
StaticFindObject=function(path)assert(path=='/Script/AssetRegistry.Default__AssetRegistryHelpers');return helpers end
io.open=function(path,mode)
    assert(path=='fixture/settings/logicmods.lua.generated.txt' and mode=='r')
    return {close=function()end,lines=function()local i=0;return function()i=i+1;return list[i]end end}
end
print=function(message)logs[#logs+1]=message end
RegisterLoadMapPostHook=function(fn)mapHook=fn end
RegisterBeginPlayPostHook=function(fn)beginHook=fn end
ExecuteInGameThread=function()error('legacy dispatch used')end
ExecuteWithDelay=function()error('unsafe async timer used')end
local function schedule(delay,fn)assert(delay==0);queued[#queued+1]=fn;return #queued end
local function contains(text)for _,line in ipairs(logs)do if line:find(text,1,true)then return true end end;return false end
local function reset()
    list={'Example','Example','../Invalid'};queued={};logs={};spawned=0;pre=0;post=0;mapHook=nil;beginHook=nil
    ExecuteInGameThreadWithDelay=schedule;UEHelpers.GetWorld=function()return world end
end
reset();run('fixture')
assert(#queued==1 and spawned==0 and type(mapHook)=='function' and type(beginHook)=='function')
queued[1]();assert(spawned==1 and pre==1)
queued[1]();assert(spawned==1) -- An already-started package cannot be spawned twice.
local context={get=function()return validObject({
    GetClass=function()return validObject({GetFullName=function()return 'BlueprintGeneratedClass /Game/Mods/Example/ModActor.ModActor_C' end})end,
    PostBeginPlay=validObject(setmetatable({}, {__call=function()post=post+1 end}))
})end}
beginHook(context);assert(post==1)
mapHook(nil,{get=function()return world end});assert(spawned==2 and pre==2)
reset();ExecuteInGameThreadWithDelay=nil;run('fixture')
assert(#queued==0 and spawned==0 and contains('[INITIAL][DEFERRED]'))
mapHook(nil,{get=function()return world end});assert(spawned==1)
reset();ExecuteInGameThreadWithDelay=function()error('scheduler failed')end;run('fixture')
assert(#queued==0 and contains('scheduler failed') and contains('load-map hook retained'))
mapHook(nil,{get=function()return world end});assert(spawned==1)
reset();run('fixture');UEHelpers.GetWorld=function()error('world unavailable')end
queued[1]();assert(spawned==0 and contains('[INITIAL][FAILED]'))
mapHook(nil,{get=function()return world end});assert(spawned==1)
reset();list={};run('fixture');assert(#queued==0 and mapHook==nil and beginHook==nil)
io.stdout:write('ModActor loader: deferred initial spawn, duplicate suppression, map hooks, optional scheduling and callback failure passed\n')
