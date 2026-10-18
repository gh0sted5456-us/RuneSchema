#include <windows.h>
#include <cstring>
#include <memory>
#include <new>
#include <map>
#include <mutex>
#include <deque>
#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/PluginRuntimeServices.h"
#include "Generator/HelpyStandaloneWindow.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/ItemIconCache.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/HelpyIconTrace.h"
#include "Generator/LazyCatalog.h"
#include "Generator/AuthoringClient.h"
#include "Generator/ToolRequest.h"
#include "Runtime/HelpySettings.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Runtime/HelpyPaths.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/HelpySourceCache.h"
#include "Runtime/HelpyUpdates.h"
#include "Runtime/HelpyDownloads.h"
#include "Runtime/HelpyMapCache.h"

namespace {
HMODULE Module{};
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),RUNESCHEMA_PLUGIN_API_VERSION,
    "RuneSchema.Helpy","RSMK (RuneSchema Modding Kit)",PS::HelpyBranding::Version};
struct State {const RuneSchemaHostApi* Host{};std::unique_ptr<PS::HelpyStandaloneWindow> Window;
    std::atomic<bool> ToolsAvailable=false,ToolsWaiting=false;std::atomic<uint64_t> ToolsGeneration=1;
    std::mutex ToolsMutex;uint64_t ToolsRevision=0; nlohmann::json ToolsResult=nlohmann::json::object(),ToolsCatalog=nlohmann::json::object(),Players=nlohmann::json::array();std::deque<nlohmann::json> IconResults;
    std::map<uint64_t,nlohmann::json> Completed;std::map<uint64_t,std::string> IconDictionary;std::deque<nlohmann::json> QueryResults;uint64_t HostRevision=0;std::chrono::steady_clock::time_point NextHostRead{};
    nlohmann::json MapGroundReply=nlohmann::json::object();
    bool LoggedQuery=false,LoggedRows=false,LoggedIcon=false;};
