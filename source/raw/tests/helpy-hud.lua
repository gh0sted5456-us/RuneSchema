local root=assert(arg[1])
local tick,cancelled,registered,values,errors,commands,reads=nil,0,{}, {}, {},{},0
function LoopInGameThreadWithDelay(delay,callback) assert(delay==100);tick=callback;return 42 end
function CancelDelayedAction(id) assert(id==42);cancelled=cancelled+1 end
function RuneSchemaHUDRegister(owner,id,fields) registered[owner.."/"..id]=fields;return "" end
function RuneSchemaHUDUnregister(owner,id) registered[owner.."/"..id]=nil;return "" end
function RuneSchemaHUDSet(owner,id,field,value) values[field]=value;return "" end
function RuneSchemaHUDError(owner,id,message) errors[#errors+1]=message;return "" end
function RuneSchemaHUDNext() reads=reads+1;local row=table.remove(commands,1);if not row then return "","","","","","" end;return table.unpack(row) end
local api=dofile(root.."/helpy-hud.lua")
assert(not tick,"No idle loop before a mod registers")
local seen={};local corrected=false
api.Register("Example","editor",{enabled=function(value) assert(type(value)=="boolean");seen.enabled=value end,size=function(value) assert(type(value)=="number");seen.size=value;if corrected then api.Set("Example","editor","size",value+1) end end,apply=function(value) assert(value==nil);seen.apply=true end,broken=function() error("mod error") end})
assert(registered["Example/editor"] and tick)
for _,row in ipairs({{"enabled","toggle","false"},{"size","slider","1.25"},{"apply","button","null"},{"broken","button","null"}}) do commands[#commands+1]={"Example","editor",row[1],row[2],row[3],"Example/editor"} end
tick();assert(seen.enabled==false and seen.size==1.25 and seen.apply and values.enabled=="false" and #errors==1)
api.Set("Example","editor","note","Text");assert(values.note=="Text")
corrected=true;commands={{"Example","editor","size","slider","1.25","Example/editor"}};tick();assert(values.size=="2.25","The mod owns its corrected value");corrected=false
reads=0;for i=1,20 do commands[#commands+1]={"Example","editor","apply","button","null","Example/editor"} end
tick();assert(reads==8 and #commands==12,"Tick must be bounded")
api.Unregister("Example","editor");assert(cancelled==1 and not registered["Example/editor"])
local old=reads;tick();assert(reads==old,"Cancelled generations cannot dispatch")
RuneSchemaHUD=nil;tick=nil;LoopInGameThreadWithDelay=function() error("unsupported") end
api=dofile(root.."/helpy-hud.lua")
assert(not pcall(api.Register,"Example","editor",{apply=function() end}) and not registered["Example/editor"],"Failed scheduling cannot leave a live UI")
print("HUD declarations/callback scheduler passed")
