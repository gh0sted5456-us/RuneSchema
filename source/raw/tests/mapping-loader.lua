local script,folder=assert(arg[1]),assert(arg[2])
local request=folder.."/mapping-request"
local nonce="0123456789abcdef\n"
local active,scheduled,dumps=nil,{},0
os.getenv=function(name)assert(name=="RUNESCHEMA_AUTO_MAPPING_REQUEST");return request end
local function take_request()
    local pending=active
    active=nil
    if pending then return request,nonce:sub(1,-2) end
end
ExecuteWithDelay=function()error("unsafe async timer used")end
ExecuteInGameThread=function()error("unnecessary thread hop used")end
ExecuteInGameThreadWithDelay=function(delay,fn)assert(delay==15000);scheduled[#scheduled+1]=fn;return #scheduled end
DumpUSMAP=function(all)assert(all);dumps=dumps+1 end
local run=dofile(script)
local function write(text)local file=assert(io.open(request,"wb"));file:write(text);file:close()end
local function result()local file=assert(io.open(request..".result","rb"));local text=file:read("*a");file:close();return text end
write(nonce);run();assert(#scheduled==0 and dumps==0) -- A valid inherited request is ignored.
RuneSchemaTakeMappingRequest=take_request
run();assert(#scheduled==0 and dumps==0) -- The current core has no pending request.
write("invalid");active=true;run();assert(#scheduled==0)
write(nonce);active=true;run();run();assert(#scheduled==1 and dumps==0)
scheduled[1]();assert(dumps==1 and result()==nonce.."ok\n")
active=true;run();os.remove(request);scheduled[2]();assert(dumps==1)
write(nonce);DumpUSMAP=function()error("dump failed")end
active=true;run();scheduled[3]();assert(result()==nonce.."error\n")
ExecuteInGameThreadWithDelay=nil;active=true;run();assert(result()==nonce.."unavailable\n" and #scheduled==3)
ExecuteInGameThreadWithDelay=function()error("scheduler failed")end;active=true;run();assert(result()==nonce.."unavailable\n")
os.remove(request);os.remove(request..".result")
print("Mapping handoff: current core only, inherited and canceled requests ignored, one game-thread delay, and failure paths passed")