bool CallCore(State* state,const nlohmann::json& request,nlohmann::json& response) {
    try {
    if(!state||!state->Host)return false;
    const auto input=request.dump();
    const auto action=request.value("Action",std::string{});
    const bool readOnly=action=="Read"||action=="AuthoringSnapshot"||action=="SettingsRead";
    uint32_t size=0;
    std::string output;
    if(readOnly){
        const auto probe=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),nullptr,0,&size);
        if(probe!=RS_PLUGIN_BUFFER_TOO_SMALL||!size||size>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return false;
        output.resize(size);
    }else{
        output.resize(action=="Completed"||action=="SettingsSave"?RUNESCHEMA_PLUGIN_MAX_MESSAGE:4096);
    }
    const auto code=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),output.data(),static_cast<uint32_t>(output.size()),&size);
    if(code!=RS_PLUGIN_OK||!size||size>output.size())return false;
    if(output[size-1]=='\0')--size;
    output.resize(size);
    response=nlohmann::json::parse(output);return true;
    }catch(...){return false;}
}
int32_t RS_PLUGIN_CALL About(void*,const char*,char* response,uint32_t capacity,uint32_t* size) {
    constexpr char value[]=R"({"plugin":"RuneSchema.Helpy","version":"0.8.0.0","ui":"plugin-dll","umg":false})";
    if(!size)return RS_PLUGIN_INVALID_ARGUMENT;*size=static_cast<uint32_t>(sizeof(value));
    if(!response||capacity<sizeof(value))return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(response,value,sizeof(value));return RS_PLUGIN_OK;
}
void StoreReceipt(State* state,const nlohmann::json& result) {
    // Progress retains the mutation slot; only final acknowledgements complete it.
    if(result.value("_InProgress",false))return;
    const auto id=result.value("_RequestId",uint64_t{});if(!id)return;
    nlohmann::json receipt={{"Status",result.value("Status",std::string("Completed."))}};
    for(const auto* key:{"InventorySnapshot","JournalCatalog","GrantResults","SpawnResult","SpawnDefinition","CloneResult","CloneSource","CloneAppearance","ItemDetails","MapSnapshot","MapBuildings","MapPoint","RecipeExportResult","RecipeWorkspaceCatalog","RecipeWorkspaceSource","ItemOverrideExportResult","KitMetadata","NativeContracts"})
        if(result.contains(key))receipt[key]=result[key];
    state->Completed[id]=std::move(receipt);while(state->Completed.size()>16)state->Completed.erase(state->Completed.begin());
}
// Called under ToolsMutex for both push and read recovery. No world-scoped
// roster, role, receipt or icon reply may leak into the next session.
void ResetToolsSession(State* state){
    state->ToolsResult=nlohmann::json::object();state->ToolsCatalog=nlohmann::json::object();state->Players=nlohmann::json::array();
    state->MapGroundReply=nlohmann::json::object();state->IconResults.clear();state->IconDictionary.clear();state->Completed.clear();state->QueryResults.clear();
    state->HostRevision=0;state->LoggedQuery=false;state->LoggedRows=false;state->LoggedIcon=false;
}
int32_t RS_PLUGIN_CALL ToolsPush(void* context,const char* requestJson,char*,uint32_t,uint32_t* size) {
    auto* state=static_cast<State*>(context);if(!state||!requestJson||!size)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        const auto message=nlohmann::json::parse(requestJson);const auto kind=message.value("Kind",std::string{});std::string diagnostic;
        auto payload=message.value("Payload",nlohmann::json::object());const auto generation=message.value("Generation",uint64_t{1});
        {
            std::lock_guard lock(state->ToolsMutex);
            if(generation<state->ToolsGeneration.load()){*size=0;return RS_PLUGIN_OK;}
            if(generation!=state->ToolsGeneration.load())ResetToolsSession(state);
            state->ToolsAvailable=message.value("Available",false);state->ToolsWaiting=message.value("Waiting",false);
            state->ToolsGeneration=generation;if(kind!="icon")++state->ToolsRevision;
            if(kind=="bootstrap")state->ToolsResult=payload.value("Result",nlohmann::json::object());
            else if(kind=="mapground")state->MapGroundReply=std::move(payload);
            else if(kind=="catalog") {
                if(payload.contains("Players")&&payload["Players"].is_array())state->Players=payload["Players"];
                for(auto it=payload.begin();it!=payload.end();++it)state->ToolsCatalog[it.key()]=it.value();
            }
            else if(kind=="query") {
                if(payload.contains("CatalogWindow")&&payload["CatalogWindow"].is_object()) {
                    auto& window=payload["CatalogWindow"];
                    if(window.contains("Dictionary")&&window["Dictionary"].is_object()) {
                        const auto& dict=window["Dictionary"];if(dict.contains("Icons")&&dict["Icons"].is_array())for(const auto& entry:dict["Icons"])
                            if(entry.is_object()&&entry.contains("Id")&&entry["Id"].is_number_unsigned()&&entry.contains("Value")&&entry["Value"].is_string())state->IconDictionary[entry["Id"].get<uint64_t>()]=entry["Value"].get<std::string>();
                    }
                    if(window.contains("Rows")&&window["Rows"].is_array())for(auto& row:window["Rows"])if(row.is_object()) {
                        for(const auto* key:{"Path","Name","Icon","Class","Type","Category","InternalName","PersistenceID","Tags"})if(row.contains(key)&&!row[key].is_string())row.erase(key);
                        if(!row.contains("Icon")&&row.contains("IconId")&&row["IconId"].is_number_unsigned()) {
                            const auto found=state->IconDictionary.find(row["IconId"].get<uint64_t>());if(found!=state->IconDictionary.end())row["Icon"]=found->second;
                        }
                        const auto correlation=row.value("Path",row.value("Name",std::string("<row>")));
                        PS::HelpyIconTrace::Event("plugin-row",correlation,
                            "name='"+row.value("Name",std::string{})+"' icon='"+row.value("Icon",std::string("<none>"))+
                            "' iconId="+(row.contains("IconId")?row["IconId"].dump():"0"));
                    }
                    window.erase("Dictionary");
                    const auto rows=window.contains("Rows")&&window["Rows"].is_array()?window["Rows"].size():0;
                    if(!state->LoggedQuery||(rows&&!state->LoggedRows)){
                        diagnostic="RSMK "+std::string(PS::HelpyBranding::Version)+" catalog received: category="+std::to_string(window.value("Category",-1))+" rows="+std::to_string(rows)+" more="+(window.value("HasMore",false)?"yes":"no");
                        state->LoggedQuery=true;if(rows)state->LoggedRows=true;
                    }
                }
                state->QueryResults.push_back(std::move(payload));while(state->QueryResults.size()>32)state->QueryResults.pop_front();
            }
            else if(kind=="icon"){
                if(!state->LoggedIcon){diagnostic="RSMK "+std::string(PS::HelpyBranding::Version)+" native icon reply received.";state->LoggedIcon=true;}
                state->IconResults.push_back(std::move(payload));while(state->IconResults.size()>16)state->IconResults.pop_front();}
            else if(kind=="result"){
                if(payload.contains("Players")&&payload["Players"].is_array())state->Players=payload["Players"];
                if(payload.contains("PlayersDelta")&&payload["PlayersDelta"].is_object()) {
                    const auto& delta=payload["PlayersDelta"];if(delta.value("Reset",false))state->Players=nlohmann::json::array();
                    if(delta.contains("Removed")&&delta["Removed"].is_array())for(const auto& path:delta["Removed"])if(path.is_string())
                        for(auto it=state->Players.begin();it!=state->Players.end();)it=it->value("Path",std::string{})==path.get<std::string>()?state->Players.erase(it):++it;
                    const auto upsert=[&](const nlohmann::json& row){if(!row.is_object())return;const auto path=row.value("Path",std::string{});if(path.empty())return;for(auto& existing:state->Players)if(existing.value("Path",std::string{})==path){existing=row;return;}state->Players.push_back(row);};
                    if(delta.contains("Added")&&delta["Added"].is_array())for(const auto& row:delta["Added"])upsert(row);
                    if(delta.contains("Updated")&&delta["Updated"].is_array())for(const auto& row:delta["Updated"])upsert(row);
                }
                state->ToolsResult=payload;StoreReceipt(state,payload);
            }
            else if(kind=="clear"){ResetToolsSession(state);state->ToolsResult=payload;}
        }
        if(!diagnostic.empty()&&state->Host&&state->Host->Log)state->Host->Log(RS_LOG_INFO,Descriptor.Id,diagnostic.c_str());
        PS::LazyCatalog::BackendSignal(generation);*size=0;return RS_PLUGIN_OK;
    }catch(...){return RS_PLUGIN_FAILED;}
}
void SyncHost(State* state,bool force=false) {
    const auto now=std::chrono::steady_clock::now();if(!force&&now<state->NextHostRead)return;
    state->NextHostRead=now+std::chrono::seconds(2);
    nlohmann::json reply;
    if(!CallCore(state,{{"Action","Read"},{"Revision",state->HostRevision},{"Category",-1}},reply)||!reply.is_object())return;
    std::lock_guard lock(state->ToolsMutex);
    const auto generation=reply.value("Generation",state->ToolsGeneration.load());
    if(generation<state->ToolsGeneration.load())return;
    // An unchanged read is a heartbeat, not new catalogue data. Avoid a
    // pointless rebuild/sort of every visible row every two seconds.
    if(generation==state->ToolsGeneration.load()&&reply.value("Revision",state->HostRevision)==state->HostRevision
        &&reply.value("Available",false)==state->ToolsAvailable.load()&&reply.value("Waiting",false)==state->ToolsWaiting.load()
        &&!reply.contains("Snapshot"))return;
    if(generation!=state->ToolsGeneration.load()) {
        ResetToolsSession(state);
    }
    state->ToolsGeneration=generation;state->ToolsAvailable=reply.value("Available",false);state->ToolsWaiting=reply.value("Waiting",false);
    state->HostRevision=reply.value("Revision",state->HostRevision);
    if(reply.contains("Snapshot")&&reply["Snapshot"].is_object()){
        const auto& snapshot=reply["Snapshot"];state->ToolsResult=snapshot;
        if(snapshot.contains("Players")&&snapshot["Players"].is_array())state->Players=snapshot["Players"];
        for(auto it=snapshot.begin();it!=snapshot.end();++it)if(it.key()!="Players")state->ToolsCatalog[it.key()]=it.value();
    }
    ++state->ToolsRevision;PS::LazyCatalog::BackendSignal(generation);
}
void ConfigureTransport(State* state) {
        PS::RuneSchemaSettingsPanel::Remote=[state](const nlohmann::json& request,nlohmann::json& result){return CallCore(state,request,result);};
        PS::HelpyIconPacks::RemoteBuild=[state](const nlohmann::json& request,nlohmann::json& result){return CallCore(state,request,result);};
        PS::HelpySourceCache::RemoteSnapshot=[state](nlohmann::json& result){return CallCore(state,{{"Action","IconCacheSources"}},result);};
        PS::ItemIconCache::RemoteResetAccounting=[state]{nlohmann::json ignored;CallCore(state,{{"Action","IconCacheResetAccounting"}},ignored);};
        PS::AuthoringClient::Remote=[state](const nlohmann::json& request,nlohmann::json& reply){return CallCore(state,request,reply);};
        PS::SpawnToolRequests::RemoteMapGroundSubmit=[state](nlohmann::json request){nlohmann::json result;return CallCore(state,{{"Action","MapGroundSubmit"},{"Request",std::move(request)}},result)&&result.value("Accepted",false);};
        PS::SpawnToolRequests::RemoteSearch=[state](std::string query){nlohmann::json ignored;CallCore(state,{{"Action","Search"},{"Query",std::move(query)}},ignored);};
        PS::SpawnToolRequests::RemoteSubmit=[state](nlohmann::json request){
            nlohmann::json result;const bool accepted=CallCore(state,{{"Action","Submit"},{"Request",std::move(request)}},result)&&result.value("Accepted",false);
            if(accepted)state->ToolsWaiting=true;return accepted;
        };
        PS::SpawnToolRequests::RemoteCompleted=[state](uint64_t id,nlohmann::json& receipt){std::lock_guard lock(state->ToolsMutex);const auto found=state->Completed.find(id);if(found==state->Completed.end())return false;receipt=std::move(found->second);state->Completed.erase(found);return true;};
        PS::SpawnToolRequests::RemoteReadQuick=[state](uint64_t& revision,int category,nlohmann::json& snapshot){
            SyncHost(state);std::lock_guard lock(state->ToolsMutex);if(revision==state->ToolsRevision)return false;
            snapshot=state->ToolsResult;snapshot["_Category"]=category;
            if(!state->MapGroundReply.empty())snapshot["MapGround"]=state->MapGroundReply;
            if(!state->QueryResults.empty()) {snapshot["_QueryResults"]=nlohmann::json::array();for(auto& row:state->QueryResults)snapshot["_QueryResults"].push_back(std::move(row));state->QueryResults.clear();}
            const auto copy=[&](const char* key){if(state->ToolsCatalog.contains(key))snapshot[key]=state->ToolsCatalog[key];};
            snapshot["Players"]=state->Players;copy("PlayerRoles");
            if(category==0){copy("Items");copy("CookedVisuals");copy("CatalogIssues");}
            else if(category==1||category==2){if(state->ToolsCatalog.contains("Definitions")&&state->ToolsCatalog["Definitions"].is_array()){
                snapshot["Definitions"]=nlohmann::json::array();for(const auto& row:state->ToolsCatalog["Definitions"]){const auto type=row.value("Type",std::string{});if((category==1&&(type=="AI"||type=="NPC"))||(category==2&&type=="Resource"))snapshot["Definitions"].push_back(row);}}}
            for(const auto* key:{"CatalogReady","_CatalogIndexing","CatalogStatus","RegistryRecords","UnresolvedAssets","MetadataUnavailable","UnclassifiedRecords","CatalogAI","CatalogResources","CatalogCoverage","IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})copy(key);
            revision=state->ToolsRevision;return true;
        };
        PS::SpawnToolRequests::RemoteCancel=[state]{nlohmann::json ignored;CallCore(state,{{"Action","Cancel"}},ignored);};
        PS::SpawnToolRequests::RemoteAvailable=[state]{return state->ToolsAvailable.load();};
        PS::SpawnToolRequests::RemoteWaiting=[state]{return state->ToolsWaiting.load();};
        PS::SpawnToolRequests::RemoteGeneration=[state]{return state->ToolsGeneration.load();};
        PS::LazyCatalog::ConfigureRemote(
            [state](nlohmann::json request){nlohmann::json result;return CallCore(state,{{"Action","QuerySubmit"},{"Request",std::move(request)}},result)&&result.value("Accepted",false);},
            [state](int category,uint64_t cancelGeneration){nlohmann::json ignored;CallCore(state,{{"Action","QueryCancel"},{"Category",category},{"CancelGeneration",cancelGeneration}},ignored);});
        PS::ItemIconCache::ConfigureRemote(
            [state](const std::string& icon){nlohmann::json result;return CallCore(state,{{"Action","IconSubmit"},{"Icon",icon}},result)&&result.value("Accepted",false);},
            [state](nlohmann::json& icon){std::lock_guard lock(state->ToolsMutex);if(state->IconResults.empty())return false;icon=std::move(state->IconResults.front());state->IconResults.pop_front();return true;});
        SyncHost(state,true);
}


}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept {return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance) noexcept {
    if(!host||!instance||host->StructSize<sizeof(RuneSchemaHostApi)||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||
        !host->Log||!host->RegisterCapability||!host->RegisterService||!host->CallService)return RS_PLUGIN_INCOMPATIBLE_API;
    *instance=nullptr;
    auto* state=new(std::nothrow) State{host};if(!state)return RS_PLUGIN_FAILED;
    state->ToolsCatalog=nlohmann::json::object();state->ToolsRevision=1;
    PS::PluginRuntimeServices::Set(host);
    constexpr const char* capabilities[]{"helpy.navigation","helpy.item-lab","helpy.recipe-studio","helpy.map","authoring.live-preview","authoring.export"};
    for(const auto* capability:capabilities)if(host->RegisterCapability(Descriptor.Id,capability)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    if(host->RegisterService(Descriptor.Id,"helpy.about",About,state)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    if(host->RegisterService(Descriptor.Id,"helpy.tools.push",ToolsPush,state)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    const auto initialized="RSMK "+std::string(PS::HelpyBranding::Version)+" native GUI plugin initialized";
    host->Log(RS_LOG_INFO,Descriptor.Id,initialized.c_str());*instance=state;return RS_PLUGIN_OK;
}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUiInit(void*) noexcept {return RS_PLUGIN_OK;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUnrealInit(void* instance) noexcept {
    auto* state=static_cast<State*>(instance);if(!state)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        wchar_t modulePath[32768]{};const auto length=GetModuleFileNameW(Module,modulePath,_countof(modulePath));
        if(!length||length>=_countof(modulePath))throw std::runtime_error("cannot resolve RSMK plugin directory");
        PS::HelpyPaths::Configure(std::filesystem::path(modulePath).parent_path().parent_path());
        PS::HelpySettings::Configure(PS::HelpyPaths::PluginRoot());
        PS::HelpySettings::EnsureLoaded();
        if(!PS::HelpySettings::IsEnabled()){state->Host->Log(RS_LOG_INFO,Descriptor.Id,"disabled by plugin settings");return RS_PLUGIN_OK;}
        state->Host->Log(RS_LOG_INFO,Descriptor.Id,"using RuneSchema host reflection services");
        ConfigureTransport(state);
        nlohmann::json environment;
        if(CallCore(state,{{"Action","Environment"}},environment))PS::HelpyEnvironment::Configure(environment);
        if(PS::HelpyEnvironment::Server()){
            USEROBJECTFLAGS flags{};DWORD bytes=0;
            if(!GetUserObjectInformationW(GetProcessWindowStation(),UOI_FLAGS,&flags,sizeof(flags),&bytes)||!(flags.dwFlags&WSF_VISIBLE)){
                state->Host->Log(RS_LOG_INFO,Descriptor.Id,"server has no interactive desktop; RSMK window skipped, server runtime continues");return RS_PLUGIN_OK;
            }
        }
        state->Window=std::make_unique<PS::HelpyStandaloneWindow>(Module);
        if(!state->Window->Start())state->Host->Log(RS_LOG_WARNING,Descriptor.Id,("standalone UI unavailable: "+state->Window->Error()).c_str());
        else state->Host->Log(RS_LOG_INFO,Descriptor.Id,"standalone RSMK ready: F2 toggle, F5 theme reload");
        state->Host->Log(RS_LOG_INFO,Descriptor.Id,"hotkeys active: F2 desktop RSMK (close to return to game), F5 skin reload");return RS_PLUGIN_OK;
    } catch(const std::exception& error) {state->Host->Log(RS_LOG_ERROR,Descriptor.Id,error.what());return RS_PLUGIN_FAILED;}
      catch(...) {state->Host->Log(RS_LOG_ERROR,Descriptor.Id,"unknown Unreal initialization failure");return RS_PLUGIN_FAILED;}
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* instance) noexcept {
    auto* state=static_cast<State*>(instance);if(!state)return;
    if(state->Window)state->Window->Shutdown();
    PS::HelpyUpdates::Shutdown();
    PS::HelpyDownloads::Shutdown();
    PS::HelpyMapCache::Shutdown();
    PS::RuneSchemaSettingsPanel::Remote={};
    PS::AuthoringClient::Clear();
    PS::HelpySourceCache::Shutdown();
    PS::HelpySourceCache::RemoteSnapshot={};PS::ItemIconCache::RemoteResetAccounting={};
    PS::HelpyIconPacks::RemoteBuild={};PS::HelpyIconPacks::BuildState=nlohmann::json::object();
    PS::ItemIconCache::ClearRemote();PS::LazyCatalog::ClearRemote();PS::SpawnToolRequests::ClearRemote();
    delete state;
    PS::PluginRuntimeServices::Reset();
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {if(reason==DLL_PROCESS_ATTACH)Module=module;return TRUE;}
