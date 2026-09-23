#include <windows.h>
#include <cstring>
#include <memory>
#include <new>
#include <map>
#include <mutex>
#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/PluginRuntimeServices.h"
#include "Generator/InGameQuickMenu.h"
#include "Generator/HelpyHotkeys.h"
#include "Generator/F2BundledPaths.h"
#include "Generator/QuickMenuDecorations.h"
#include "Generator/ToolRequest.h"
#include "Runtime/HelpySettings.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/StaticClassStorage.h"
#include "Unreal/Hooks.hpp"

namespace {
HMODULE Module{};
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),RUNESCHEMA_PLUGIN_API_VERSION,
    "RuneSchema.Helpy","RuneSchema Helpy","0.7.5.9"};
nlohmann::json EmbeddedCatalog() {
    nlohmann::json items=nlohmann::json::array(),definitions=nlohmann::json::array();
    for(const auto& entry:PS::F2Catalog::BundledPaths()) {
        const auto name=PS::QuickDecorations::ReadableAssetName(entry.path);
        if(entry.kind==PS::F2Catalog::Kind::Item) {
            items.push_back({{"Path",entry.path},{"Name",name},{"Available",true},
                {"Cooked",true},{"Embedded",true},{"Reason","Embedded RSDW catalogue; validated when used"}});
            continue;
        }
        if(entry.kind!=PS::F2Catalog::Kind::Npc&&entry.kind!=PS::F2Catalog::Kind::Enemy
            &&entry.kind!=PS::F2Catalog::Kind::Resource)continue;
        const char* type=entry.kind==PS::F2Catalog::Kind::Npc?"NPC":
            entry.kind==PS::F2Catalog::Kind::Enemy?"AI":"Resource";
        const auto key=std::string("@embedded-")+PS::QuickUI::Lower(type)+":"+entry.path;
        definitions.push_back({{"Key",key},{"Name",name},{"Class",entry.path},{"Type",type},
            {"Available",true},{"Loaded",false},{"Embedded",true},
            {"TemporaryAllowed",entry.kind!=PS::F2Catalog::Kind::Npc},
            {"PermanentAllowed",false},{"TemporaryReason",entry.kind==PS::F2Catalog::Kind::Npc?
                "Refresh once in-world to validate NPC services before spawning":""},
            {"PermanentReason","Refresh once in-world before saving a permanent placement"}});
    }
    return {{"Items",std::move(items)},{"Definitions",std::move(definitions)},
        {"CatalogReady",true},{"_CatalogIndexing",false},{"IndexFinished",true},
        {"CatalogStatus","Embedded catalogue ready; Refresh validates the current world. Full Scan is manual."},
        {"IndexStage","Ready (embedded)"},{"IndexDetail","No automatic UObject scan or disk-cache rebuild was started."}};
}
struct State {const RuneSchemaHostApi* Host{};std::unique_ptr<PS::InGameQuickMenu> Menu;
    RC::Unreal::Hook::GlobalCallbackId Tick=RC::Unreal::Hook::ERROR_ID;bool KeyDown=false,RendererReady=false;uint32_t RetryTicks=0;
    std::atomic<bool> ToolsAvailable=false,ToolsWaiting=false;std::atomic<uint64_t> ToolsGeneration=1;
    std::mutex ToolsMutex;uint64_t ToolsRevision=0; nlohmann::json ToolsResult=nlohmann::json::object(),ToolsCatalog=nlohmann::json::object();
    std::map<uint64_t,nlohmann::json> Completed;};
