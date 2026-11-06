local file=assert(io.open(assert(arg[1]),"rb"));local source=file:read("*a");file:close()
local shared,scheduled,commands,publications,stopped={},{},{},0,false
local latest,host={},false
ModRef={GetSharedVariable=function(_,key)return shared[key] end,SetSharedVariable=function(_,key,value)shared[key]=value end}
for _,name in ipairs({"RegisterHook","RegisterKeyBind","NotifyOnNewObject","StaticFindObject","FindAllOf"}) do _G[name]=function()error("Mod Menu 2 must not install or open the original game menu")end end
ExecuteInGameThread=function()error("unexpected thread hop")end
ExecuteWithDelay=function()error("unsafe async timer")end
ExecuteInGameThreadWithDelay=function(delay,fn)assert(delay==100 or delay==1000);scheduled[#scheduled+1]=fn;return #scheduled end
local schema=[[{"schema":99,"id":"Example","name":"Example editor","helpy":{"layout":"character","tabs":[{"name":"Shape","keys":["Shape","Enabled"]},{"name":"Other","keys":["Caption"]}],"preview":{"channelVariable":"Example.preview"}},"settings":[{"key":"Enabled","type":"toggle","default":true},{"key":"Shape","type":"slider","min":0,"max":100,"default":10},{"key":"ServerLimit","type":"slider","min":0,"max":100,"default":10,"runsOn":"host"},{"key":"Style","type":"choice","options":["A","B"],"default":"A"},{"key":"Caption","type":"text","default":"A quote: \""},{"key":"OpenKey","type":"key","default":"F8"},{"type":"button","label":"Reset pose","action":"reset-pose"}]}]]
local config="; keep this comment\r\nEnabled = true\r\nShape = 10 ; shape comment\r\nUnknown = preserved\r\n"
local written=0
RuneSchemaModMenuExchange=function(text)
    local json=assert(RuneSchemaTestJson);local request,problem=json.decode(text);assert(request,problem)
    if request.Op=="catalog" then return '{"Pages":[{"Folder":"Example","Path":"C:/Mods/Example"}]}' end
    if request.Op=="read" then return json.encodeFlat({Exists=true,Text=request.Path:match("modmenu.json$") and schema or config},{"Exists","Text"}) end
    if request.Op=="write" then assert(request.Expected==config);config=request.Text;written=written+1;return '{"Saved":true}' end
    if request.Op=="publish" then assert(type(request.Snapshot.Pages)=="table");latest=request.Snapshot;publications=publications+1;return '{"Accepted":true}' end
    if request.Op=="next" then return #commands>0 and table.remove(commands,1) or '{"Active":true,"HostAuthority":'..tostring(host)..'}' end
    if request.Op=="log" then return '{"Accepted":true}' end
    error("Unexpected operation "..tostring(request.Op))
end
local nativeBridge=RuneSchemaModMenuExchange
-- Expose only the existing decoder in this isolated harness; production has no test API.
local adapted=source:gsub("local json = {}","local json = {}; RuneSchemaTestJson=json",1)
assert(load(adapted,"@helpy-mod-menu.lua"))()
assert(shared["ModMenu.version"]=="2.0.2" and shared["ModMenu.Example.Enabled"]==true and shared["ModMenu.Example.Shape"]==10)
assert(publications==1 and written==0 and #scheduled==1)
local function tick(command)if command then commands[#commands+1]='{"Active":true,"HostAuthority":'..tostring(host)..',"Command":'..command..'}' end;local fn=table.remove(scheduled,1);assert(fn);fn()end
tick('{"Op":"set","Id":"Example","Key":"Shape","Value":25,"Expected":10}')
assert(written==1 and shared["ModMenu.Example.Shape"]==25 and shared["ModMenu.Example.rev"]==1)
assert(config:find("; keep this comment",1,true) and config:find("shape comment",1,true) and config:find("Unknown = preserved",1,true) and config:find("\r\n",1,true))
tick('{"Op":"set","Id":"Example","Key":"Style","Value":"unlisted","Expected":"A"}')
assert(written==1 and shared["ModMenu.Example.Style"]=="A")
tick('{"Op":"button","Id":"Example","Action":"reset-pose"}')
assert(shared["ModMenu.Example.action"]=="reset-pose#1" and written==1)
shared["ModMenu.request.set"]="Example#1|Enabled=false|Shape=30";tick()
assert(shared["ModMenu.Example.Enabled"]==false and shared["ModMenu.Example.Shape"]==30)
shared["ModMenu.request.open"]="Example#1|row=Shape";tick()
assert(publications>=5 and latest.OpenRow=="Shape" and latest.OpenId=="Example")
assert(latest.Pages[1].Helpy.layout=="character" and #latest.Pages[1].Helpy.tabs==2)
local beforeWrites=written;tick('{"Op":"set","Id":"Example","Key":"ServerLimit","Value":20,"Expected":10}')
assert(written==beforeWrites and shared["ModMenu.Example.ServerLimit"]==10)
host=true;tick('{"Op":"set","Id":"Example","Key":"ServerLimit","Value":20,"Expected":10}')
assert(written==beforeWrites+1 and shared["ModMenu.Example.ServerLimit"]==20)
tick('{"Op":"page","Id":"Example","Rect":"view:10,20,800,600"}')
shared["Example.preview"]="Local\\RuneSchema.CharacterPreview.123.JBBB";tick()
assert(latest.Pages[1].PreviewChannel==shared["Example.preview"] and shared["ModMenu.rect"]=="view:10,20,800,600")
config=config:gsub("Shape = 30","Shape = 31");beforeWrites=written
tick('{"Op":"set","Id":"Example","Key":"Shape","Value":35,"Expected":30}')
assert(written==beforeWrites and config:find("Shape = 31",1,true))
local malformed,problem=RuneSchemaTestJson.decode(string.rep("[",40).."0"..string.rep("]",40));assert(not malformed and problem:find("nesting",1,true))
commands[#commands+1]='{"Active":false}';tick();assert(shared["ModMenu.version"]==nil and shared["ModMenu.page"]=="" and #scheduled==0)
-- An already-loaded separate Mod Menu prevents all discovery, writes and scheduling.
shared["ModMenu.version"]="2.0.2"
local before=#scheduled;local stops=0;RuneSchemaModMenuExchange=function(text)assert(text:find('"Op":"stop"',1,true));stops=stops+1;return '{}'end
assert(load(source))();assert(stops==1 and #scheduled==before)
RuneSchemaModMenuExchange=nil;assert(load(source))();assert(#scheduled==before)
for _,format in ipairs({"json","lua"}) do
    shared,scheduled,commands={},{},{};RuneSchemaModMenuExchange=nativeBridge
    schema='{"id":"Example","name":"Format test","config":{"format":"'..format..'","file":"settings.'..format..'"},"settings":[{"key":"Shape","type":"slider","min":0,"max":100,"default":10}]}'
    config=format=="json" and '{"Shape":10,"Extra":"kept"}' or '-- keep comment\nlocal Shape = 10 -- shape comment\nlocal Extra = 9\n'
    assert(load(adapted))();tick('{"Op":"set","Id":"Example","Key":"Shape","Value":20,"Expected":10}')
    assert(shared["ModMenu.Example.Shape"]==20)
    if format=="json" then local saved=assert(RuneSchemaTestJson.decode(config));assert(saved.Shape==20 and saved.Extra=="kept")
    else assert(config:find('-- keep comment',1,true) and config:find('shape comment',1,true) and config:find('local Extra = 9',1,true)) end
    commands[#commands+1]='{"Active":false}';tick();assert(#scheduled==0)
end
-- Helpy-only rows/presentation and complete templates are isolated from the legacy settings array.
shared,scheduled,commands={},{},{};RuneSchemaModMenuExchange=nativeBridge;host=false
schema=[[{"id":"Example","name":"Character","settings":[{"key":"Shape","type":"slider","min":0,"max":100,"default":10},{"key":"Enabled","type":"toggle","default":true},{"key":"Style","type":"choice","options":["A","B"],"default":"A"},{"key":"Caption","type":"text","maxLength":40,"default":"old"}],"--Helpy":{"schema":1,"layout":"character","controls":{"Shape":"dial","Style":"radial"},"settings":[{"key":"Hip","type":"dial","min":0,"max":5,"default":1}],"templates":{"id":"appearance","version":1,"keys":["Shape","Enabled","Style","Caption","Hip"],"contextVariable":"Example.character"}}}]]
config="; keep\nShape = 10\nEnabled = true\nStyle = A\nCaption = old\nHip = 1\nUnknown = retained\n"
shared['Example.character']='character-a';assert(load(adapted))()
assert(latest.Pages[1].Helpy.templates.id=='appearance' and latest.Pages[1].TemplateContext=='character-a')
assert(#latest.Pages[1].Settings==5 and latest.Pages[1].Settings[1].presentation=='dial' and latest.Pages[1].Settings[3].presentation=='radial' and latest.Pages[1].Settings[5].presentation=='dial')
local apply='{"Op":"template_apply","Id":"Example","TemplateId":"appearance","TemplateVersion":1,"Revision":0,"Context":"character-a","Expected":{"Shape":10,"Enabled":true,"Style":"A","Caption":"old","Hip":1},"Values":{"Shape":25,"Enabled":false,"Style":"B","Caption":"new","Hip":2}}'
local beforeWrites=written;tick(apply)
assert(written==beforeWrites+1 and shared['ModMenu.Example.Shape']==25 and shared['ModMenu.Example.Enabled']==false and shared['ModMenu.Example.Hip']==2 and shared['ModMenu.Example.rev']==1)
assert(config:find('Unknown = retained',1,true) and config:find('; keep',1,true))
local updated=apply:gsub('"Revision":0','"Revision":1'):gsub('"Expected":{.-},','"Expected":{"Shape":25,"Enabled":false,"Style":"B","Caption":"new","Hip":2},')
beforeWrites=written;tick(updated:gsub('"Hip":2}}','"Hip":99}}'));assert(written==beforeWrites and shared['ModMenu.Example.Shape']==25)
tick(updated:gsub('"Hip":2}}','"Hip":2,"Unexpected":true}}'));assert(written==beforeWrites)
shared['Example.character']='character-b';tick(updated);assert(written==beforeWrites and latest.Status:find('target changed',1,true))
shared['Example.character']='character-a';tick(apply);assert(written==beforeWrites and latest.Status:find('page changed',1,true))
commands[#commands+1]='{"Active":false}';tick();assert(#scheduled==0)
shared,scheduled={},{};local scheduler=ExecuteInGameThreadWithDelay;local failures=0
RuneSchemaModMenuExchange=function(text)local request=assert(RuneSchemaTestJson.decode(text));if request.Op=="stop" then failures=failures+1;return '{}' end;return nativeBridge(text) end
ExecuteInGameThreadWithDelay=nil;assert(load(source))();assert(failures==1 and #scheduled==0)
ExecuteInGameThreadWithDelay=function()error("fixture scheduler failure")end;assert(load(adapted))();assert(failures==2 and #scheduled==0 and shared["ModMenu.version"]==nil)
ExecuteInGameThreadWithDelay=scheduler
local radialOptions={};for i=1,13 do radialOptions[i]='"Option'..i..'"' end
schema='{"id":"Example","settings":[],"--Helpy":{"schema":1,"settings":[{"key":"LargeChoice","type":"radial","options":['..table.concat(radialOptions,",")..'],"default":"Option1"}]}}'
shared={};scheduled={};RuneSchemaModMenuExchange=nativeBridge;assert(load(adapted))();assert(latest.Pages[1].Settings[1].type=="choice" and latest.Pages[1].Settings[1].presentation==nil)
-- RGB/RGBA colors retain string configs and validate live edits and complete templates.
commands[#commands+1]='{"Active":false}';tick()
shared,scheduled,commands={},{},{};RuneSchemaModMenuExchange=nativeBridge
schema=[[{"id":"Example","settings":[{"key":"Tint","type":"text","default":"#112233"}],"--Helpy":{"schema":1,"controls":{"Tint":"color"},"settings":[{"key":"Accent","type":"color","alpha":true,"default":"#44556680"}],"templates":{"id":"colors","version":1,"keys":["Tint","Accent"]}}}]]
config="Tint = #112233\nAccent = #44556680\nUnknown = retained\n"
assert(load(adapted))();assert(latest.Pages[1].Settings[1].presentation=='color' and latest.Pages[1].Settings[2].colorAlpha==true)
local beforeWrites=written
tick('{"Op":"set","Id":"Example","Key":"Tint","Expected":"#112233","Value":"#FF0088"}')
assert(written==beforeWrites+1 and shared['ModMenu.Example.Tint']=='#FF0088' and config:find('Unknown = retained',1,true))
beforeWrites=written
for _,invalid in ipairs({'123','"#GG0000"','"#11223344"','"red"','"#123"','"#112233\\n"'})do
    tick('{"Op":"set","Id":"Example","Key":"Tint","Expected":"#FF0088","Value":'..invalid..'}')
    assert(written==beforeWrites and shared['ModMenu.Example.Tint']=='#FF0088')
end
local colorTemplate='{"Op":"template_apply","Id":"Example","TemplateId":"colors","TemplateVersion":1,"Revision":1,"Context":"","Expected":{"Tint":"#FF0088","Accent":"#44556680"},"Values":{"Tint":"#ABCDEF","Accent":"#10203040"}}'
tick(colorTemplate:gsub('#10203040','#102030'));assert(written==beforeWrites)
tick(colorTemplate);assert(written==beforeWrites+1 and shared['ModMenu.Example.Tint']=='#ABCDEF' and shared['ModMenu.Example.Accent']=='#10203040')
commands[#commands+1]='{"Active":false}';tick()
shared,scheduled,commands={},{},{};schema=[[{"id":"Example","settings":[],"--Helpy":{"schema":1,"settings":[{"key":"Bad","type":"color","default":"#112233payload"},{"key":"BadAlpha","type":"color","alpha":"true","default":"#11223344"},{"key":"DefaultColor","type":"color"}]}}]]
config='';assert(load(adapted))();assert(#latest.Pages[1].Settings==1 and shared['ModMenu.Example.DefaultColor']=='#FFFFFF' and shared['ModMenu.Example.Bad']==nil)
commands[#commands+1]='{"Active":false}';tick()
print("PASS: legacy schemas, text/JSON/Lua config preservation, callbacks, host controls, row requests, preview extension, isolated --Helpy rows, atomic templates, stale-target rejection, competing-mod guards, validated RGB/RGBA colors and game-thread scheduling")
