local script=assert(arg[1]);local shared,scheduled,cancelled,logs={},{},{},{}
local config='Height = 1.1 ; retained comment\nSkinColor = #AABBCC\nEnabled = false\nUnknown = ignored\nBuild = os.execute("must not execute")\n'
-- Keep the fixture read-only and independent of the game's installed folders.
io.open=function(path,mode)
 assert(path:match('/config%.txt$') and mode=='r')
 return {lines=function()return config:gmatch('[^\n]+')end,close=function()end}
end
print=function(text)logs[#logs+1]=text end
ModRef={SetSharedVariable=function(_,key,value)shared[key]=value end,GetSharedVariable=function(_,key)return shared[key]end}
LoopInGameThreadWithDelay=function(delay,fn)assert(delay==250);scheduled[#scheduled+1]=fn;return #scheduled end
CancelDelayedAction=function(handle)cancelled[#cancelled+1]=handle end
ExecuteWithDelay=function()error('unsafe async timer')end
ExecuteInGameThread=function()error('unexpected legacy dispatch')end
local api=dofile(script);assert(api.Active and #scheduled==1 and api.Values.Height==1.1 and api.Values.Enabled==false and api.Values.Build==0.5 and shared['HelpyEditorExample.target']=='example-preview')
local notifications=0;api.OnSettingsChanged=function(values,rev)notifications=notifications+1;assert(values.SkinColor=='#AABBCC' and rev==2)end
scheduled[1]();assert(notifications==0)
shared['ModMenu.version']='2.0.2';shared['ModMenu.HelpyEditorExample.rev']=2;shared['ModMenu.HelpyEditorExample.Height']=1.2
scheduled[1]();scheduled[1]();assert(notifications==1 and api.Values.Height==1.2)
assert(api.SelectTarget('character-b') and shared['HelpyEditorExample.target']=='character-b');assert(not api.SelectTarget('bad\nvalue'))
shared['ModMenu.HelpyEditorExample.rev']=3;shared['ModMenu.HelpyEditorExample.SkinColor']='bad';scheduled[1]();assert(not api.Active and #cancelled==1 and api.Values.SkinColor=='#AABBCC')
local loop=LoopInGameThreadWithDelay;LoopInGameThreadWithDelay=nil;local standalone=dofile(script);assert(not standalone.Active and standalone.Values.Height==1.1)
LoopInGameThreadWithDelay=function()error('scheduler unavailable')end;local failed=dofile(script);assert(not failed.Active)
LoopInGameThreadWithDelay=loop
io.stdout:write('External editor example: typed local config, no config execution, provider-optional callbacks, revision coalescing, target fencing and scheduler cleanup passed\n')
