local script,folder=assert(arg[1]),assert(arg[2])
local request=folder.."/mapping-request"
local nonce="0123456789abcdef\n"
local active,scheduled,dumps=nil,{},0
os.getenv=function(name)assert(name=="RUNESCHEMA_AUTO_MAPPING_REQUEST");return active end
ExecuteWithDelay=function()error("unsafe async timer used")end
ExecuteInGameThread=function()error("unnecessary thread hop used")end
ExecuteInGameThreadWithDelay=function(delay,fn)assert(delay==15000);scheduled[#scheduled+1]=fn;return #scheduled end
DumpUSMAP=function(all)assert(all);dumps=dumps+1 end
local run=dofile(script)
run();assert(#scheduled==0 and dumps==0)
active=request
local function write(text)local file=assert(io.open(request,"wb"));file:write(text);file:close()end
local function result()local file=assert(io.open(request..".result","rb"));local text=file:read("*a");file:close();return text end
write("invalid");run();assert(#scheduled==0)
write(nonce);run();assert(#scheduled==1 and dumps==0);scheduled[1]();assert(dumps==1 and result()==nonce.."ok\n")
DumpUSMAP=function()error("dump failed")end;run();scheduled[2]();assert(result()==nonce.."error\n")
ExecuteInGameThreadWithDelay=nil;run();assert(result()==nonce.."unavailable\n" and #scheduled==2)
ExecuteInGameThreadWithDelay=function()error("scheduler failed")end;run();assert(result()==nonce.."unavailable\n")
os.remove(request);os.remove(request..".result")
print("Mapping handoff: opt-in only, one game-thread delay, success, dump/scheduler failure and no async fallback passed")
