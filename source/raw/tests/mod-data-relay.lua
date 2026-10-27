local helper=assert(arg[1])
local hooks,calls,timers={}, {}, {}
local now=100;os.time=function()return now end
ExecuteWithDelay=function(_,fn)timers[#timers+1]=fn end
ExecuteInGameThread=function(fn)fn()end
RegisterHook=function(path,fn)hooks[path]=fn;return 1,2 end
local alive=true
local owner={IsValid=function()return true end,IsLocallyControlled=function()return true end}
local endpoint={IsValid=function()return alive end,GetOwner=function()return owner end,
    ServerRequestRuneSchemaAction=function(_,channel,entity,action,payload,revision)calls[#calls+1]={channel,entity,action,payload,revision}end}
FindAllOf=function()return alive and {endpoint} or {}end
local api=dofile(helper);assert(dofile(helper)==api)
local received={};api.Open("RSHC:appearance",1,function(sender,data,sequence)received[#received+1]={sender,data,sequence}end)
api.Tick();assert(calls[1][3]=="subscribe" and calls[1][4]=='{"version":1}')
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
end
assert(#received==before+1 and #received[#received][2]==1048576)
local ok=pcall(api.Send,"RSHC:appearance",string.rep("x",1048577));assert(not ok)
alive=false;api.Tick();assert(#timers==1)
print("Lua relay API: scheduling, chunk/CRC interoperability, receive/replay, bounds, receipts, resync and disconnect passed")
