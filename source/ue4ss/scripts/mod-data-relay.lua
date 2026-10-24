-- Optional public client helper. Data is opaque; this module never loads assets.
if RuneSchemaRelay then return RuneSchemaRelay end
local api = {Status={}}
local routes, pending, seen, outgoing = {}, {}, {}, {}
local reserved, queued, sequence, revision = 0, 0, 0, 0
local endpoint, hooked, receipt_hooked, running, last_scan, last_subscribe = nil, false, false, false, 0, 0
local path = "/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C:ClientRuneSchemaNotification"
local function token(s) return type(s)=="string" and #s>0 and #s<=193 and s:match("^[%w_.:%-]+$")~=nil end
local function crc(s)
    local v=0xffffffff
    for i=1,#s do v=v~s:byte(i);for _=1,8 do v=(v>>1)~((-(v&1))&0xedb88320) end end
    return (~v)&0xffffffff
end
local function hex(s) return (s:gsub(".",function(c)return string.format("%02x",c:byte())end)) end
local function unhex(s) assert(#s<=2048 and #s%2==0 and not s:find("[^0-9a-f]"),"invalid chunk");return (s:gsub("..",function(v)return string.char(tonumber(v,16))end)) end
local function packet(m,part,sender)
    return string.format('{"bytes":%d,"channel":"%s","crc":%u,"hex":"%s","part":%d,%s"sequence":%d,"total":%d,"v":1,"version":%d}',
        #m.data,m.channel,m.crc,hex(m.data:sub(part*1024+1,(part+1)*1024)),part,sender and ('"sender":"'..sender..'",') or "",m.sequence,(#m.data+1023)//1024,m.version)
end
local function forget(key) if pending[key] then reserved=reserved-pending[key].bytes;pending[key]=nil end end
local function receive(text)
    assert(type(text)=="string" and #text<=4096,"oversized relay packet")
    local function number(k) local n=tonumber(text:match('"'..k..'":(%d+)'));assert(n and math.type(n)=="integer","invalid packet integer");return n end
    local channel=text:match('"channel":"([%w_.:%-]+)"');local sender=text:match('"sender":"([%w_.:%-]+)"');local code=text:match('"hex":"([0-9a-f]*)"')
    assert(token(channel) and token(sender) and code,"invalid relay identity")
    local route=routes[channel];if not route then return end
    local version,seq,part,total,bytes,sum=number("version"),number("sequence"),number("part"),number("total"),number("bytes"),number("crc")
    assert(number("v")==1 and version==route.version and seq>0 and bytes>0 and bytes<=65536 and total==(bytes+1023)//1024 and part<total and sum<=0xffffffff,"invalid relay bounds")
    local chunk=unhex(code);assert(#chunk==math.min(1024,bytes-part*1024),"invalid chunk size")
    local canonical=string.format('{"bytes":%d,"channel":"%s","crc":%u,"hex":"%s","part":%d,"sender":"%s","sequence":%d,"total":%d,"v":1,"version":%d}',bytes,channel,sum,code,part,sender,seq,total,version)
    assert(text==canonical,"noncanonical relay packet")
    local key=sender.."/"..channel;if seq<=(seen[key] or 0) then return end
    local a=pending[key]
    if a and part==0 and seq>a.sequence then forget(key);a=nil end
    if not a then
        local count=0;for _ in pairs(pending) do count=count+1 end
        assert(part==0 and count<256 and reserved+bytes<=2097152,"relay receive capacity exceeded")
        a={bytes=bytes,sequence=seq,total=total,crc=sum,next=0,chunks={},deadline=os.time()+30};pending[key]=a;reserved=reserved+bytes
    end
    assert(a.sequence==seq and a.bytes==bytes and a.total==total and a.crc==sum and a.next==part,"interleaved/duplicate relay chunk")
    a.chunks[#a.chunks+1]=chunk;a.next=a.next+1
    if a.next==a.total then
        local data=table.concat(a.chunks);forget(key);assert(crc(data)==sum,"relay checksum mismatch")
        local count=0;for _ in pairs(seen) do count=count+1 end;assert(seen[key] or count<4096,"relay replay table capacity exceeded")
        seen[key]=seq;route.callback(sender,data,seq)
    end
end
local function value(param) local v=param:get();if type(v)=="string" or type(v)=="number" or type(v)=="boolean" then return v end;return v:ToString() end
local function reset() endpoint=nil;pending={};seen={};outgoing={};reserved=0;queued=0;last_subscribe=0 end
local function find_endpoint()
    for _,component in ipairs(FindAllOf("BPC_RuneSchemaRegistryBridge_C") or {}) do
        if component:IsValid() then local owner=component:GetOwner();if owner:IsValid() and owner:IsLocallyControlled() then return component end end
    end
end
local function request(action,channel,text)
    revision=revision+1;endpoint:ServerRequestRuneSchemaAction("mod.data",channel,action,text,revision)
end
function api.Tick()
    local now=os.time();for key,a in pairs(pending) do if a.deadline<=now then forget(key) end end
    if endpoint and not endpoint:IsValid() then reset() end
    if not endpoint and now-last_scan>=1 then
        last_scan=now;local ok,found=pcall(find_endpoint);if ok then endpoint=found end
    end
    if not endpoint then return end
    if not hooked then
        local ok=pcall(RegisterHook,path,function(_,channel,_,payload)
            if value(channel)~="mod.data" then return end
            local accepted,problem=pcall(receive,value(payload));if not accepted then api.LastError=tostring(problem) end
        end)
        if not ok then return end;hooked=true
    end
    if not receipt_hooked then
        local ok=pcall(RegisterHook,path:gsub("ClientRuneSchemaNotification$","ClientRuneSchemaReceipt"),function(_,channel,entity,_,success,detail)
            if value(channel)=="mod.data" then local key=value(entity);api.Status[key]=value(success) and "accepted" or value(detail);if not value(success) then api.LastError=api.Status[key] end end
        end)
        if not ok then return end;receipt_hooked=true
    end
    if now-last_subscribe>=20 then for _,r in pairs(routes) do r.subscribed=false end;last_subscribe=now end
    for channel,r in pairs(routes) do if not r.subscribed then request(r.resync and "resync" or "subscribe",channel,'{"version":'..r.version..'}');r.subscribed=true;r.resync=false;return end end
    local next=outgoing[1];if not next then return end
    request("chunk",next.channel,packet(next,next.part));next.part=next.part+1
    if next.part*1024>=#next.data then queued=queued-#next.data;table.remove(outgoing,1) end
end
local function schedule()
    if running then return end
    assert(type(ExecuteWithDelay)=="function" and type(ExecuteInGameThread)=="function","UE4SS scheduling is unavailable")
    running=true
    local function step() ExecuteInGameThread(function()
        local ok,problem=pcall(api.Tick);if not ok then api.LastError=tostring(problem) end
        ExecuteWithDelay(250,step)
    end) end
    ExecuteWithDelay(250,step)
end
function api.Open(channel,version,callback)
    assert(token(channel) and channel:match("^[%w_.%-]+:[%w_.%-]+$") and type(version)=="number" and math.type(version)=="integer" and version>=1 and version<=65535 and type(callback)=="function","invalid relay subscription")
    local count=0;for _ in pairs(routes) do count=count+1 end;assert(routes[channel] or count<64,"too many relay channels")
    assert(not routes[channel] or routes[channel].version==version,"conflicting relay version")
    routes[channel]={version=version,callback=callback,subscribed=false};schedule();return api
end
function api.Send(channel,data)
    assert(routes[channel] and type(data)=="string" and #data>0 and #data<=65536 and #outgoing<16 and queued+#data<=262144,"invalid/oversized relay send")
    sequence=sequence+1;queued=queued+#data;outgoing[#outgoing+1]={channel=channel,version=routes[channel].version,data=data,sequence=sequence,crc=crc(data),part=0};return sequence
end
function api.Resync(channel)
    assert(routes[channel],"open channel first")
    for key in pairs(seen) do if key:sub(-#channel-1)=="/"..channel then seen[key]=nil end end
    for key in pairs(pending) do if key:sub(-#channel-1)=="/"..channel then forget(key) end end
    routes[channel].resync=true;routes[channel].subscribed=false
end
RuneSchemaRelay=api
return api
