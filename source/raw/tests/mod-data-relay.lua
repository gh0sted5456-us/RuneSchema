local helper=assert(arg[1])
local hooks,calls,timers={}, {}, {}
local now=100;os.time=function()return now end
ExecuteWithDelay=function()error("unsafe async timer used")end
ExecuteInGameThread=function()error("unnecessary thread hop used")end
LoopInGameThreadWithDelay=function(delay,fn)assert(delay==250);timers[#timers+1]=fn;return #timers end
local canceled={};CancelDelayedAction=function(handle)canceled[handle]=true end
RegisterHook=function(path,fn)hooks[path]=fn;return 1,2 end
local alive=true
local owner={IsValid=function()return true end,IsLocallyControlled=function()return true end}
local endpoint={IsValid=function()return alive end,GetOwner=function()return owner end,
    ServerRequestRuneSchemaAction=function(_,channel,entity,action,payload,revision)calls[#calls+1]={channel,entity,action,payload,revision}end}
FindAllOf=function()return alive and {endpoint} or {}end
local api=dofile(helper);assert(dofile(helper)==api and #timers==0)
local received={};api.Open("RSHC:appearance",1,function(sender,data,sequence)received[#received+1]={sender,data,sequence}end)
timers[1]();assert(calls[1][3]=="subscribe" and calls[1][4]=='{"version":1,"maxBytes":4194304}')
api.Send("RSHC:appearance","123456789");api.Tick()
assert(calls[2][3]=="chunk" and calls[2][4]:find('"crc":3421780262',1,true))
local function param(value)return {get=function()return value end}end
local notification=hooks["/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C:ClientRuneSchemaNotification"]
local packet='{"bytes":9,"channel":"RSHC:appearance","crc":3421780262,"hex":"313233343536373839","part":0,"sender":"peer-1-2","sequence":1,"total":1,"v":1,"version":1}'
notification(nil,param("mod.data"),nil,param(packet));assert(#received==1 and received[1][2]=="123456789")
notification(nil,param("mod.data"),nil,param(packet));assert(#received==1)
notification(nil,param("mod.data"),nil,param(packet:gsub('"bytes":9','"bytes":999999999')));assert(api.LastError and #received==1)
local receipt=hooks["/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C:ClientRuneSchemaReceipt"]
receipt(nil,param("mod.data"),param("RSHC:appearance"),nil,param(false),param("denied"));assert(api.Status["RSHC:appearance"]=="denied")
api.Resync("RSHC:appearance");api.Tick();assert(calls[#calls][3]=="resync")
notification(nil,param("mod.data"),nil,param(packet));assert(#received==2)
api.Send("RSHC:appearance",string.rep("x",3000));for _=1,3 do api.Tick()end;assert(calls[#calls][4]:find('"part":2',1,true))
api.Send("RSHC:appearance",string.rep("x",65537))
local before=#received
for part=0,1023 do
    local large=string.format('{"bytes":1048576,"channel":"RSHC:appearance","crc":3677983029,"hex":"%s","part":%d,"sender":"peer-1-2","sequence":2,"total":1024,"v":1,"version":1}',string.rep("7a",1024),part)
    notification(nil,param("mod.data"),nil,param(large))
    if part%2==0 then now=now+10;api.Tick() end
end
assert(#received==before+1 and #received[#received][2]==1048576)
local ok=pcall(api.Send,"RSHC:appearance",string.rep("x",4194305));assert(not ok)
alive=false;api.Tick();assert(#timers==1)
api.Open("RSHC:second",1,function()end);assert(#timers==1)
assert(api.Close("RSHC:appearance") and not canceled[1]);assert(api.Close("RSHC:second") and canceled[1])
assert(not api.Close("RSHC:second"));local previous=#calls
timers[1]();api.Tick();assert(#calls==previous)
alive=true;now=now+2;api.Open("RSHC:appearance",1,function()end);assert(#timers==2)
timers[1]();assert(#calls==previous);timers[2]();assert(#calls==previous+1)
local latest=calls[#calls][5]
receipt(nil,param("mod.data"),param("RSHC:appearance"),param(latest),param(true),param("accepted"))
api.Send("RSHC:appearance",string.rep("x",4194304));api.Tick()
assert(#calls==previous+1 and api.LastError:find("transfer size",1,true))
api.Resync("RSHC:appearance");api.Tick()
latest=calls[#calls][5]
receipt(nil,param("mod.data"),param("RSHC:appearance"),param(latest),param(true),param("relay-capacity-v2:4194304"))
api.Send("RSHC:appearance",string.rep("x",4194304))
previous=#calls
for _=1,63 do api.Tick() end
assert(#calls==previous)
api.Tick()
assert(#calls==previous+3 and calls[#calls][4]:find('"part":2',1,true) and calls[#calls][4]:find('"crc":3348915610',1,true))
local original=api.Tick;api.Tick=function()error("tick fixture failure")end;timers[2]();assert(api.LastError:find("tick fixture failure",1,true));api.Tick=original
api.Close("RSHC:appearance");assert(canceled[2])
local schedule=LoopInGameThreadWithDelay;LoopInGameThreadWithDelay=function()error("timer unavailable")end
assert(not pcall(api.Open,"RSHC:failed",1,function()end));assert(not pcall(api.Send,"RSHC:failed","x"))
LoopInGameThreadWithDelay=schedule
local binary
api.Open("RSHC:failed",1,function(_,data)binary=data end);assert(#timers==3)
now=now+2;api.Tick()
local bytes={}
for i=0,255 do bytes[#bytes+1]=string.char(i) end
local data=table.concat(bytes)
api.Send("RSHC:failed",data);api.Tick()
local wire=calls[#calls][4]
local pairs={}
for i=0,255 do pairs[#pairs+1]=string.format("%02x",i) end
assert(wire:find('"hex":"'..table.concat(pairs)..'"',1,true))
notification(nil,param("mod.data"),nil,param(wire:gsub('("part":0,)','%1"sender":"authority",')))
assert(binary==data)
api.Close("RSHC:failed")
print("Lua relay API: scheduling, chunk/CRC interoperability, receive/replay, bounds, receipts, resync and disconnect passed")
