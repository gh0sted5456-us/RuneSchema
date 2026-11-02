// Exercise production serialization, push decoding, and query state together.
// The fixture renderer alone cannot catch a lost host/plugin message.
#include "../plugins/helpy/HelpyPlugin.cpp"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/PlayerInventoryPanel.h"
#include "Core/MapProjection.h"
#include "Core/MapSpawnRequest.h"
#include "Core/ServerModVersions.h"
#include "Core/HelpyServerAccess.h"
#include "Runtime/HelpyRuntimeMonitor.h"
#include "Generator/HelpyOverlayInput.h"
#include "Runtime/HelpyCursorRestore.h"
#include <imgui_internal.h>
#include <cassert>
namespace {
using json=nlohmann::json;
json LastQuery,LastSubmission,LastHostRead,HostSnapshot={{"Players",json::array({{{"Path","/Player/Local"},{"Name","Local player"}}})}};
uint64_t HostGeneration=41,HostRevision=8;
std::atomic<bool> ThrowHost=false;
std::mutex HostMutex;std::atomic<int> BlockAction=0;std::atomic<bool> BlockEntered=false;std::shared_future<void> HostRelease;
unsigned SettingsWrites=0;std::map<std::string,unsigned> HostCalls;
unsigned IconManifestReads=0;
unsigned CrashOptionReads=0;
std::map<std::string,unsigned> GuardedCalls;
json SettingsDoc={{"advancedLogging",false},{"persistence",{{"allowCorruptCharacterEntry",false}}},{"future",std::string(10000,'x')}};
PS::HelpyAuthoring::Session AuthoringHost;
int32_t RS_PLUGIN_CALL HostCall(const char*,const char* service,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(ThrowHost)throw std::runtime_error("host read failed");
    assert(std::string(service)=="runeschema.tools");
    const auto request=json::parse(input);json reply;
    const auto action=request.value("Action",std::string{});
    if((BlockAction==1&&action=="QuerySubmit")||(BlockAction==2&&action=="Read")||(BlockAction==3&&action=="IconSubmit")||(BlockAction==4&&action=="Submit")){BlockEntered=true;HostRelease.wait();}
    std::lock_guard lock(HostMutex);++HostCalls[action];
    if(action=="HelpyNativeOverlay"){
        if(!output||capacity<8192){*size=8192;return RS_PLUGIN_BUFFER_TOO_SMALL;}
        ++GuardedCalls[action];reply={{"Ready",true},{"Api","D3D12"}};
    }
    else if(action=="ModsRead"||action=="ModsSave"||action=="ModsImport"||action=="QuickLoadSave"||action=="AdditionalLoggingSave"||action=="CrashDumpSave"){
        if(!output||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
        ++GuardedCalls[action];reply={{"Ok",true},{"Root","D:/XboxGames/Active/RuneSchema"},{"Rows",json::array()},{"Padding",std::string(10000,'x')}};
    }
    else if(request["Action"]=="SettingsRead"||request["Action"]=="SettingsSave"){
        if(request["Action"]=="SettingsSave"){
            ++SettingsWrites;assert(output&&capacity==RUNESCHEMA_PLUGIN_MAX_MESSAGE);
            assert(request["ExpectedContents"]=="fixture-contents");
            for(const auto& [path,value]:request.at("Patch").items())SettingsDoc[json::json_pointer(path)]=value;
        }
        reply={{"Ok",true},{"Document",SettingsDoc},{"Editable",{{"/advancedLogging",false},{"/persistence/allowCorruptCharacterEntry",false}}},
            {"ExpectedContents","fixture-contents"},{"Status","Saved for next launch"}};
    }
    else if(action=="CrashDumpRead"){
        ++CrashOptionReads;reply={{"Ok",true},{"FullMemoryDump",false},{"EnableDumping",true},{"ExpectedContents",std::string(10000,'x')}};
    }
    else if(action=="IconCacheSources"){
        ++IconManifestReads;
        reply={{"Version",4},{"Ready",true},{"Authoritative",true},{"PlanRevision",1},{"ContentRevision",1},
            {"Key","0123456789abcdef"},{"Sources",{{"game","abcdef0123456789"}}},{"Padding",std::string(20000,'x')}};
    }
    else if(request["Action"]=="AuthoringSnapshot")reply=AuthoringHost.Snapshot(request.value("Revision",uint64_t{}));
    else if(request["Action"]=="AuthoringSubmit")reply=AuthoringHost.Submit(request.at("Request"));
    else if(request["Action"]=="AuthoringActivate")reply=AuthoringHost.Activate();
    else if(request["Action"]=="Read"){
        LastHostRead=request;
        reply={{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Revision",HostRevision}};
        if(request.value("Revision",uint64_t{})!=HostRevision){
            reply["Snapshot"]=request.value("Category",-1)==-2?json::object():HostSnapshot;
            if(request.value("Category",-1)==-2)for(const auto* key:{"ServerMods","RemoteAccess"})if(HostSnapshot.contains(key))reply["Snapshot"][key]=HostSnapshot[key];
        }
    }else{
        if(request["Action"]=="QuerySubmit")LastQuery=request.at("Request");
        if(request["Action"]=="Submit")LastSubmission=request.at("Request");
        reply={{"Accepted",action!="QuerySubmit"||!request.at("Request").value("Query",std::string{}).starts_with("reject-")}};
    }
    const auto bytes=reply.dump();*size=static_cast<uint32_t>(bytes.size()+1);
    if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(output,bytes.c_str(),*size);return RS_PLUGIN_OK;
}
}
int main(){
    {PS::HelpyOverlayFrames::Page page;page.PadSequence=2;XINPUT_GAMEPAD pad{};pad.wButtons=XINPUT_GAMEPAD_A;std::memcpy(page.Pad.data(),&pad,sizeof(pad));
        const auto fresh=PS::HelpyOverlayInput::FreshPad(page,1000);assert(fresh&&(fresh->wButtons&XINPUT_GAMEPAD_A));
        assert(!PS::HelpyOverlayInput::FreshPad(page,1251));page.PadSequence=4;std::memset(page.Pad.data(),0,sizeof(pad));
        assert(PS::HelpyOverlayInput::FreshPad(page,1260)->wButtons==0);page.PadSequence=5;assert(!PS::HelpyOverlayInput::FreshPad(page,1261));}
    {
        const auto folder=std::filesystem::temp_directory_path()/("helpy-diagnostic-sink-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(folder/"Mods"/"RuneSchema");
        const auto root=folder/"Mods"/"RuneSchema";const auto original=PS::DiagnosticLogPaths::UseUE4SS;
        PS::DiagnosticLogPaths::UseUE4SS=false;
        assert(PS::HelpyRuntimeMonitor::Write(root,{PS::Diagnostics::Id::PlayTimeReminder,"one\nline"}));
        std::ifstream input(root/"RuneSchema.log");const std::string contents((std::istreambuf_iterator<char>(input)),{});input.close();
        assert(contents.find("[RS-PLAY-001]")!=std::string::npos&&contents.find("one line")!=std::string::npos&&!std::filesystem::exists(root/"UE4SS.log"));
        PS::DiagnosticLogPaths::UseUE4SS=true;assert(PS::HelpyRuntimeMonitor::Write(root,{PS::Diagnostics::Id::ProcessCpuHigh,"sample"}));
        assert(std::filesystem::exists(folder/"RuneSchema.log")&&!std::filesystem::exists(folder/"UE4SS.log"));
        PS::DiagnosticLogPaths::UseUE4SS=original;std::filesystem::remove_all(folder);
    }
    {std::vector<UINT> released;PS::ReleaseHeldGameInput([](UINT key){return key==VK_LBUTTON||key==VK_RBUTTON||key==VK_MBUTTON||key=='W';},[&](UINT message,WPARAM key,LPARAM){released.push_back(message);if(message==WM_KEYUP)assert(key=='W');});
        assert((released==std::vector<UINT>{WM_LBUTTONUP,WM_RBUTTONUP,WM_MBUTTONUP,WM_KEYUP}));
        assert(PS::CapturedGameCall(true,100,100,20)&&!PS::CapturedGameCall(false,100,100,20)&&!PS::CapturedGameCall(true,120,100,20));
        for(const auto count:{0u,1u,64u,UINT(-1)}){
            assert(PS::MaskBufferedGameInput(false,100,100,20,count)==count);
            assert(PS::MaskBufferedGameInput(true,120,100,20,count)==count);
            assert(PS::MaskBufferedGameInput(true,100,100,20,count)==(count==UINT(-1)?count:0));
        }
        for(const auto message:{WM_LBUTTONDBLCLK,WM_RBUTTONDBLCLK,WM_MBUTTONDBLCLK}){PS::HelpyOverlayFrames::Input event;assert(PS::HelpyOverlayFrames::WindowInput(message,0,0,event));assert(event.Kind==PS::HelpyOverlayFrames::InputKind::Button&&event.Code==(message==WM_LBUTTONDBLCLK?WM_LBUTTONDOWN:message==WM_RBUTTONDBLCLK?WM_RBUTTONDOWN:WM_MBUTTONDOWN));}}
    {PS::ItemGridPicker::State selected;char primary[1025]="/Game/Previous.Previous";selected.SelectedPath=primary;selected.SelectedRevision=123;
        for(unsigned i=0;i<64;++i){const std::string path="/Game/Selected"+std::to_string(i)+".Selected";const json row={{"Path",path},{"Name","Selected"}};assert(PS::ItemGridPicker::ApplySelection(selected,path,primary,sizeof(primary),true,true,&row,false));}
        assert(selected.SelectedPaths.size()==64&&selected.SelectedRows.size()==64&&std::string(primary)=="/Game/Previous.Previous"&&selected.SelectedRevision==123);
        for(unsigned i=0;i<64;++i)assert(PS::ItemGridPicker::ApplySelection(selected,"/Game/Selected"+std::to_string(i)+".Selected",primary,sizeof(primary),true,true,nullptr,false));
        assert(selected.SelectedPaths.empty()&&selected.SelectedRows.empty()&&!primary[0]);PS::ItemGridPicker::SyncLegacySelection(selected,primary);assert(selected.SelectedPaths.empty());}
    {POINT point{100,200};assert(!PS::MaskGameHover(false,1100,1000,500,point)&&point.x==100);
        assert(!PS::MaskGameHover(true,2000,1000,500,point)&&point.x==100);
        assert(PS::MaskGameHover(true,1100,1000,500,point)&&point.x==-32000&&point.y==-32000);
        point={100,200};assert(!PS::MaskGameHover(false,1100,1000,500,point)&&point.x==100&&point.y==200);}
    {
        using namespace PS::ServerModVersions;using VersionState=PS::ServerModVersions::State;
        const json remote=json::array({{{"Id","ServerFolder"},{"ModId","Siege"},{"Version","1"}}});
        json local=json::array({{{"Name","RenamedLocalFolder"},{"Present",true},{"Enabled",true},{"Metadata",{{"modId","siege"},{"version","1"}}}}});
        assert(Compare(remote,local,false)[0].Status==VersionState::Unknown);
        assert(Compare(remote,local,true)[0].Status==VersionState::Match);
        local[0]["Metadata"]["version"]="2";assert(Compare(remote,local,true)[0].Status==VersionState::Different);
        local[0]["Metadata"]["version"]="";assert(Compare(remote,local,true)[0].Status==VersionState::Unknown);
        local[0]["Enabled"]=false;assert(Compare(remote,local,true)[0].Status==VersionState::Disabled);
        local.push_back(local[0]);assert(Compare(remote,local,true)[0].Status==VersionState::Ambiguous);
        local.clear();assert(Compare(remote,local,true)[0].Status==VersionState::Missing);
        json host=json::array({{{"Id","siege"},{"ModId","siege"},{"Version","1"}},{{"Id","labels"},{"Version",""}},{{"Id","unlabelled-local"},{"Version","2"}}});
        local=json::array({{{"Name","Renamed"},{"Present",true},{"Enabled",true},{"Metadata",{{"modId","siege"},{"version"," 1 "}}}},{{"Name","labels"},{"Present",true},{"Enabled",true},{"Metadata",{{"version",""}}}},{{"Name","unlabelled-local"},{"Present",true},{"Enabled",true},{"Metadata",{{"version","  "}}}},{{"Name","client-extra"},{"Present",true},{"Enabled",true},{"Metadata",{{"version","9"}}}},{{"Name","client-unversioned"},{"Present",true},{"Enabled",false},{"Metadata",json::object()}},{{"Name","removed-folder"},{"Present",false},{"Enabled",true},{"Metadata",json::object()}}});
        auto report=Build(host,local,true);assert(report.Counts.Matched==1&&report.Counts.Unversioned==2&&report.Counts.ClientOnly==2&&report.Counts.Alignment()==Tone::Aligned);
        assert(report.Host[1].Excluded&&report.Host[2].Excluded&&report.ClientOnly==std::vector<size_t>({3,4}));
        auto text=Text(host,local,report);assert(text.find("Unversioned - excluded")!=text.npos&&text.find("client-extra")!=text.npos&&text.find("removed-folder")==text.npos&&text.find("Renamed")==text.npos);
        local[0]["Metadata"]["version"]="3";report=Build(host,local,true);assert(report.Counts.Different==1&&report.Counts.Alignment()==Tone::Differences);
        local[0]["Metadata"]["version"]="1";local[0]["Enabled"]=false;report=Build(host,local,true);assert(report.Counts.Disabled==1&&report.Counts.Alignment()==Tone::Differences);
        local.erase(local.begin());report=Build(host,local,true);assert(report.Counts.Missing==1&&report.Counts.Unversioned==2&&report.Counts.Alignment()==Tone::Differences);
        host.erase(host.begin());report=Build(host,local,true);assert(report.Counts.Unversioned==2&&report.Counts.Alignment()==Tone::Neutral);
        report=Build(host,local,false);assert(!report.Counts.Ready&&report.Counts.Alignment()==Tone::Neutral&&report.ClientOnly.empty());
        assert(Text(host,local,report).find("not ready")!=std::string::npos);
        host=json::array({{{"Id","siege"},{"ModId","siege"},{"Version","1"}}});local=json::array({{{"Name","one"},{"Present",true},{"Enabled",true},{"Metadata",{{"modId","siege"},{"version","1"}}}},{{"Name","two"},{"Present",true},{"Enabled",true},{"Metadata",{{"modId","siege"},{"version","1"}}}}});
        report=Build(host,local,true);assert(report.Counts.Ambiguous==1&&report.ClientOnly.empty()&&report.Counts.Alignment()==Tone::Differences);
        local[0]["Metadata"]["version"]="";local[1]["Metadata"]["version"]="";report=Build(host,local,true);assert(report.Counts.Unversioned==1&&report.Counts.Ambiguous==0&&report.Counts.Alignment()==Tone::Neutral&&report.Host[0].Status==VersionState::Ambiguous);
    }
    {
        ImGui::CreateContext();PS::HelpyOverlayFrames::Page page;page.Left=20;page.Top=15;
        const auto window=CreateWindowW(L"STATIC",L"Private overlay pointer fixture",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
        PS::HelpyOverlayFrames::Push(page,{PS::HelpyOverlayFrames::InputKind::Mouse,0,1,1});
        PS::HelpyOverlayFrames::Push(page,{PS::HelpyOverlayFrames::InputKind::Button,WM_LBUTTONDOWN,100,80});
        PS::HelpyOverlayInput::Feed(page,window);assert(page.InputRead==page.InputWrite);
        auto* context=ImGui::GetCurrentContext();ImVec2 position{};bool clicked=false;
        for(const auto& event:context->InputEventsQueue){if(event.Type==ImGuiInputEventType_MousePos)position={event.MousePos.PosX,event.MousePos.PosY};if(event.Type==ImGuiInputEventType_MouseButton&&event.MouseButton.Down){assert(position.x==80&&position.y==65);clicked=true;}}
        POINT latest{};assert(GetCursorPos(&latest)&&ScreenToClient(window,&latest));assert(clicked&&position.x==latest.x-20&&position.y==latest.y-15);
        RECT client{};assert(GetClientRect(window,&client));page.ViewportWidth=client.right*2;page.ViewportHeight=client.bottom*2;context->InputEventsQueue.clear();
        PS::HelpyOverlayInput::Feed(page,window);assert(!context->InputEventsQueue.empty());const auto& scaled=context->InputEventsQueue.back();assert(scaled.Type==ImGuiInputEventType_MousePos&&scaled.MousePos.PosX==latest.x*2-20&&scaled.MousePos.PosY==latest.y*2-15);
        DestroyWindow(window);ImGui::DestroyContext();
    }
    {
        auto preferences=PS::HelpyImGuiTheme::Theme{};preferences.runtimeTimeUnit=3;preferences.backgroundFile="Scene.png";preferences.backgroundStrength=.2f;
        preferences.showFps=true;preferences.helpyFpsLimit=45;preferences.fpsRefreshMs=500;
        preferences.showGameFps=true;preferences.fpsHotkey="F11";
        preferences.gameOverlay=true;preferences.overlayOpacity=.77f;preferences.diagnosticRegistryActivity=true;preferences.itemInternalNames=true;
        preferences.playTimeReminders=false;preferences.resourceDiagnostics=false;preferences.gpuResourceDiagnostics=false;preferences.ue4ssDiagnostics=false;
        preferences.startupResourceAlerts=true;
        preferences.controllerShortcut=2;preferences.reserveControllerGuide=false;
        preferences.navigationCollapsed=true;preferences.navigationPinned=true;
        preferences.resourceCpuThreshold=85;preferences.resourceMemoryThreshold=91;preferences.resourceGpuThreshold=99;preferences.ue4ssLogBudget=234;preferences.ue4ssOverlapSeconds=7;
        preferences.diagnosticToastsEnabled=false;preferences.diagnosticToastLimit=2;preferences.diagnosticHistoryDays=7;preferences.diagnosticSplitRatio=.6f;preferences.diagnosticDeveloperMode=true;
        PS::HelpyRuntimePreferences::CacheFirst=false;PS::HelpyRuntimePreferences::PassiveRefresh=false;
        const auto document=PS::HelpyImGuiTheme::DocumentFrom(preferences,200);
        assert(document["navigation"]["collapsed"]==true&&document["navigation"]["pinned"]==true&&!PS::HelpyImGuiTheme::DefaultDocument()["navigation"]["collapsed"].get<bool>());
        assert(document["cache"]["loadOrder"]=="gameFirst"&&document["cache"]["passiveRefresh"]==false);
        assert(PS::HelpyImGuiTheme::DefaultDocument()["cache"]["loadOrder"]=="cacheFirst");
        const auto roundTrip=PS::HelpyBrowsePolicy::Read(document["cache"]);assert(!roundTrip.CacheFirst&&!roundTrip.PassiveRefresh);
        PS::HelpyRuntimePreferences::CacheFirst=true;PS::HelpyRuntimePreferences::PassiveRefresh=true;
        assert(document["runtime"]["timeUnit"]==3&&document["background"]["file"]=="Scene.png"&&document["background"]["strength"].get<float>()==.2f);
        assert(document["runtime"]["showFps"]==true&&document["runtime"]["helpyFpsLimit"]==45&&document["runtime"]["fpsRefreshMs"]==500);
        assert(document["runtime"]["showGameFps"]==true&&document["runtime"]["fpsHotkey"]=="F11");
        assert(document["runtime"]["gameOverlay"]==true&&document["runtime"]["overlayOpacity"].get<float>()==.77f);
        assert(document["views"]["itemInternalNames"]==true&&!PS::HelpyImGuiTheme::DefaultDocument()["views"]["itemInternalNames"].get<bool>());
        assert(document["monitoring"]["resources"]==false&&document["monitoring"]["gpu"]==false&&document["monitoring"]["ue4ss"]==false&&document["monitoring"]["playTimeReminders"]==false);
        assert(document["monitoring"]["startupResourceAlerts"]==true&&!PS::HelpyImGuiTheme::DefaultDocument()["monitoring"]["startupResourceAlerts"].get<bool>());
        assert(document["input"]["shortcut"]==2&&document["input"]["reserveGuide"]==false);
        assert(PS::HelpyImGuiTheme::DefaultDocument()["input"]["shortcut"]==1&&PS::HelpyImGuiTheme::DefaultDocument()["input"]["reserveGuide"]==true);
        assert(document["monitoring"]["cpuThreshold"]==85&&document["monitoring"]["memoryThreshold"]==91&&document["monitoring"]["gpuThreshold"]==99&&document["monitoring"]["luaMessages"]==234&&document["monitoring"]["overlapSeconds"]==7);
        assert(document["notifications"]["registryActivity"]==true);
        const auto defaults=PS::HelpyImGuiTheme::DefaultDocument()["runtime"];
        assert(defaults["showFps"]==false&&defaults["helpyFpsLimit"]==60&&defaults["fpsRefreshMs"]==1000);
        assert(defaults["showGameFps"]==false&&defaults["fpsHotkey"]=="F10");
    assert(defaults["gameOverlay"]==true&&defaults["nativeOverlay"]==true&&!PS::HelpyImGuiTheme::DefaultDocument()["notifications"]["registryActivity"].get<bool>());
        assert(!document["notifications"]["enabled"].get<bool>()&&document["notifications"]["visibleLimit"]==2&&document["diagnosticHistory"]["ageDays"]==7);
        assert(document["diagnosticPresentation"]["splitRatio"].get<float>()==.6f&&document["diagnosticPresentation"]["developerMode"].get<bool>());
        assert(PS::HelpyImGuiTheme::DefaultDocument()["runtime"]["timeUnit"]==0&&PS::HelpyImGuiTheme::DefaultDocument()["background"]["file"]=="");
    }
    PS::HelpyEnvironment::Configure({{"DedicatedServer",true}});
    {
        PS::ItemGridPicker::State picker;picker.Query="helm";picker.SearchInput="helm";picker.ActiveCategory="Armor";
        picker.FavoritesOnly=true;picker.PersonalTag="favorites";picker.SourceProvider="mods/dye";picker.PageIndex=4;
        PS::ItemGridPicker::SelectFamily(picker,"hard_leather_dyed_black_mirror_trim");
        assert(picker.Query.empty()&&picker.SearchInput.empty()&&!picker.FavoritesOnly&&picker.PersonalTag.empty());
        assert(picker.ActiveCategory=="All"&&picker.PageIndex==0&&picker.SourceProvider=="mods/dye"&&picker.ResetScroll);
    }
    RuneSchemaHostApi host{};host.CallService=HostCall;State state{&host};ConfigureTransport(&state);
    const auto syncHost=[&](bool metadata=false){SyncHost(&state,true,metadata);state.NextHostRead=state.NextMetadataRead=std::chrono::steady_clock::time_point::max();};syncHost();
    const auto flushCatalog=[&]{while(state.CatalogSubmit.valid()||!state.CatalogQueue.empty()){if(state.CatalogSubmit.valid())state.CatalogSubmit.wait();PumpCatalog(&state);}};
    for(const auto* action:{"Prepare","Open","Show","Hide","Status"}){
        const auto before=HostCalls["HelpyNativeOverlay"];json reply;
        assert(PS::RuneSchemaSettingsPanel::Remote({{"Action","HelpyNativeOverlay"},{"Payload",{{"Action",action}}}},reply)&&reply.at("Ready")==true&&reply.at("Api")=="D3D12");
        assert(HostCalls["HelpyNativeOverlay"]==before+1&&GuardedCalls["HelpyNativeOverlay"]==before+1);
    }
    {json reply;assert(CallCore(&state,{{"Action","CrashDumpRead"}},reply)&&reply.at("ExpectedContents").get<std::string>().size()==10000&&CrashOptionReads==2);}
    {json manifest;assert(PS::HelpySourceCache::RemoteSnapshot(manifest)&&manifest.at("Padding").get<std::string>().size()==20000&&IconManifestReads==2);
        PS::HelpySourceCache::Current.Provisional=true;PS::HelpySourceCache::CachedSourcesAttempted=true;PS::HelpySourceCache::Next={};
        assert(PS::HelpySourceCache::Poll({})&&PS::HelpySourceCache::Ready()&&!PS::HelpySourceCache::Current.Provisional);
        assert(PS::HelpySourceCache::Fingerprint("/Game/UI/T_Power.T_Power")=="abcdef0123456789");
        PS::HelpySourceCache::Current={};}
    for(const auto* action:{"ModsRead","ModsSave","ModsImport","QuickLoadSave","AdditionalLoggingSave","CrashDumpSave"}){
        json reply;assert(CallCore(&state,{{"Action",action}},reply)&&reply.at("Ok")==true&&reply.at("Root")=="D:/XboxGames/Active/RuneSchema");assert(GuardedCalls[action]==1);
    }
    assert(state.ToolsGeneration==41&&state.ToolsAvailable&&state.Players.size()==1); // No bootstrap push required.
    const auto unchangedRevision=state.ToolsRevision;syncHost();syncHost();
    assert(state.ToolsRevision==unchangedRevision); // Heartbeats do not invalidate row/order caches.
    const auto serverGrant=json{{"Action","GiveItems"},{"Player","/Server/Player/Remote"},{"Items",json::array({{{"Item","/Game/Items/Log.Log"},{"Quantity",1}}})}};
    assert(PS::SpawnToolRequests::TrySubmit(serverGrant)&&LastSubmission==serverGrant);
    StoreReceipt(&state,{{"_RequestId",uint64_t(556)},{"JournalCatalog",{{"Entries",json::array({{{"Path","/Game/Journal/Native.Native"},{"Native",true},{"Pages",json::array({"native text"})}}})},{"HasMore",false}}}});
    json journalReceipt;assert(PS::SpawnToolRequests::TakeCompleted(556,journalReceipt)&&journalReceipt["JournalCatalog"]["Entries"][0]["Pages"][0]=="native text");
    StoreReceipt(&state,{{"_RequestId",uint64_t(557)},{"InventorySnapshot",{{"PlayerGuid","fixture"},{"Items",json::array({{{"Name","Ash Logs"},{"Count",12}}})}}}});
    json inventoryReceipt;assert(PS::SpawnToolRequests::TakeCompleted(557,inventoryReceipt)&&inventoryReceipt["InventorySnapshot"]["Items"][0]["Count"]==12);
    PS::RuneSchemaSettingsPanel::Load();assert(PS::RuneSchemaSettingsPanel::Loaded&&SettingsWrites==0);
    PS::RuneSchemaSettingsPanel::Draft["advancedLogging"]=true;PS::RuneSchemaSettingsPanel::Dirty=true;
    assert(PS::RuneSchemaSettingsPanel::Patch()==json({{"/advancedLogging",true}}));
    PS::RuneSchemaSettingsPanel::Save();assert(SettingsWrites==1&&!PS::RuneSchemaSettingsPanel::Dirty&&SettingsDoc["advancedLogging"]==true);
    PS::RuneSchemaSettingsPanel::Save();assert(SettingsWrites==1); // no duplicate write to fetch a large reply
    using namespace PS::LazyCatalog;
    ResetForGeneration(41);FirstOpen(41);flushCatalog();
    assert(LastQuery["Operation"]=="CatalogWindow"&&LastQuery["Category"]==0&&LastQuery["ClientGeneration"]==41&&LastQuery["Mode"]=="Alphabetical");
    const auto push=[&](json rows,bool more,std::string cursor){
        flushCatalog();const auto request=LastQuery;
        json window={{"RequestId",request["RequestId"]},{"CancelGeneration",request["CancelGeneration"]},{"RuntimeGeneration",HostGeneration},
            {"Category",0},{"Mode",request["Mode"]},{"Reset",request["Reset"]},{"Rows",rows},{"HasMore",more},{"NextCursor",cursor},
            {"Dictionary",{{"Icons",json::array({{{"Id",uint64_t(7)},{"Value","/Game/Icons/Log.Log"}}})}}}};
        const auto message=json({{"Kind","query"},{"Generation",HostGeneration},{"Available",true},{"Payload",{{"CatalogWindow",window}}}}).dump();
        uint32_t size=0;assert(ToolsPush(&state,message.c_str(),nullptr,0,&size)==RS_PLUGIN_OK);
        uint64_t revision=0;json snapshot;assert(PS::SpawnToolRequests::RemoteReadQuick(revision,0,snapshot));
        Apply(snapshot,revision);Overlay(snapshot,0);return snapshot;
    };
    PS::ItemGridPicker::State filterState;filterState.SourceFilter=PS::ItemSource::Kind::Modded;filterState.ActiveCategory="Armor";filterState.FamilyKey="fallen_hoplite_dyed_chrome";
    const auto sourceQuery=PS::ItemGridPicker::BackendQuery(filterState);
    assert(Search(0,sourceQuery));flushCatalog();assert(LastQuery["Query"]=="__helpysource:modded __helpycat:armor __helpyfamily:fallen_hoplite_dyed_chrome");
    auto sourceSnapshot=push(json::array({{{"Path","/Dye/Helm.Helm"},{"Name","Fallen Hoplite's Helm (Dyed Chrome)"},{"Category","Armor"},{"SourceKind","modded"},{"SourceName","Dye"}}}),false,"done");
    PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,1);
    assert(filterState.Matches.size()==1&&PS::ItemSource::Row(sourceSnapshot["Items"][0]).Name=="Dye");
    {
        const auto savedSources=PS::HelpySourceCache::Current;
        PS::HelpySourceCache::Current.DisplayNames={{"dye","Colorful Dye"},{"not-on-this-page","Unloaded Mod"}};
        PS::HelpySourceCache::Current.Containers={{"mods/dye/paks/Colors_P.utoc",{{"Owner","mods/dye"},{"Status","Indexed"}}}};
        const auto choices=PS::ItemGridPicker::ProviderChoices(sourceSnapshot);
        assert(std::find(choices.begin(),choices.end(),std::pair<std::string,std::string>{"mods/dye","Colorful Dye"})!=choices.end());
        assert(std::find(choices.begin(),choices.end(),std::pair<std::string,std::string>{"mods/not-on-this-page","Unloaded Mod"})!=choices.end());
        filterState.SourceProvider="mods/dye";
        assert(PS::ItemCatalogFilter::Query(PS::ItemGridPicker::BackendQuery(filterState))(sourceSnapshot["Items"][0]));
        PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,2);assert(filterState.Matches.size()==1);
        filterState.SourceProvider="mods/other";PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,3);assert(filterState.Matches.empty());
        filterState.SourceProvider.clear();PS::HelpySourceCache::Current=savedSources;
    }
    PS::ItemGridPicker::State orderedState;
    json unordered={{"Items",json::array({{{"Path","/Game/Z"},{"Name","Zinc"}},{{"Path","/Mod/B"},{"Name","apple"}},{{"Path","/Game/A"},{"Name","Apple"}},{{"Path","/Game/B"},{"Name","Bronze"}}})}};
    PS::ItemGridPicker::Rebuild(unordered,orderedState,2);
    assert((orderedState.Matches==std::vector<size_t>{2,1,3,0}));
    assert(Search(0,"test"));
    // Real host replies can be empty while still scanning toward ItemData.
    for(int i=0;i<8;++i){push(json::array(),true,"R:18:"+std::to_string((i+1)*2048));assert(For(0).Pending&&For(0).LastError.empty());}
    auto snapshot=push(json::array({{{"Path","/Game/Items/Log.Log"},{"Name","Ash logs"},{"Icon",nullptr},{"IconId",uint64_t(7)}}}),false,"done");
    assert(snapshot["Items"].size()==1&&snapshot["Items"][0]["Icon"]=="/Game/Icons/Log.Log");
    assert(snapshot["Players"].size()==1&&!For(0).Pending);
    // Replaying a snapshot must not duplicate a window.
    Apply(snapshot,999);assert(For(0).Rows.size()==1);
    assert(Retry(0));CheckTimeouts(For(0).RequestedAt+std::chrono::seconds(16));assert(!For(0).Pending&&!For(0).LastError.empty());
    assert(Retry(0));snapshot=push(json::array({{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}}}),false,"done");
    assert(snapshot["Items"][0]["Name"]=="Stone");
    // A missed world-change push is recovered by the read-only state read.
    flushCatalog();HostGeneration=42;++HostRevision;HostSnapshot["Players"]=json::array();syncHost();
    assert(state.ToolsGeneration==42&&state.Players.empty());ResetForGeneration(42);FirstOpen(42);flushCatalog();assert(LastQuery["ClientGeneration"]==42);
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    PS::SpawnToolsPanel::DefinitionPending=777;
    state.Completed[777]={{"SpawnDefinition",{{"Key","npc:test"},{"Name","Test NPC"}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    assert(PS::SpawnToolsPanel::InspectedDefinitions.at("npc:test")["Name"]=="Test NPC");
    PS::SpawnToolsPanel::DefinitionPending=778;state.Completed[778]={{"SpawnDefinition",{{"Key",nullptr}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);assert(!PS::SpawnToolsPanel::DefinitionStatus.empty());
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");const auto detailId=PS::ItemDetails::Pending;flushCatalog();
    assert(detailId&&PS::ItemDetails::PendingPath=="/Game/Items/Stone.Stone");
    assert(LastSubmission["Action"]=="InspectClone"&&LastSubmission["InspectDetails"]==true&&!LastSubmission.contains("Player"));
    // Read-only item details travel over the same production request/receipt path.
    state.ToolsWaiting=false;
    const json nativeDetails={{"Source","/Game/Items/Stone.Stone"},{"Name","Stone"},{"Icon","/Custom/UI/ClonedHelm.ClonedHelm"},{"PowerLevel",4},
        {"Fields",json::array({{{"NativeName","Description"},{"Value","Native description"},{"HasValue",true}},{{"NativeName","BaseDamage"},{"Value",12},{"HasValue",true}}})}};
    const json detailMessage={{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",detailId},{"ItemDetails",nativeDetails}}}};
    uint32_t responseSize=0;const auto detailWire=detailMessage.dump();assert(ToolsPush(&state,detailWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    PS::ItemDetails::Poll();const auto* details=PS::ItemDetails::Find("/Game/Items/Stone.Stone");assert(details&&details->Error.empty());
    assert(PS::ItemPresentation::Description(details->Native)=="Native description");
    assert(PS::ItemPresentation::Facts(details->Native)[0].Value=="12");
    const json staleIcon={{"Path","/Game/Items/Stone.Stone"},{"Icon","/Game/UI/Source.Source"}};
    assert(PS::ItemGridPicker::IconForRow(staleIcon)=="/Custom/UI/ClonedHelm.ClonedHelm");
    PS::ItemGridPicker::ClearSharedCart();assert(PS::ItemGridPicker::AddToCart(staleIcon));
    assert(PS::ItemGridPicker::SharedCart.Entries()[0].Row["Icon"]=="/Custom/UI/ClonedHelm.ClonedHelm");PS::ItemGridPicker::ClearSharedCart();
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");assert(!PS::ItemDetails::Pending); // Cached hover never resubmits.
    PS::ItemGridPicker::PinDetails(staleIcon);PS::ItemGridPicker::DetachedBay=true;
    assert(std::string(PS::ItemGridPicker::Bay.Selected)=="/Game/Items/Stone.Stone"&&PS::ItemGridPicker::Bay.DetailsExpanded);
    assert(PS::ItemPresentation::Description(PS::ItemDetails::Find(PS::ItemGridPicker::Bay.Selected)->Native)=="Native description");
    PS::ItemGridPicker::DetachedBay=false;assert(PS::ItemGridPicker::Bay.DetailsRow==staleIcon);
    flushCatalog();HostGeneration=43;syncHost();PS::ItemDetails::Poll();assert(!PS::ItemDetails::Find("/Game/Items/Stone.Stone"));
    // Same cart, two authoritative recipients: one serialized request at a
    // time, immutable target snapshot, and clear only after BOTH confirmations.
    PS::SpawnToolsPanel::PollCartDelivery();state.ToolsWaiting=false;
    PS::SpawnToolsPanel::UiSnapshot["Players"]=json::array({{{"Path","/Game/Players/A.A"},{"Name","Alice"}},{{"Path","/Game/Players/B.B"},{"Name","Bob"}}});
    const auto batchItem=json{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}};
    assert(PS::ItemGridPicker::AddToCart(batchItem));
    PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto batchFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(batchFirst));
    assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(batchFirst));assert(LastSubmission["Player"]=="/Game/Players/A.A");
    const auto grant=json{{"GrantResults",json::array({{{"Item","/Game/Items/Stone.Stone"},{"Count",1},{"State","Confirmed"}}})}};
    state.Completed[batchFirst]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().size()==1&&PS::ItemGridPicker::SharedCart.Delivery());
    assert(LastSubmission["Player"]=="/Game/Players/B.B");const auto batchSecond=PS::SpawnToolsPanel::CartBatch.PendingId();assert(batchSecond&&batchSecond!=batchFirst);
    state.Completed[batchSecond]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().empty()&&!PS::ItemGridPicker::SharedCart.Delivery()&&!PS::SpawnToolsPanel::CartNeedsReview);
    assert(PS::ItemGridPicker::AddToCart(batchItem));PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto failedFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(failedFirst));assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(failedFirst));
    state.Completed[failedFirst]={{"Status","Request unconfirmed"}};state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(LastSubmission["Player"]=="/Game/Players/A.A"&&PS::SpawnToolsPanel::CartNeedsReview&&PS::ItemGridPicker::SharedCart.Entries().size()==1);
    assert(!PS::ItemGridPicker::SharedCart.Delivery());PS::SpawnToolsPanel::PollCartDelivery();assert(LastSubmission["Player"]=="/Game/Players/A.A");
    PS::SpawnToolsPanel::CartNeedsReview=false;PS::ItemGridPicker::ClearSharedCart();
    // Map coordinates and artwork metadata survive production JSON transport.
    PS::MapProjection::Projection projection{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};
    constexpr uint64_t mapId=700;
    assert(PS::SpawnToolRequests::TrySubmit({{"Action","MapSnapshot"},{"_RequestId",mapId}}));
    assert(LastSubmission["Action"]=="MapSnapshot");
    json map={{"BridgeVersion",1},{"Projection",projection.ToJson()},{"Image",{{"File","native-map.rsicon"},{"Status","Ready"}}},
        {"Players",json::array({{{"Name","Local player"},{"Location",{198000,91450,100}}}})}};
    const auto mapWire=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",mapId},{"MapSnapshot",map}}}}).dump();
    assert(ToolsPush(&state,mapWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);json mapReceipt;
    assert(PS::SpawnToolRequests::TakeCompleted(mapId,mapReceipt)&&mapReceipt["MapSnapshot"]==map);
    // Stage updates cannot masquerade as arrival or free the UI mutation slot.
    const auto travelProgress=json({{"Kind","result"},{"Available",true},{"Waiting",true},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",true},{"Status","Loading destination"},
            {"MapTravel",{{"Phase","Preparing"},{"ElapsedSeconds",3},{"Committed",false}}}}}}).dump();
    assert(ToolsPush(&state,travelProgress.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(PS::SpawnToolRequests::IsWaiting()&&!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    uint64_t travelRevision=0;json travelSnapshot;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot["MapTravel"]["Phase"]=="Preparing"&&travelSnapshot.value("Players",json::array())==state.Players);
    const auto arrived=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",false},{"Status","Arrived"}}}}).dump();
    assert(ToolsPush(&state,arrived.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(!PS::SpawnToolRequests::IsWaiting()&&PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    assert(!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    // Height replies survive the production decoder without altering action receipts.
    const auto heightWire=json({{"Kind","mapground"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"RequestId",uint64_t(703)},{"Generation",HostGeneration},{"GroundAvailable",true},{"Location",{198000,91450,1234}}}}}).dump();
    assert(ToolsPush(&state,heightWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    travelRevision=0;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot.at("MapGround").at("Location")[2]==1234&&!PS::SpawnToolRequests::TakeCompleted(703,mapReceipt));
    const auto placement=PS::MapSpawnRequest::Build(projection,projection.ToWorld({0.25,0.75}),{{"Key","tree"},{"Class","/Game/Tree.Tree_C"},{"Type","Resource"}},"player",true,1,0,701);
    assert(PS::SpawnToolRequests::TrySubmit(placement)&&LastSubmission==placement);
    AuthoringHost.Configure(true,true);assert(PS::AuthoringClient::Activate());AuthoringHost.TickActivation();PS::AuthoringClient::Poll(true);
    assert(PS::AuthoringClient::Submit("Catalogue"));auto authoring=AuthoringHost.Take();assert(authoring);
    AuthoringHost.Complete((*authoring)["RequestId"],{{"Large",std::string(300000,'z')}},"Large snapshot");
    PS::AuthoringClient::Poll(true);assert(PS::AuthoringClient::Result("Catalogue")["Large"].get<std::string>().size()==300000);
    PS::AuthoringClient::Clear();
    // Production push/read route retains authoritative display roles, including
    // an empty roster. No role update is a mutation receipt or a client grant.
    const auto roleRow=json{{"Path","/Player/Admin"},{"Guid",std::string(32,'A')},{"Name","Same"},
        {"RoleStatus",PS::PlayerStatus::Describe(1,{1,2,4,8,128},false)}};
    const auto pushRoles=[&](const json& rows){const auto wire=json{{"Kind","catalog"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"Players",rows},{"PlayerRoles",PS::PlayerStatus::Snapshot(rows,HostGeneration)}}}}.dump();
        assert(ToolsPush(&state,wire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);PS::SpawnToolsPanel::RefreshSnapshot(-1);};
    pushRoles(json::array({roleRow}));assert(PS::PlayerBadges::Find(roleRow)->at("Role")=="admin");
    pushRoles(json::array());assert(!PS::PlayerBadges::Find(roleRow));assert(PS::SpawnToolsPanel::UiSnapshot["Players"].empty());
    const auto serverMods=json::array({{{"Id","Siege"},{"Name","Siege Equipment"},{"Version","0.4.26"}}});
    auto modPush=json{{"Kind","catalog"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"ServerMods",serverMods}}}}.dump();
    assert(ToolsPush(&state,modPush.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    state.QueryResults.push_back({{"MetadataMustNotConsume",true}});const auto pendingQueries=state.QueryResults.size();
    uint64_t modRevision=~uint64_t{};json modView;assert(PS::SpawnToolRequests::ReadQuickIfChanged(modRevision,-2,modView)&&modView["ServerMods"]==serverMods);
    const json challenge={{"Available",true},{"PinRequired",true},{"Allowed",false},{"CharacterGuid","my-character"}};
    const auto accessPush=json{{"Kind","catalog"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"RemoteAccess",challenge}}}}.dump();
    assert(ToolsPush(&state,accessPush.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(PS::SpawnToolRequests::ReadQuickIfChanged(modRevision,-2,modView)&&modView["RemoteAccess"]==challenge);
    PS::HelpyServerAccess::Prompt accessPrompt;
    assert(accessPrompt.Observe(HostGeneration,modView["RemoteAccess"])&&!accessPrompt.Observe(HostGeneration,modView["RemoteAccess"]));
    auto granted=challenge;granted["Allowed"]=true;granted["PinRequired"]=false;
    assert(!accessPrompt.Observe(HostGeneration,granted)&&!accessPrompt.Observe(HostGeneration,challenge));
    assert(accessPrompt.Observe(HostGeneration+1,challenge));auto another=challenge;another["CharacterGuid"]="another-character";
    assert(accessPrompt.Observe(HostGeneration+1,another));another["Available"]=false;assert(!accessPrompt.Observe(HostGeneration+2,another));
    {
        const auto oldSnapshot=HostSnapshot,oldResult=state.ToolsResult;const auto oldHostRevision=state.HostRevision;
        HostSnapshot["RemoteAccess"]=challenge;HostSnapshot["Items"]=json::array({{{"Name",std::string(20000,'x')}}});++HostRevision;
        syncHost(true);assert(LastHostRead["Category"]==-2&&LastHostRead["Revision"]==0&&state.HostRevision==oldHostRevision&&state.ToolsResult==oldResult);
        const auto calls=HostCalls["Read"];SyncHost(&state,false,true);assert(HostCalls["Read"]==calls);HostSnapshot=oldSnapshot;
    }
    assert(state.QueryResults.size()==pendingQueries&&!modView.contains("_QueryResults")&&!modView.contains("Items"));state.QueryResults.pop_back();
    // An empty dedicated roster still permits metadata-only AI/resource queries.
    assert(Search(1,"goblin"));flushCatalog();assert(LastQuery["Category"]==1);
    assert(Search(2,"silver"));flushCatalog();assert(LastQuery["Category"]==2);
    // Recipient editing never touches item quantities, including the legacy
    // single-player fallback. Clearing is explicit, not a hidden re-selection.
    namespace Cart=PS::SpawnToolsPanel;
    const auto roster=json::array({{{"Path","/Player/A"},{"Name","Alice"}},{{"Path","/Player/B"},{"Name","Bob"}}});
    pushRoles(roster);assert(PS::ItemGridPicker::AddToCart(batchItem));
    PS::ItemGridPicker::SharedCart.SetQuantity("/Game/Items/Stone.Stone",7);
    const auto staged=PS::ItemGridPicker::SharedCart.Payload();
    Cart::SelectedPlayer="/Player/A";Cart::CartRecipientsChosen=false;assert(Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::RemoveCartRecipient("/Player/A")&&Cart::EffectiveCartRecipients().empty());
    assert(Cart::CartRecipientsChosen&&Cart::SelectedPlayer.empty()&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    Cart::CartRecipients={{"/Player/A","Alice"},{"/Player/B","Bob"}};Cart::SelectedPlayer="/Player/B";
    assert(Cart::RemoveCartRecipient("/Player/B")&&Cart::EffectiveCartRecipients().size()==1&&Cart::SelectedPlayer=="/Player/A");
    assert(Cart::ClearCartRecipients()&&Cart::EffectiveCartRecipients().empty()&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    assert(!Cart::ClearCartRecipients());
    Cart::CartRecipients={{"/Player/A","Alice"},{"/Player/B","Bob"}};Cart::SelectedPlayer="/Player/A";
    // Departure removes stale identities; matching display names do not retarget.
    pushRoles(json::array({{{"Path","/Player/NewA"},{"Name","Alice"}},{{"Path","/Player/B"},{"Name","Renamed Bob"}}}));
    assert(Cart::SelectedPlayer.empty()&&Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::EffectiveCartRecipients()[0].Path=="/Player/B"&&Cart::EffectiveCartRecipients()[0].Name=="Renamed Bob");
    // A disconnected next recipient is never submitted from an immutable batch.
    Cart::CartBatch.Begin(staged,{{"/Player/B","Bob"},{"/Player/A","Alice"}});
    const auto disconnected=++Cart::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(disconnected));state.ToolsWaiting=false;
    assert(Cart::SubmitNextCartRecipient(disconnected));assert(!Cart::ClearCartRecipients()&&!Cart::RemoveCartRecipient("/Player/B"));
    auto stagedReceipt=staged;for(auto& item:stagedReceipt)item["State"]="Confirmed";
    state.Completed[disconnected]={{"GrantResults",stagedReceipt}};state.ToolsWaiting=false;Cart::PollCartDelivery();
    assert(LastSubmission["Player"]=="/Player/B"&&!Cart::CartBatch.Pending()&&Cart::CartNeedsReview);
    assert(PS::ItemGridPicker::SharedCart.Payload()==staged&&!PS::ItemGridPicker::SharedCart.Delivery());
    Cart::CartNeedsReview=false;
    // Server -> menu -> single player, even when the clear push was missed.
    // Generation changes cancel UI batches, keep items, and reject old receipts.
    Cart::CartBatch.Begin(staged,{{"/Player/B","Bob"}});const auto interrupted=++Cart::CartSequence;
    assert(PS::ItemGridPicker::SharedCart.BeginDelivery(interrupted));state.ToolsWaiting=false;assert(Cart::SubmitNextCartRecipient(interrupted));
    HostGeneration=44;++HostRevision;HostSnapshot={{"Status","Menu"}};syncHost();Cart::RefreshSnapshot(-1);Cart::PollCartDelivery();
    assert(state.Players.empty()&&Cart::SelectedPlayer.empty()&&Cart::CartRecipients.empty()&&Cart::EffectiveCartRecipients().empty());
    assert(!PS::ItemGridPicker::SharedCart.Delivery()&&Cart::CartBatch.Receipts().empty()&&Cart::CartNeedsReview);
    assert(PS::ItemGridPicker::SharedCart.Payload()==staged);
    // A higher-generation result without a roster must not inherit any players.
    HostGeneration=45;++HostRevision;HostSnapshot={{"Status","Single player"}};
    const auto newWorld=json{{"Kind","result"},{"Generation",HostGeneration},{"Available",true},{"Waiting",false},{"Payload",{{"Status","New world"}}}}.dump();
    assert(ToolsPush(&state,newWorld.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(state.Players.empty()&&state.Completed.empty()&&state.ToolsCatalog.empty());
    for(const auto* kind:{"catalog","result","clear","query","icon","bootstrap"}){
        const auto stale=json{{"Kind",kind},{"Generation",uint64_t(43)},{"Available",false},{"Waiting",true},
            {"Payload",{{"Players",roster},{"_RequestId",interrupted},{"GrantResults",stagedReceipt}}}}.dump();
        assert(ToolsPush(&state,stale.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    }
    assert(state.ToolsGeneration==45&&state.ToolsAvailable&&!state.ToolsWaiting&&state.Players.empty()&&state.Completed.empty()&&state.QueryResults.empty()&&state.IconResults.empty());
    Cart::RefreshSnapshot(-1);assert(Cart::EffectiveCartRecipients().empty()&&Cart::SelectedPlayer.empty());
    const auto single=json::array({{{"Path","/Player/Single"},{"Name","Alice"}}});HostSnapshot["Players"]=single;++HostRevision;pushRoles(single);
    assert(Cart::EffectiveCartRecipients().empty()); // Never auto-deliver to the new world.
    Cart::SelectedPlayer="/Player/Single";assert(Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::EffectiveCartRecipients()[0].Path=="/Player/Single"&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    json lateReceipt;assert(!PS::SpawnToolRequests::TakeCompleted(interrupted,lateReceipt));
    Cart::CartNeedsReview=false;assert(Cart::ClearCartRecipients());PS::ItemGridPicker::ClearSharedCart();
    // A busy read/validation does not require another click or submit a grant twice.
    Cart::SelectedPlayer="/Player/Single";Cart::CartRecipientsChosen=false;Cart::CartNeedsReview=false;
    assert(PS::ItemGridPicker::SharedCart.Add({{"Path","/Game/Items/Queued.Queued"}}));
    Cart::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),Cart::EffectiveCartRecipients());
    const auto queued=++Cart::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(queued));
    LastSubmission=json::object();state.ToolsWaiting=true;Cart::PollCartDelivery();Cart::PollCartDelivery();assert(LastSubmission.empty());
    state.ToolsWaiting=false;Cart::PollCartDelivery();const auto once=LastSubmission;
    assert(once["Action"]=="GiveItems"&&once["Player"]=="/Player/Single"&&Cart::CartBatch.Pending());
    Cart::PollCartDelivery();assert(LastSubmission==once);Cart::CartBatch.Stop();Cart::PollCartDelivery();PS::ItemGridPicker::ClearSharedCart();Cart::CartNeedsReview=false;
    {json reply;const auto calls=HostCalls["Read"];assert(CallCore(&state,{{"Action","Read"},{"Revision",HostRevision}},reply));assert(HostCalls["Read"]==calls+1);}
    ThrowHost=true;json ignored;assert(!CallCore(&state,{{"Action","Read"}},ignored));
    {namespace A=PS::AuthoringClient;A::Clear();bool busy=true;
        A::Remote=[&](const json&,json& reply){reply={{"Generation",77},{"Revision",busy?1:2},{"RequestId",uint64_t(778)},{"Operation","SaveEditorSlotItems"},{"Busy",busy},{"Result",{{"Rows",json::array({{{"Name","Cape"}}})}}}};return true;};
        A::Poll(true);assert(A::LastReceipt==0&&A::Result("SaveEditorSlotItems").empty());busy=false;A::Poll(true);assert(A::LastReceipt==778&&A::Result("SaveEditorSlotItems")["Rows"].size()==1);A::Clear();}
    {namespace A=PS::AuthoringClient;namespace P=PS::PlayerInventoryPanel;uint64_t requestId=0;bool complete=false;
        A::Snapshot={{"Generation",uint64_t(81)},{"Active",true},{"Enabled",true},{"Ready",true},{"Busy",false}};
        A::Remote=[&](const json& request,json& reply){if(request["Action"]=="AuthoringSubmit"){assert(request["Request"]["Operation"]=="SaveEditorSlotItems"&&request["Request"]["Arguments"]["Slot"]=="Cape"&&request["Request"]["Arguments"]["CacheFirst"]==true);requestId=request["Request"]["RequestId"].get<uint64_t>();reply={{"Accepted",true}};return true;}
            reply={{"Generation",uint64_t(81)},{"Revision",uint64_t(complete?3:2)},{"Active",true},{"Enabled",true},{"Ready",true},{"Busy",requestId&&!complete},{"RequestId",requestId},{"Operation","SaveEditorSlotItems"},{"Result",complete?json{{"Slot","Cape"},{"Rows",json::array({{{"Name","Cape"},{"NativeSlotVerified",true},{"SlotStrategy","Cape"}}})},{"NextOffset",1},{"HasMore",false}}:json::object()}};return true;};
        P::Open=true;P::Generation=PS::SpawnToolRequests::CurrentGeneration();P::ChangingGear=true;P::GearLocation="Cape";P::GearRows=json::array();P::GearCatalogRequest=0;P::GearMore=true;P::GearOffset=0;P::GearPending=0;P::Pending=0;P::Refresh=false;P::Bay.Query.clear();P::GearQuery.clear();
        P::Poll();assert(requestId&&P::GearCatalogRequest==requestId&&P::GearRows.empty());A::Poll(true);assert(P::GearCatalogRequest&&A::LastReceipt==0);
        complete=true;A::Poll(true);P::Poll();assert(P::GearRows.size()==1&&!P::GearMore&&P::GearCatalogRequest==0);P::Open=false;P::ChangingGear=false;A::Clear();}
    {namespace P=PS::PlayerInventoryPanel;auto& cache=PS::LazyCatalog::For(0);const auto saved=cache;
        cache={};cache.RuntimeGeneration=PS::SpawnToolRequests::CurrentGeneration();cache.Query="unrelated-global-filter";
        const json cape={{"Name","Native cape"},{"Path","/Game/Cape.Cape"},{"SlotStrategy","Cape"},{"NativeSlotVerified",true}};
        auto head=cape;head["Path"]="/Game/Head.Head";head["SlotStrategy"]="Head";
        auto unverified=cape;unverified["Path"]="/Game/Unknown.Unknown";unverified.erase("NativeSlotVerified");
        cache.Rows=json::array({cape,head,unverified});cache.Cache["duplicate"]={json::array({cape}),"",false,cache.RuntimeGeneration};
        auto stale=cape;stale["Path"]="/Game/Stale.Stale";cache.Cache["stale"]={json::array({stale}),"",false,cache.RuntimeGeneration+1};
        P::ChangeGear({{"Name","Player"}},{{"Location","Cape"}});
        assert(P::GearRows==json::array({cape})&&cache.Query=="unrelated-global-filter"&&P::Bay.ActiveCategory=="All");
        P::Open=false;P::ChangingGear=false;P::Refresh=false;cache=saved;}
    {namespace P=PS::PlayerInventoryPanel;
        ThrowHost=false;state.ToolsAvailable=true;state.ToolsWaiting=false;P::Open=false;P::Detached=false;P::Refresh=false;P::Target={{"Name","Keep inspector target"}};
        const json player={{"Path","/Player/Repair"},{"Guid","0123456789abcdef0123456789abcdef"}},item={{"Path","/Game/Gear.Gear"},{"Instance",{1,2,3,4}},{"Slot",2}};
        P::RestoreGear(player,item);const auto repair=P::RepairPending;
        assert(repair&&LastSubmission["Action"]=="RestorePlayerGear"&&LastSubmission["Player"]==player["Path"]&&LastSubmission["PlayerGuid"]==player["Guid"]&&LastSubmission["Instance"]==item["Instance"]&&LastSubmission["Slot"]==2);
        assert(!P::Open&&!P::Detached&&!P::Refresh&&P::Target["Name"]=="Keep inspector target");
        P::RestoreGear(player,item);assert(P::RepairPending==repair&&LastSubmission["_RequestId"]==repair);
        state.Completed[repair]={{"EquipmentAction",{{"Confirmed",true}}}};state.ToolsWaiting=false;P::Poll();assert(!P::RepairPending&&P::RepairStatus=="Durability restored."&&!P::Open);
        P::RestoreGear(player,item);assert(P::RepairPending);P::RepairStarted-=std::chrono::seconds(16);P::Poll();assert(!P::RepairPending&&P::RepairStatus.find("unconfirmed")!=std::string::npos&&!P::Open);
        state.ToolsWaiting=false;P::RestoreGear(player,item);assert(P::RepairPending);P::RepairGeneration=PS::SpawnToolRequests::CurrentGeneration()+1;P::Poll();assert(!P::RepairPending&&P::RepairStatus.find("World changed")!=std::string::npos&&!P::Open);state.ToolsWaiting=false;
    }
    {uint32_t bytes=0;const auto revision=state.ToolsRevision;state.RelayOnly=true;assert(ToolsPush(&state,"not parsed by the game-side relay",nullptr,0,&bytes)==RS_PLUGIN_OK&&state.ToolsRevision==revision);state.RelayOnly=false;assert(ToolsPush(&state,"not JSON",nullptr,0,&bytes)==RS_PLUGIN_FAILED);}
    flushCatalog();if(state.HostRead.valid())state.HostRead.wait();ThrowHost=false;
    {
        std::promise<void> release;HostRelease=release.get_future().share();BlockEntered=false;BlockAction=1;
        auto search=std::async(std::launch::async,[]{return Search(0,"reject-stale");});
        const bool responsive=search.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
        if(!responsive)release.set_value();const bool accepted=search.get();
        if(responsive){
            for(int i=0;i<32;++i)assert(Search(0,"latest-"+std::to_string(i)));
            const auto queued=state.CatalogQueue.size();release.set_value();assert(queued<=1);
        }
        flushCatalog();BlockAction=0;DrainCancellations();assert(responsive&&accepted&&BlockEntered);
        assert(LastQuery["Query"]=="latest-31"&&For(0).LastError.empty()&&For(0).Pending);
        assert(Search(0,"reject-current"));flushCatalog();assert(!For(0).Pending&&!For(0).LastError.empty());
    }
    {
        std::promise<void> release;HostRelease=release.get_future().share();BlockEntered=false;BlockAction=2;
        state.NextHostRead={};const auto lastPlayers=state.Players;
        auto refresh=std::async(std::launch::async,[&]{SyncHost(&state);});
        const bool responsive=refresh.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
        if(!responsive)release.set_value();refresh.get();
        if(responsive){uint32_t size=0;const auto message=json{{"Kind","bootstrap"},{"Generation",HostGeneration},{"Available",true},{"Payload",{{"Result",{{"Status","newer push"}}}}}}.dump();
            assert(ToolsPush(&state,message.c_str(),nullptr,0,&size)==RS_PLUGIN_OK);release.set_value();}
        if(state.HostRead.valid())state.HostRead.wait();SyncHost(&state);BlockAction=0;
        assert(responsive&&BlockEntered&&state.ToolsResult["Status"]=="newer push"&&state.Players==lastPlayers);
    }
    {
        std::promise<void> release;HostRelease=release.get_future().share();BlockEntered=false;BlockAction=3;
        auto icon=std::async(std::launch::async,[]{return PS::ItemIconCache::RemoteSubmit("/Game/UI/Async.Async");});
        const bool responsive=icon.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
        release.set_value();assert(icon.get()&&responsive);if(state.IconSubmit.valid())state.IconSubmit.wait();PumpIcons(&state);BlockAction=0;assert(BlockEntered);
    }
    {
        std::promise<void> release;HostRelease=release.get_future().share();BlockEntered=false;BlockAction=4;
        const json inspect={{"Action","InspectClone"},{"InspectDetails",true},{"Source","/Game/Items/Stone.Stone"},{"_Source","HelpyHover"},{"_RequestId",uint64_t(99001)}};
        assert(ItemInspection(inspect)&&!ItemInspection(serverGrant));
        auto details=std::async(std::launch::async,[&]{return PS::SpawnToolRequests::RemoteSubmit(inspect);});
        const bool responsive=details.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
        release.set_value();assert(details.get()&&responsive);flushCatalog();BlockAction=0;assert(BlockEntered&&LastSubmission==inspect);
    }
    PS::ItemIconCache::ClearRemote();ClearRemote();PS::SpawnToolRequests::ClearRemote();
    PS::RuneSchemaSettingsPanel::Remote={};
}
