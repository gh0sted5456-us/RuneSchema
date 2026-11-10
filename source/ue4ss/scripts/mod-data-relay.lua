-- Optional public client helper. Data is opaque; this module never loads assets.
if RuneSchemaRelay then return RuneSchemaRelay end
local api = {Status={}}
local routes, pending, seen, outgoing = {}, {}, {}, {}
local reserved, queued, sequence, revision, seen_count = 0, 0, 0, 0, 0
local endpoint, hooked, receipt_hooked, running, last_scan = nil, false, false, false, 0
local timer, generation = nil, 0
local path = "/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C:ClientRuneSchemaNotification"
local legacy_bytes, max_bytes, incoming_budget, outgoing_budget = 1048576, 4194304, 16777216, 16777216
local max_routes, max_replay_entries = 512, 128 * 512
local function token(s) return type(s)=="string" and #s>0 and #s<=193 and s:match("^[%w_.:%-]+$")~=nil end
local crc_table, encode_hex, decode_hex={},{},{}
for i=0,255 do
    local v=i;for _=1,8 do v=(v>>1)~((-(v&1))&0xedb88320) end;crc_table[i]=v
    local byte,pair=string.char(i),string.format("%02x",i)
    encode_hex[byte]=pair;decode_hex[pair]=byte
end
local function crc_update(v,s,first,last) for i=first or 1,last or #s do v=(v>>8)~crc_table[(v~s:byte(i))&255] end;return v end
local function hex(s) return (s:gsub(".",encode_hex)) end
local function unhex(s) assert(#s<=2048 and #s%2==0 and not s:find("[^0-9a-f]"),"invalid chunk");return (s:gsub("..",decode_hex)) end
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
    assert(number("v")==1 and version==route.version and seq>0 and bytes>0 and bytes<=max_bytes and total==(bytes+1023)//1024 and part<total and sum<=0xffffffff,"invalid relay bounds")
    local chunk=unhex(code);assert(#chunk==math.min(1024,bytes-part*1024),"invalid chunk size")
    local canonical=string.format('{"bytes":%d,"channel":"%s","crc":%u,"hex":"%s","part":%d,"sender":"%s","sequence":%d,"total":%d,"v":1,"version":%d}',bytes,channel,sum,code,part,sender,seq,total,version)
    assert(text==canonical,"noncanonical relay packet")
    local key=sender.."/"..channel;if seq<=(seen[key] or 0) then return end
    local a=pending[key]
    if a and part==0 and seq>a.sequence then forget(key);a=nil end
    if not a then
        local count=0;for _ in pairs(pending) do count=count+1 end
        assert(part==0 and count<256 and reserved+bytes<=incoming_budget,"relay receive capacity exceeded")
        a={bytes=bytes,sequence=seq,total=total,crc=sum,rolling=0xffffffff,next=0,chunks={}}
        pending[key]=a;reserved=reserved+bytes
    end
    assert(a.sequence==seq and a.bytes==bytes and a.total==total and a.crc==sum and a.next==part,"interleaved/duplicate relay chunk")
    a.chunks[#a.chunks+1]=chunk;a.rolling=crc_update(a.rolling,chunk);a.next=a.next+1
    a.deadline=os.time()+30
    if a.next==a.total then
        local data=table.concat(a.chunks);forget(key);assert(((~a.rolling)&0xffffffff)==sum,"relay checksum mismatch")
        if not seen[key] then
            assert(seen_count<max_replay_entries,"relay replay table capacity exceeded")
            seen_count=seen_count+1
        end
        seen[key]=seq;route.callback(sender,data,seq)
    end
end
local function value(param) local v=param:get();if type(v)=="string" or type(v)=="number" or type(v)=="boolean" then return v end;return v:ToString() end
local function reset()
    endpoint=nil;pending={};seen={};outgoing={};reserved=0;queued=0;seen_count=0
    for _,route in pairs(routes) do route.last_attempt=nil;route.subscription=nil;route.capacity=nil;route.fast=nil end
end
local function find_endpoint()
    for _,component in ipairs(FindAllOf("BPC_RuneSchemaRegistryBridge_C") or {}) do
        if component:IsValid() then local owner=component:GetOwner();if owner:IsValid() and owner:IsLocallyControlled() then return component end end
    end
end
local function request(action,channel,text)
    revision=revision+1;endpoint:ServerRequestRuneSchemaAction("mod.data",channel,action,text,revision)
end
local function subscribe(channel,route,now)
    route.subscription=revision+1
    route.last_attempt=now
    local action=route.resync and "resync" or "subscribe"
    route.resync=false
    request(action,channel,('{"version":%d,"maxBytes":%d}'):format(route.version,max_bytes))
end
function api.Tick()
    if not next(routes) then return end
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
        local ok=pcall(RegisterHook,path:gsub("ClientRuneSchemaNotification$","ClientRuneSchemaReceipt"),function(_,channel,entity,received_revision,success,detail)
            if value(channel)~="mod.data" then return end
            local key,accepted,reason=value(entity),value(success),value(detail)
            local route=routes[key]
            if route and received_revision and route.subscription==value(received_revision) then
                local limit=accepted and tonumber(reason:match("^relay%-capacity%-v2:(%d+)$"))
                route.fast=limit and limit>0 and limit<=max_bytes
                route.capacity=accepted and (route.fast and limit or legacy_bytes) or 0
            end
            api.Status[key]=accepted and "accepted" or reason
            if not accepted then api.LastError=reason end
        end)
        if not ok then return end;receipt_hooked=true
    end
    local transfer=outgoing[1]
    if transfer then
        local route=routes[transfer.channel]
        if not route.last_attempt or route.resync then subscribe(transfer.channel,route,now);return end
        if #transfer.data>legacy_bytes and not route.capacity then
            transfer.waiting_since=transfer.waiting_since or now
            if now-transfer.waiting_since<40 then
                if now-route.last_attempt>=20 then subscribe(transfer.channel,route,now) end
                return
            end
        end
        if #transfer.data>(route.capacity or legacy_bytes) then
            api.LastError="Relay destination does not support this transfer size"
        else
            if not transfer.crc then
                local last=math.min(#transfer.data,transfer.checked+65536)
                transfer.rolling=crc_update(transfer.rolling,transfer.data,transfer.checked+1,last)
                transfer.checked=last
                if last<#transfer.data then return end
                transfer.crc=(~transfer.rolling)&0xffffffff
            end
            local chunks=route.fast and 3 or 1
            for _=1,chunks do
                request("chunk",transfer.channel,packet(transfer,transfer.part))
                transfer.part=transfer.part+1
                if transfer.part*1024>=#transfer.data then break end
            end
            if transfer.part*1024<#transfer.data then return end
        end
        queued=queued-#transfer.data
        table.remove(outgoing,1)
        return
    end
    for channel,route in pairs(routes) do
        if route.resync or not route.last_attempt then subscribe(channel,route,now);return end
    end
    for channel,route in pairs(routes) do
        if now-route.last_attempt>=20 then subscribe(channel,route,now);return end
    end
end
local function schedule()
    if running then return end
    assert(type(LoopInGameThreadWithDelay)=="function" and type(CancelDelayedAction)=="function","UE4SS game-thread scheduling is unavailable")
    generation=generation+1;local current=generation
    local ok,handle=pcall(LoopInGameThreadWithDelay,250,function()
        if not running or generation~=current then return end
        local ok,problem=pcall(api.Tick);if not ok then api.LastError=tostring(problem) end
    end)
    assert(ok,"UE4SS game-thread scheduling failed: "..tostring(handle))
    timer=handle;running=true
end
function api.Open(channel,version,callback)
    assert(token(channel) and channel:match("^[%w_.%-]+:[%w_.%-]+$") and type(version)=="number" and math.type(version)=="integer" and version>=1 and version<=65535 and type(callback)=="function","invalid relay subscription")
    local count=0;for _ in pairs(routes) do count=count+1 end;assert(routes[channel] or count<max_routes,"too many relay channels")
    assert(not routes[channel] or routes[channel].version==version,"conflicting relay version")
    local previous=routes[channel];routes[channel]={version=version,callback=callback}
    local ok,problem=pcall(schedule);if not ok then routes[channel]=previous;error(problem,0) end;return api
end
function api.Close(channel)
    if not routes[channel] then return false end
    routes[channel]=nil;api.Status[channel]=nil
    for key in pairs(pending) do if key:sub(-#channel-1)=="/"..channel then forget(key) end end
    for key in pairs(seen) do if key:sub(-#channel-1)=="/"..channel then seen[key]=nil;seen_count=seen_count-1 end end
    for i=#outgoing,1,-1 do if outgoing[i].channel==channel then queued=queued-#outgoing[i].data;table.remove(outgoing,i) end end
    if not next(routes) then
        running=false;generation=generation+1
        if timer then pcall(CancelDelayedAction,timer);timer=nil end
        reset()
    end
    return true
end
function api.Send(channel,data)
    assert(routes[channel] and type(data)=="string" and #data>0 and #data<=max_bytes and #outgoing<16 and queued+#data<=outgoing_budget,"invalid/oversized relay send")
    sequence=sequence+1;queued=queued+#data
    outgoing[#outgoing+1]={channel=channel,version=routes[channel].version,data=data,sequence=sequence,rolling=0xffffffff,checked=0,part=0}
    return sequence
end
function api.Resync(channel)
    assert(routes[channel],"open channel first")
    for key in pairs(seen) do if key:sub(-#channel-1)=="/"..channel then seen[key]=nil;seen_count=seen_count-1 end end
    for key in pairs(pending) do if key:sub(-#channel-1)=="/"..channel then forget(key) end end
    routes[channel].resync=true
end
RuneSchemaRelay=api
return api