bool CallCore(State* state,const nlohmann::json& request,nlohmann::json& response) {
    if(!state||!state->Host)return false;const auto input=request.dump();uint32_t size=0;
    auto code=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),nullptr,0,&size);
    if(code!=RS_PLUGIN_BUFFER_TOO_SMALL||!size||size>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return false;
    std::string output(size,'\0');code=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),output.data(),size,&size);
    if(code!=RS_PLUGIN_OK||!size||size>output.size())return false;if(output[size-1]=='\0')--size;output.resize(size);
    try{response=nlohmann::json::parse(output);return true;}catch(...){return false;}
}
int32_t RS_PLUGIN_CALL About(void*,const char*,char* response,uint32_t capacity,uint32_t* size) {
    constexpr char value[]=R"({"plugin":"RuneSchema.Helpy","version":"0.7.5.9","ui":"plugin-dll","umg":false})";
    if(!size)return RS_PLUGIN_INVALID_ARGUMENT;*size=static_cast<uint32_t>(sizeof(value));
    if(capacity<sizeof(value))return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(response,value,sizeof(value));return RS_PLUGIN_OK;
}
void StoreReceipt(State* state,const nlohmann::json& result) {
    const auto id=result.value("_RequestId",uint64_t{});if(!id)return;
    nlohmann::json receipt={{"Status",result.value("Status",std::string("Completed."))}};
    for(const auto* key:{"GrantResults","SpawnResult","CloneResult","CloneSource","CloneAppearance","ItemDetails","RecipeExportResult","ItemOverrideExportResult"})
        if(result.contains(key))receipt[key]=result[key];
    state->Completed[id]=std::move(receipt);while(state->Completed.size()>16)state->Completed.erase(state->Completed.begin());
}
int32_t RS_PLUGIN_CALL ToolsPush(void* context,const char* requestJson,char*,uint32_t,uint32_t* size) {
    auto* state=static_cast<State*>(context);if(!state||!requestJson||!size)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        const auto message=nlohmann::json::parse(requestJson);const auto kind=message.value("Kind",std::string{});
        const auto& payload=message.value("Payload",nlohmann::json::object());std::lock_guard lock(state->ToolsMutex);
        state->ToolsAvailable=message.value("Available",false);state->ToolsWaiting=message.value("Waiting",false);
        state->ToolsGeneration=message.value("Generation",uint64_t{1});++state->ToolsRevision;
        if(kind=="bootstrap") {state->ToolsResult=payload.value("Result",nlohmann::json::object());
            const auto core=payload.value("Catalog",nlohmann::json::object());
            for(auto it=core.begin();it!=core.end();++it)state->ToolsCatalog[it.key()]=it.value();}
        else if(kind=="catalog")for(auto it=payload.begin();it!=payload.end();++it)state->ToolsCatalog[it.key()]=it.value();
        else if(kind=="result"){state->ToolsResult=payload;StoreReceipt(state,payload);}
        else if(kind=="clear"){state->ToolsResult=payload;state->ToolsCatalog=EmbeddedCatalog();state->Completed.clear();}
        if(state->Menu)state->Menu->NotifyToolsChanged();*size=0;return RS_PLUGIN_OK;
    }catch(...){return RS_PLUGIN_FAILED;}
}
}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept {return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance) noexcept {
    if(!host||!instance||host->StructSize<sizeof(RuneSchemaHostApi)||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION)return RS_PLUGIN_INCOMPATIBLE_API;
    auto* state=new(std::nothrow) State{host};if(!state)return RS_PLUGIN_FAILED;
    state->ToolsCatalog=EmbeddedCatalog();state->ToolsRevision=1;
    PS::PluginRuntimeServices::Set(host);
    constexpr const char* capabilities[]{"helpy.navigation","helpy.item-lab","helpy.recipe-studio","helpy.journal-quests","authoring.live-preview","authoring.export"};
    for(const auto* capability:capabilities)if(host->RegisterCapability(Descriptor.Id,capability)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    if(host->RegisterService(Descriptor.Id,"helpy.about",About,state)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    if(host->RegisterService(Descriptor.Id,"helpy.tools.push",ToolsPush,state)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    host->Log(RS_LOG_INFO,Descriptor.Id,"native GUI plugin initialized");*instance=state;return RS_PLUGIN_OK;
}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUiInit(void*) noexcept {return RS_PLUGIN_OK;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUnrealInit(void* instance) noexcept {
    auto* state=static_cast<State*>(instance);if(!state)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        wchar_t modulePath[32768]{};const auto length=GetModuleFileNameW(Module,modulePath,_countof(modulePath));
        if(!length||length>=_countof(modulePath))throw std::runtime_error("cannot resolve Helpy plugin directory");
        PS::HelpySettings::Configure(std::filesystem::path(modulePath).parent_path().parent_path());
        PS::HelpySettings::EnsureLoaded();
        if(!PS::HelpySettings::IsEnabled()){state->Host->Log(RS_LOG_INFO,Descriptor.Id,"disabled by plugin settings");return RS_PLUGIN_OK;}
        state->Host->Log(RS_LOG_INFO,Descriptor.Id,"using RuneSchema host reflection services");
        DragonWilds::StaticClassStorage::Initialize();
        if (!DragonWilds::StaticClassStorage::NumericPropertyStaticClass
            || !DragonWilds::StaticClassStorage::BoolPropertyStaticClass
            || !DragonWilds::StaticClassStorage::StrPropertyStaticClass
            || !DragonWilds::StaticClassStorage::StructPropertyStaticClass
            || !DragonWilds::StaticClassStorage::ObjectPropertyStaticClass)
            throw std::runtime_error("required reflected Canvas property classes are unavailable");
        PS::SpawnToolRequests::RemoteSearch=[state](std::string query){nlohmann::json ignored;CallCore(state,{{"Action","Search"},{"Query",std::move(query)}},ignored);};
        PS::SpawnToolRequests::RemoteSubmit=[state](nlohmann::json request){
            nlohmann::json result;const bool accepted=CallCore(state,{{"Action","Submit"},{"Request",std::move(request)}},result)&&result.value("Accepted",false);
            if(accepted)state->ToolsWaiting=true;return accepted;
        };
        PS::SpawnToolRequests::RemoteCompleted=[state](uint64_t id,nlohmann::json& receipt){std::lock_guard lock(state->ToolsMutex);const auto found=state->Completed.find(id);if(found==state->Completed.end())return false;receipt=std::move(found->second);state->Completed.erase(found);return true;};
        PS::SpawnToolRequests::RemoteReadQuick=[state](uint64_t& revision,int category,nlohmann::json& snapshot){
            std::lock_guard lock(state->ToolsMutex);if(revision==state->ToolsRevision)return false;
            snapshot=state->ToolsResult;snapshot["_Category"]=category;
            const auto copy=[&](const char* key){if(state->ToolsCatalog.contains(key))snapshot[key]=state->ToolsCatalog[key];};
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
        state->Menu=std::make_unique<PS::InGameQuickMenu>();
        RC::Unreal::Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema.Helpy");options.HookName=TEXT("HelpyPluginHotkey");
        state->Tick=RC::Unreal::Hook::RegisterEngineTickPostCallback([state](auto&,auto*,float,bool){
            if(!state->RendererReady&&++state->RetryTicks>=300){state->RetryTicks=0;state->RendererReady=state->Menu&&state->Menu->Initialize(false);
                if(state->RendererReady)state->Host->Log(RS_LOG_INFO,Descriptor.Id,"renderer attached; Helpy is ready");}
            const bool down=(GetAsyncKeyState(PS::HelpyHotkeys::Active.load())&0x8000)!=0;
            if(down&&!state->KeyDown&&state->Menu&&state->RendererReady)state->Menu->Toggle();state->KeyDown=down;
        },options);
        state->Host->Log(RS_LOG_INFO,Descriptor.Id,"hotkey active; renderer will attach when the game viewport is ready");return RS_PLUGIN_OK;
    } catch(const std::exception& error) {state->Host->Log(RS_LOG_ERROR,Descriptor.Id,error.what());return RS_PLUGIN_FAILED;}
      catch(...) {state->Host->Log(RS_LOG_ERROR,Descriptor.Id,"unknown Unreal initialization failure");return RS_PLUGIN_FAILED;}
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* instance) noexcept {
    auto* state=static_cast<State*>(instance);if(!state)return;
    if(state->Tick!=RC::Unreal::Hook::ERROR_ID)RC::Unreal::Hook::UnregisterCallback(state->Tick);
    PS::SpawnToolRequests::ClearRemote();
    if(state->Menu)state->Menu->Shutdown();delete state;
    PS::PluginRuntimeServices::Reset();
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {if(reason==DLL_PROCESS_ATTACH)Module=module;return TRUE;}
