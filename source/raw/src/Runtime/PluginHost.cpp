#include "Runtime/HelpyHudInterfaces.h"
#include "Runtime/PluginHost.h"
#include "Runtime/ModLocalServices.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include "Runtime/HelpyModMenu.h"
#include "Core/HelpyActivity.h"
#include "Core/PlayerCoordinates.h"
#include "../../../shared/HelpyDesktopTransport.h"
#include "Core/MapPresentationCache.h"
#include "Runtime/PluginCatalog.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/MappingBackbone.h"
#include "Runtime/HostServices.h"
#include "Core/UsmapIndex.h"
#include "Generator/ToolRequest.h"
#include "Generator/AssetSearch.h"
#include "Generator/HelpyQueryService.h"
#include "Core/QuestPresentationCatalog.h"
#include "Utility/BuildInfo.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
#include "Runtime/Storefront.h"
#include "Runtime/ExecutableVersion.h"
#include "Runtime/SteamBuild.h"
#include "Runtime/UE4SSCompatibility.h"
#include "Runtime/NetworkStatus.h"
#include "Runtime/MultiplayerPolicy.h"
#include "Runtime/MultiplayerRuntime.h"
#include "Runtime/PluginEvents.h"
#include "Runtime/ItemSnapshotService.h"
#include "Core/ItemSnapshot.h"
#include "Core/ItemDestroy.h"
#include "Runtime/ItemDestroyService.h"
#include "Core/ItemGrant.h"
#include "Runtime/ItemGrantService.h"
#include "Runtime/HelpyDeliveryGate.h"
#include "Core/PluginServerRequest.h"
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRole.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PlayerStatus.h"
#include "Runtime/GameFrameRate.h"
#include "Core/HelpySourceCache.h"
#include "Core/HelpyCacheOwnership.h"
#include "Core/IconCacheBuild.h"
#include "Core/HelpyAuthoringProtocol.h"
#include "Core/RuneSchemaSettingsEditor.h"
#include "Core/CharacterEntryRecovery.h"
#include "Runtime/QuickLoad.h"
#include "Runtime/AdditionalLogging.h"
#include "Core/CrashDumpConfiguration.h"
#include "Runtime/ModManagerService.h"
#include "Generator/HelpyRuntimePreferences.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/DragonWildsSignatures.h"
#include <windows.h>
#include <algorithm>
#include <mutex>
#include "Core/GameplayEventFeed.h"
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace PS {
namespace {
struct Service { std::string Owner;RuneSchemaServiceFn Callback{};void* Context{}; };
struct Loaded { PluginCatalog::Plugin Manifest;HMODULE Module{};void* Instance{};RuneSchemaPluginShutdownFn Shutdown{};RuneSchemaPluginPhaseFn UiInit{};RuneSchemaPluginPhaseFn UnrealInit{};bool UnrealReady=false; };
std::mutex Gate;std::unordered_map<std::string,Service> Services;std::unordered_map<std::string,std::string> Capabilities;
std::unordered_map<std::string,std::string> ActiveConnections;
// Owned plugin events: loaded Host=helpy game halves (guarded by Gate) and the push target.
std::unordered_set<std::string> HelpyGameHalves;
std::unordered_set<std::string> DestroyCallers;
// Capabilities each loaded game-side plugin declared in its manifest (guarded by Gate). Declaring
// item.grant or server.request is the installer's consent; several plugins may declare them.
std::unordered_map<std::string,std::unordered_set<std::string>> DeclaredCapabilities;
bool Declared(const char* caller,std::string_view capability){
    std::scoped_lock lock(Gate);const auto found=DeclaredCapabilities.find(caller);
    return found!=DeclaredCapabilities.end()&&found->second.contains(std::string(capability));
}
// runeschema.server.request: the bridge transport and the answers this game is waiting for.
std::mutex ServerRequestGate;PluginHost::ServerTransport ServerSend;PluginServerRequest::Pending ServerPending;
// The server request whose "<Id>.server" callback is running on this thread, for item.grant.
thread_local const PluginServerRequest::Serving* CurrentServing=nullptr;
std::mutex EventGate;HelpyDesktop::Server* EventDesktop=nullptr;std::unordered_map<std::string,PluginEvents::Bucket> EventBuckets;
thread_local const char* ActiveOwner=nullptr;
std::atomic<bool> DesktopBridgeReady=false;
bool Token(const char* value){return value&&PluginCatalog::Token(value);}
int32_t JsonResponse(const nlohmann::json& value,char* response,uint32_t capacity,uint32_t* size) {
    if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
    const auto text=value.dump();
    if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;
    *size=static_cast<uint32_t>(text.size()+1);
    if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
std::mutex MultiplayerPolicyLifetime;
int32_t RS_PLUGIN_CALL CallService(const char*,const char*,const char*,char*,uint32_t,uint32_t*);
int32_t RS_PLUGIN_CALL CoreTools(void*,const char* requestJson,char* response,uint32_t capacity,uint32_t* size) {
    try {
        const auto request=nlohmann::json::parse(requestJson);const auto action=request.value("Action",std::string{});
        nlohmann::json result;
        if(action=="MultiplayerPolicyLocation")
            return JsonResponse({{"Path",RuntimeModPath::Text(HostServices::SettingsDirectory()/"multiplayer.json")}},response,capacity,size);
        if(action=="MultiplayerRoute")return JsonResponse(MultiplayerRuntime::Probe(request),response,capacity,size);
        if(action=="MultiplayerTuning"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(request.value("Operation",std::string("Read"))!="Read"&&(!response||capacity<8192)){
                *size=8192;return RS_PLUGIN_BUFFER_TOO_SMALL;
            }
            return JsonResponse(MultiplayerRuntime::Control(request),response,capacity,size);
        }
        if(action=="HudCapabilities")return JsonResponse({{"InputOwnership",true},{"Extensions",true}},response,capacity,size);
        if(action=="HudInterfacesRead")return JsonResponse(HelpyHudInterfaces::Read(request.value("Revision",~uint64_t{})),response,capacity,size);
        if(action=="HudInterfacesWrite")return JsonResponse(HelpyHudInterfaces::Queue(request.at("Payload")),response,capacity,size);
        if(action=="HudExtensions"){
            if(Storefront::IsDedicatedServer())return JsonResponse({{"Available",false},{"Clients",nlohmann::json::array()}},response,capacity,size);
            const auto payload=request.at("Payload").dump();return CallService("RuneSchema.Helpy","hud.extensions",payload.c_str(),response,capacity,size);
        }
        if(action=="HelpyNativeOverlay"){
            if(Storefront::IsDedicatedServer())return JsonResponse({{"Ready",false},{"Error","Dedicated servers use Helpy's desktop window"}},response,capacity,size);
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<8192){*size=8192;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            const auto payload=request.at("Payload").dump();return CallService("RuneSchema.Helpy","helpy.native-overlay",payload.c_str(),response,capacity,size);
        }
        if(action=="HelpyVisibility"){
            if(!request.contains("Visible")||!request.at("Visible").is_boolean())return RS_PLUGIN_INVALID_ARGUMENT;
            if(request.contains("CaptureInput")&&!request.at("CaptureInput").is_boolean())return RS_PLUGIN_INVALID_ARGUMENT;
            const auto code=JsonResponse({{"Accepted",true}},response,capacity,size);
            if(code==RS_PLUGIN_OK){HelpyActivity::CaptureInput=request.value("CaptureInput",false)&&!Storefront::IsDedicatedServer();HelpyActivity::Visible=request.at("Visible").get<bool>();}
            return code;
        }
        if(action=="HudInputCapture"){
            if(!request.contains("Active")||!request["Active"].is_boolean())return RS_PLUGIN_INVALID_ARGUMENT;
            const auto code=JsonResponse({{"Accepted",true}},response,capacity,size);
            if(code==RS_PLUGIN_OK)HelpyActivity::HudCaptureInput=request["Active"].get<bool>()&&!Storefront::IsDedicatedServer();
            return code;
        }
        if(action=="HelpyProfileChanged"){
            const auto code=JsonResponse({{"Accepted",true}},response,capacity,size);if(code==RS_PLUGIN_OK&&SpawnToolRequests::PushUpdate)SpawnToolRequests::PushUpdate("profile",nlohmann::json::object(),0);return code;
        }
        if(action=="DesktopBridgeStatus")result={{"Ready",DesktopBridgeReady.load()}};
        else if(action=="HelpyModMenuRead")result=HelpyModMenu::Read(request.value("Revision",~uint64_t{}));
        else if(action=="HelpyModMenuWrite"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<4096){*size=4096;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=HelpyModMenu::Queue(request.at("Payload"));
        }
        else if(action=="Environment")result={{"Storefront",Storefront::Name(Storefront::Current())},{"DedicatedServer",Storefront::IsDedicatedServer()},{"GameProcessId",GetCurrentProcessId()},
            {"NativeIconCaptureAvailable",!Storefront::IsDedicatedServer()},
            {"Platform",Storefront::PlatformName(Storefront::CurrentDetection())},
            {"RuneSchemaVersion",BuildInfo::Version},{"GameVersion",ExecutableVersion::Current()},
            {"UE4SS",UE4SSCompatibility::Snapshot()},{"Steam",SteamBuild::Snapshot()},{"UpdateChecksAvailable",true}};
        else if(action=="StartupStats"){
            result=StartupTrace::Snapshot();result["FrontendReady"]=GameFrameRate::StartupReady.load();
            if(!Storefront::IsDedicatedServer()&&!GameFrameRate::StartupReady.load())GameFrameRate::StartupProbe=true;
        }
        else if(action=="FrameRate")result={{"Available",GameFrameRate::Available()},{"Fps",GameFrameRate::Fps.load()}};
        else if(action=="GameplayEvents")result=Storefront::IsDedicatedServer()?nlohmann::json{{"Available",false}}:PS::GameplayEventFeed::Service(request);
        else if(action=="LocalCoordinates")result=Storefront::IsDedicatedServer()?nlohmann::json{{"Available",false}}:PlayerCoordinates::Current.Read(SpawnToolRequests::Generation.load());
        else if(action=="NetworkStatus"){
            SpawnToolRequests::RequestPlayerRoles();
            result=NetworkStatus::Current.Snapshot(SpawnToolRequests::Generation.load(),SpawnToolRequests::Available.load(),SpawnToolRequests::Waiting.load());
        }
        else if(action=="ModsRead"||action=="ModsSave"||action=="ModsImport"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=ModManager::Service(request);
        }
        else if(action=="QuickLoadRead")result=QuickLoad::Read();
        else if(action=="AdditionalLoggingRead")result=AdditionalLogging::Read();
        else if(action=="CrashDumpRead")result=CrashDumpConfiguration::Read(HostServices::ModDirectory().parent_path().parent_path());
        else if(action=="CrashDumpSave"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=CrashDumpConfiguration::Save(HostServices::ModDirectory().parent_path().parent_path(),request);
        }
        else if(action=="AdditionalLoggingSave"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=AdditionalLogging::Save(request);
        }
        else if(action=="QuickLoadSave"){
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=QuickLoad::Save(request);
        }
        else if(action=="SettingsRead"){
            result=RuneSchemaSettingsEditor::Read(HostServices::SettingsDirectory());
            result["BypassStatus"]=DragonWilds::CharacterEntryRecovery::StatusText();
        }
        else if(action=="SettingsSave"){
            // Never perform a file write during a response-size probe or force
            // a caller to repeat a successful mutation to retrieve its result.
            if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
            if(!response||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
            result=RuneSchemaSettingsEditor::Save(HostServices::SettingsDirectory(),request);
            if(result.value("Ok",false)&&result.value("HostPolicyChanged",false))PSConfig::Get()->InvalidateHelpyAuthority();
            result["BypassStatus"]=DragonWilds::CharacterEntryRecovery::StatusText();
        }
        else if(action=="QuestCatalog")result=QuestPresentationCatalog::Read(request);
        else if(action=="IconCacheSources"){
            const auto known=request.contains("Known")&&request["Known"].is_string()?request["Known"].get<std::string>():std::string{};
            result=HelpySourceCache::ForClient(known.size()<=128?known:std::string{});
        }
        else if(action=="IconCacheOwnership")result=HelpyCacheOwnership::Page(HelpySourceCache::OwnershipSnapshot(),request);
        else if(action=="IconCacheBuildRead")result=IconCacheBuild::Read(request.value("IncludeManifest",false));
        else if(action=="IconCacheBuildStart"||action=="IconCacheRefreshStart"){
            HelpySourceCache::Requested=true;
            const bool refresh=action=="IconCacheRefreshStart";
            result={{"Accepted",!Storefront::IsDedicatedServer()&&IconCacheBuild::Start(request.value("Source",std::string{}),refresh,refresh?request.value("Item",std::string{}):std::string{})}};
        }
        else if(action=="IconCacheBuildCancel"){IconCacheBuild::Cancel();result={{"Accepted",true}};}
        else if(action=="IconCacheResetAccounting"){HelpyRuntimePreferences::IconCacheEpoch.fetch_add(1);result={{"Accepted",true}};}
        else if(action=="CacheBundleImported"){AssetSearch::NotifyImportedCache();++MapPresentationCache::Revision;result={{"Accepted",true}};}
        else if(action=="AuthoringActivate")result=HelpyAuthoring::Bridge.Activate();
        else if(action=="AuthoringSubmit") {
            try {result=HelpyAuthoring::Bridge.Submit(request.at("Request"));}
            catch(const std::exception& error){result={{"Accepted",false},{"Status",error.what()}};}
        }
        else if(action=="AuthoringSnapshot")result=HelpyAuthoring::Bridge.Snapshot(request.value("Revision",uint64_t{}));
        else if(action=="Read") {
            const auto prior=request.value("Revision",uint64_t{});const auto category=request.value("Category",-1);
            if(category!=-2)SpawnToolRequests::RequestPlayerRoles();
            // Hold the game's tool mutex only to read counters and take shared catalog pointers. The (large)
            // snapshot is built after release, so Helpy never stalls the game thread's CatalogProgress/Publish.
            uint64_t revision=0;SpawnToolRequests::CatalogMap catalog,fields;
            {
                std::lock_guard lock(SpawnToolRequests::Mutex);revision=SpawnToolRequests::Revision;
                result={{"Available",SpawnToolRequests::Available.load()},{"Waiting",SpawnToolRequests::Waiting.load()},
                    {"Generation",SpawnToolRequests::Generation.load()},{"Revision",revision},{"Changed",prior!=revision}};
                if(prior!=revision){catalog=SpawnToolRequests::QuickCatalogData;fields=SpawnToolRequests::Result;}
            }
            if(prior!=revision){
                const bool full=category!=0&&category!=1&&category!=2&&category!=-2;
                auto base=full?SpawnToolRequests::Expand(fields):nlohmann::json::object();
                if(!full){
                    if(category!=-2){const auto it=fields.find("Status");base["Status"]=it==fields.end()?nlohmann::json("Ready."):*it->second;}
                    catalog.merge(fields);
                }
                result["Snapshot"]=SpawnToolRequests::CatalogSnapshot(std::move(base),catalog,category);
            }
        } else if(action=="Submit") result={{"Accepted",SpawnToolRequests::TrySubmit(request.at("Request"))}};
        else if(action=="QuerySubmit") result={{"Accepted",HelpyQueryService::Submit(request.at("Request"))}};
        else if(action=="MapGroundSubmit") result={{"Accepted",SpawnToolRequests::SubmitMapGround(request.at("Request"))}};
        else if(action=="QueryCancel") {HelpyQueryService::Cancel(request.value("Category",-1),request.value("CancelGeneration",uint64_t{}));result={{"Accepted",true}};}
        else if(action=="IconSubmit") {
            const auto icon=request.value("Icon",std::string{});const bool accepted=!icon.empty()&&icon.size()<=4096
                &&ItemIconRequests::TrySubmit(nlohmann::json{{"Icon",icon}});
            result={{"Accepted",accepted}};
        }
        else if(action=="Completed") {nlohmann::json receipt;const bool found=SpawnToolRequests::TakeCompleted(request.value("RequestId",uint64_t{}),receipt);result={{"Found",found}};if(found)result["Receipt"]=std::move(receipt);}
        else if(action=="Search") {SpawnToolRequests::PrioritizeSearch(request.value("Query",std::string{}));result={{"Accepted",true}};}
        else if(action=="Cancel") {SpawnToolRequests::CancelRequested=true;result={{"Accepted",true}};}
        else return RS_PLUGIN_INVALID_ARGUMENT;
        return JsonResponse(result,response,capacity,size);
    }catch(...){return RS_PLUGIN_FAILED;}
}
int32_t RS_PLUGIN_CALL CoreDiscovery(void*,const char* requestJson,char* response,uint32_t capacity,uint32_t* size) {
    try {
        const auto request=nlohmann::json::parse(requestJson);
        if(!request.is_object())return RS_PLUGIN_INVALID_ARGUMENT;
        nlohmann::json services=nlohmann::json::array(),capabilities=nlohmann::json::array(),connections=nlohmann::json::array();
        {std::scoped_lock lock(Gate);
        for(const auto& [name,service]:Services)services.push_back({{"Name",name},{"Owner",service.Owner}});
        for(const auto& [name,owner]:Capabilities)capabilities.push_back({{"Name",name},{"Owner",owner}});
        for(const auto& [name,owner]:ActiveConnections)connections.push_back({{"Name",name},{"Owner",owner}});}
        const auto& mapping=MappingBackbone::Current(std::filesystem::path(HostServices::WorkingDirectory()));
        const auto parser=Usmap::Index::Shared().Snapshot();
        return JsonResponse({{"ApiVersion",RUNESCHEMA_PLUGIN_API_VERSION},{"Services",services},
            {"Capabilities",capabilities},{"Connections",connections},{"MappingBackbone",{
                {"Available",mapping.Available},{"Path",mapping.Available?mapping.Path.string():std::string{}},
                {"Bytes",mapping.Size},{"Fingerprint",mapping.Fingerprint},{"Index",parser}}}},response,capacity,size);
    }catch(...){return RS_PLUGIN_FAILED;}
}
int32_t RS_PLUGIN_CALL CoreMapping(void*,const char* requestJson,char* response,uint32_t capacity,uint32_t* size) {
    if(!requestJson)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        const auto request=nlohmann::json::parse(requestJson);
        const auto& mapping=MappingBackbone::Current(std::filesystem::path(HostServices::WorkingDirectory()));
        const auto result=Usmap::Index::Shared().Handle(mapping.Path,mapping.Fingerprint,
            HostServices::MappingCacheDirectory()/"index",request);
        return JsonResponse(result,response,capacity,size);
    } catch(const std::exception& error) {
        return JsonResponse({{"Ok",false},{"Error",error.what()}},response,capacity,size);
    } catch(...) {
        return JsonResponse({{"Ok",false},{"Error","Mapping query failed"}},response,capacity,size);
    }
}
int32_t RS_PLUGIN_CALL CoreBindings(void*,const char* requestJson,char* response,uint32_t capacity,uint32_t* size) {
    if(!requestJson)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        const auto request=nlohmann::json::parse(requestJson);
        if(!request.is_object()||request.value("Action",std::string("Status"))!="Status")return RS_PLUGIN_INVALID_ARGUMENT;
        static constexpr std::array<const char*,7> names{{"FPakPlatformFile::GetPakFolders","UObjectGlobals::StaticFindObject",
            "FName::Constructor","FMemory::Free","UDataTable::Serialize","GetObjectsOfClass","FName::ToString_Wchar"}};
        const auto requested=request.value("Binding",std::string{});nlohmann::json rows=nlohmann::json::array();
        for(const auto* name:names)if(requested.empty()||requested==name)rows.push_back({{"Binding",name},
            {"Available",DragonWilds::SignatureManager::GetSignature(name)!=nullptr},
            {"Source",DragonWilds::SignatureManager::GetSource(name)}});
        return JsonResponse({{"Ok",true},{"Bindings",std::move(rows)}},response,capacity,size);
    }catch(...){return RS_PLUGIN_FAILED;}
}
void RS_PLUGIN_CALL HostLog(uint32_t level,const char* plugin,const char* message) {
    const std::string_view id=plugin?plugin:"unknown",text=message?message:"";
    if(level==RS_LOG_ERROR)PS::PluginLog<RC::LogLevel::Error>(id,text);
    else if(level==RS_LOG_WARNING)PS::PluginLog<RC::LogLevel::Warning>(id,text);
    else PS::PluginLog<RC::LogLevel::Normal>(id,text);
}
int32_t RS_PLUGIN_CALL RegisterCapability(const char* plugin,const char* capability) {
    if(!Token(plugin)||!Token(capability)||!ActiveOwner||std::string_view(plugin)!=ActiveOwner)return RS_PLUGIN_INVALID_ARGUMENT;
    std::scoped_lock lock(Gate);auto [it,inserted]=Capabilities.emplace(capability,plugin);
    return inserted||it->second==plugin?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;
}
int32_t RS_PLUGIN_CALL RegisterService(const char* plugin,const char* name,RuneSchemaServiceFn callback,void* context) {
    if(!Token(plugin)||!Token(name)||!callback||!ActiveOwner||std::string_view(plugin)!=ActiveOwner||std::string_view(name)==PluginEvents::Service||std::string_view(name)==ItemSnapshot::Service||std::string_view(name)==ItemDestroy::Service
        ||std::string_view(name)==ItemGrant::Service||std::string_view(name)==PluginServerRequest::Service)return RS_PLUGIN_INVALID_ARGUMENT;
    // "<Id>.server" and "<Id>.server-result" belong to plugin <Id> only.
    const std::string_view text(name);
    for(const std::string_view suffix:{std::string_view(".server"),std::string_view(".server-result")})
        if(text.ends_with(suffix)&&text.substr(0,text.size()-suffix.size())!=plugin)return RS_PLUGIN_INVALID_ARGUMENT;
    std::scoped_lock lock(Gate);return Services.emplace(name,Service{plugin,callback,context}).second?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;
}
// runeschema.server.request, server side: the sender is the controller RuneSchema found, never the payload.
static nlohmann::json ServePluginRequest(RC::Unreal::UObject* controller,bool remote,const std::string& plugin,const PluginServerRequest::Request& request,const PluginServerRequest::Deliveries& sender={}){
    namespace P=PluginServerRequest;
    const auto character=ItemSnapshot::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(DragonWilds::ActorHelper::GetObjectRef(controller,TEXT("PlayerState"))));
    if(character.empty())return P::Result(request.Action,request.Id,false,nlohmann::json("The sending character is not initialized yet."),P::NotReady);
    Service service;
    {std::scoped_lock lock(Gate);const auto found=Services.find(P::ServerService(plugin));
        if(found==Services.end()||found->second.Owner!=plugin)return P::Result(request.Action,request.Id,false,nlohmann::json(plugin+" is not installed on this server."),P::NotInstalled);
        service=found->second;}
    const auto call=P::ServerCall(plugin,request,character,remote).dump();
    std::string reply(P::MaxReplyBytes,'\0');uint32_t size=0;int32_t code=RS_PLUGIN_FAILED;
    // Core services the server half calls during this callback see which request it is serving.
    const P::Serving serving{plugin,character,remote,sender};
    struct ServingScope{const P::Serving* previous;explicit ServingScope(const P::Serving* now):previous(CurrentServing){CurrentServing=now;}~ServingScope(){CurrentServing=previous;}};
    try{ServingScope scope(&serving);code=service.Callback(service.Context,call.c_str(),reply.data(),static_cast<uint32_t>(reply.size()),&size);}catch(...){code=RS_PLUGIN_FAILED;}
    if(code==RS_PLUGIN_BUFFER_TOO_SMALL)return P::Result(request.Action,request.Id,false,nlohmann::json("The server's reply exceeded the transport limit."));
    if(code!=RS_PLUGIN_OK||!size||size>reply.size())return P::Result(request.Action,request.Id,false,nlohmann::json("The server's "+plugin+" refused the request (code "+std::to_string(code)+")."));
    if(reply[size-1]=='\0')--size;reply.resize(size);
    const auto value=nlohmann::json::parse(reply,nullptr,false);
    if(!value.is_object())return P::Result(request.Action,request.Id,false,nlohmann::json("The server's "+plugin+" returned an invalid reply."));
    return P::Result(request.Action,request.Id,true,value);
}
static RC::Unreal::UObject* LocalController(){
    using namespace RC::Unreal;
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerController"));
    if(!type)return nullptr;TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
    for(auto* candidate:controllers){
        if(!candidate||candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))||!candidate->GetWorld())continue;
        try{DragonWilds::ActorHelper::FunctionCall local(candidate,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();if(local.Result<bool>())return candidate;}catch(...){}
    }
    return nullptr;
}
// runeschema.server.request, sending side. Local (single player or host): the reply carries the result.
// Remote: the reply carries the bridge revision and the result arrives at "<Plugin>.server-result".
static int32_t ServerRequest(const char* caller,const char* request,char* response,uint32_t capacity,uint32_t* size){
    namespace P=PluginServerRequest;
    *size=0;
    // A size probe sends nothing.
    constexpr uint32_t Needed=4096;
    if(!response||capacity<Needed){*size=Needed;return RS_PLUGIN_BUFFER_TOO_SMALL;}
    {using namespace RC::Unreal;if(!IsInGameThread())return RS_PLUGIN_FAILED;}
    P::Request parsed;
    try{parsed=P::Parse(nlohmann::json::parse(request));}catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
    auto* controller=LocalController();
    if(!controller)return RS_PLUGIN_NOT_FOUND;
    nlohmann::json reply;
    if(Network::OwnsGameplay(Network::Detect(controller).Mode))reply={{"Local",true},{"Result",ServePluginRequest(controller,false,caller,parsed)}};
    else{
        std::scoped_lock lock(ServerRequestGate);
        if(!ServerSend)return RS_PLUGIN_NOT_FOUND;
        if(ServerPending.Full(P::Pending::Clock::now()))return RS_PLUGIN_FAILED;
        // This PC's own delivery flags for its character, with the caller's own pause exempt:
        // the server cannot read them, and a grant while serving this request needs them open.
        P::Deliveries sender{true,false,"unknown"};
        try{
            const auto character=PlayerStatusRuntime::CharacterGuid(DragonWilds::ActorHelper::GetObjectRef(controller,TEXT("PlayerState")));
            const auto block=HelpyDeliveryGate::Assess(character,"",ItemGrant::OwnPauseMod(caller));
            if(!block)sender={true,true,""};
            else sender.By=block->User?std::string("helpy"):block->Id.empty()?std::string("unknown"):block->Mod;
        }catch(...){}
        const auto revision=ServerSend(caller,parsed.Action,P::Wire(parsed,sender).dump());
        if(revision<=0)return RS_PLUGIN_NOT_FOUND;
        ServerPending.Add(revision,{caller,parsed.Action,parsed.Id,P::Pending::Clock::now()});
        reply={{"Local",false},{"Revision",revision}};
    }
    return JsonResponse(reply,response,capacity,size);
}
// helpy.plugin.event: queue an owner-tagged event for the caller's own Helpy-side half.
// Nonblocking: OK = queued; NOT_FOUND = not a Host=helpy game half or Helpy is not attached;
// FAILED = over the per-owner rate (coalesce and retry later); INVALID_ARGUMENT = bad request.
static int32_t PluginEvent(const char* caller,const char* request,uint32_t* size) {
    *size=0;
    {std::scoped_lock lock(Gate);if(!HelpyGameHalves.contains(caller))return RS_PLUGIN_NOT_FOUND;}
    try{
        std::scoped_lock lock(EventGate);if(!EventDesktop)return RS_PLUGIN_NOT_FOUND;
        auto& bucket=EventBuckets[caller];auto probe=bucket;
        if(!PluginEvents::Take(probe,GetTickCount64()))return RS_PLUGIN_FAILED;
        const auto event=PluginEvents::Build(caller,request,probe.Sequence).dump();
        if(!EventDesktop->TryPush(event))return RS_PLUGIN_NOT_FOUND;
        bucket=probe;return RS_PLUGIN_OK;
    }catch(const std::exception&){return RS_PLUGIN_INVALID_ARGUMENT;}
}
int32_t RS_PLUGIN_CALL CallService(const char* caller,const char* name,const char* request,char* response,uint32_t capacity,uint32_t* size) {
    if(!Token(caller)||!Token(name)||!request||!size||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE
        ||(!response&&capacity)||strnlen_s(request,RUNESCHEMA_PLUGIN_MAX_MESSAGE+1)>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    if(std::string_view(name)==PluginEvents::Service)return PluginEvent(caller,request,size);
    // Core-owned and read-only; game thread, authoritative world. Not reachable over the desktop pipe.
    if(std::string_view(name)==ItemSnapshot::Service)return ItemSnapshotService::Call(request,response,capacity,size);
    // Core-owned grant into one inventory; the caller must declare item.grant in its manifest.
    if(std::string_view(name)==ItemGrant::Service)return ItemGrantService::Call(caller,Declared(caller,ItemGrant::Capability),CurrentServing,request,response,capacity,size);
    if(std::string_view(name)==PluginServerRequest::Service){
        if(!Declared(caller,PluginServerRequest::Capability))return RS_PLUGIN_INVALID_ARGUMENT;
        return ServerRequest(caller,request,response,capacity,size);
    }
    if(std::string_view(name)==ItemDestroy::Service){
        bool permitted=false;{std::scoped_lock lock(Gate);const auto cap=Capabilities.find(std::string(ItemDestroy::Capability));permitted=DestroyCallers.contains(caller)&&cap!=Capabilities.end()&&cap->second==caller;}
        // A remote player's character only while serving that player's own request (PluginServerRequest::RequireSender).
        if(permitted&&response&&capacity>=ItemDestroy::MinCapacity){
            std::string target;
            try{const auto input=nlohmann::json::parse(request);if(input.is_object()&&input.contains("Player")&&input["Player"].is_string())target=ItemSnapshot::NormalizeGuid(input["Player"].get<std::string>());}catch(...){}
            if(!target.empty())try{
                std::string local;
                {using namespace RC::Unreal;if(IsInGameThread())if(auto* own=LocalController())local=ItemSnapshot::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(DragonWilds::ActorHelper::GetObjectRef(own,TEXT("PlayerState"))));}
                PluginServerRequest::RequireSender(caller,CurrentServing,target,!local.empty()&&local==target);
            }catch(const std::exception& error){return JsonResponse(ItemDestroy::Refusal("Target",error.what()),response,capacity,size);}
        }
        return ItemDestroyService::Call(caller,permitted,request,response,capacity,size);
    }
    std::unique_lock policyLock(MultiplayerPolicyLifetime,std::defer_lock);
    if(std::string_view(name)==MultiplayerPolicy::Service){
        if(std::string_view(caller)!="RuneSchema.Core")return RS_PLUGIN_INVALID_ARGUMENT;
        policyLock.lock();
    }
    Service service;{std::scoped_lock lock(Gate);const auto found=Services.find(name);if(found==Services.end())return RS_PLUGIN_NOT_FOUND;service=found->second;}
    try{return service.Callback(service.Context,request,response,capacity,size);}catch(...){return RS_PLUGIN_FAILED;}
}
// Desktop pipe: a Helpy frontend plugin calls a service owned by its own in-game half.
static nlohmann::json OwnedServiceCall(const nlohmann::json& request) {
    const auto caller=request.value("Caller",std::string{}),name=request.value("Service",std::string{});
    const auto input=request.value("Input",std::string{});const auto capacity=request.value("Capacity",uint32_t{});
    if(capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE||input.size()>RUNESCHEMA_PLUGIN_MAX_MESSAGE||input.find('\0')!=std::string::npos)return {{"Code",RS_PLUGIN_INVALID_ARGUMENT}};
    Service service;{std::scoped_lock lock(Gate);const auto found=Services.find(name);
        if(found==Services.end()||!PluginCatalog::OwnedServiceAllowed(caller,name,found->second.Owner))return {{"Code",RS_PLUGIN_NOT_FOUND}};service=found->second;}
    thread_local std::vector<char> output;if(output.size()<capacity)output.resize(capacity);uint32_t size=0;int32_t code=RS_PLUGIN_FAILED;
    try{code=service.Callback(service.Context,input.c_str(),capacity?output.data():nullptr,capacity,&size);}catch(...){code=RS_PLUGIN_FAILED;}
    nlohmann::json reply={{"Code",code},{"Size",size}};
    if(code==RS_PLUGIN_OK&&size&&size<=capacity){auto length=size;if(output[length-1]=='\0')--length;reply["Output"]=std::string(output.data(),length);}
    return reply;
}
void* RS_PLUGIN_CALL FindObject(void* objectClass,void* objectPackage,const wchar_t* name,int32_t exactClass) {
    if(!name)return nullptr;
    return UECustom::UObjectGlobals::StaticFindObject(
        static_cast<RC::Unreal::UClass*>(objectClass),static_cast<RC::Unreal::UObject*>(objectPackage),name,exactClass!=0);
}
int32_t RS_PLUGIN_CALL ForEachObjectOfClass(void* objectClass,int32_t includeDerived,uint64_t excludeFlags,
    uint32_t exclusionInternalFlags,RuneSchemaObjectVisitorFn visitor,void* context) {
    if(!objectClass||!visitor)return RS_PLUGIN_INVALID_ARGUMENT;
    try {
        RC::Unreal::TArray<RC::Unreal::UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(static_cast<RC::Unreal::UClass*>(objectClass),objects,includeDerived!=0,
            static_cast<RC::Unreal::EObjectFlags>(excludeFlags),static_cast<RC::Unreal::EInternalObjectFlags>(exclusionInternalFlags));
        for(auto* object:objects)if(visitor(context,object)!=RS_PLUGIN_OK)break;
        return RS_PLUGIN_OK;
    }catch(...){return RS_PLUGIN_FAILED;}
}
void* RS_PLUGIN_CALL FindFieldClass(const wchar_t* name) {
    if(!name)return nullptr;
    try{return DragonWilds::PropertyHelper::FindFieldClassByName(RC::StringType{name});}catch(...){return nullptr;}
}
int32_t RS_PLUGIN_CALL IsFieldA(void* field,void* fieldClass) {
    if(!field||!fieldClass)return 0;
    try{return DragonWilds::PropertyHelper::IsPropertyA(static_cast<RC::Unreal::FField*>(field),
        static_cast<RC::Unreal::FFieldClass*>(fieldClass))?1:0;}catch(...){return 0;}
}
void* RS_PLUGIN_CALL ResolveBinding(const char* caller,const char* binding) {
    if(!Token(caller)||!binding||strnlen_s(binding,129)>128)return nullptr;
    const auto source=DragonWilds::SignatureManager::GetSource(binding);
    if(source=="unresolved")return nullptr;
    return DragonWilds::SignatureManager::GetSignature(binding);
}
int32_t RS_PLUGIN_CALL ObjectIdentity(void* object,int32_t* index,int32_t* serial){
    if(!object||!index||!serial)return RS_PLUGIN_INVALID_ARGUMENT;*index=-1;*serial=0;
    try{const WeakObjectHandle handle(static_cast<RC::Unreal::UObject*>(object));if(handle.Get()!=object)return RS_PLUGIN_NOT_FOUND;*index=handle.ObjectIndex;*serial=handle.ObjectSerialNumber;return RS_PLUGIN_OK;}catch(...){return RS_PLUGIN_FAILED;}
}
void* RS_PLUGIN_CALL ResolveObjectIdentity(int32_t index,int32_t serial){WeakObjectHandle handle;handle.ObjectIndex=index;handle.ObjectSerialNumber=serial;return handle.Get();}
const RuneSchemaModApiV1* RS_PLUGIN_CALL GetModApi(const char* plugin){
    if(!ActiveOwner||!plugin||!Token(plugin)||std::string_view(ActiveOwner)!=plugin)return nullptr;
    return ModLocalServices::Api(plugin);
}
constexpr RuneSchemaHostApi HostApi{sizeof(RuneSchemaHostApi),RUNESCHEMA_PLUGIN_API_VERSION,HostLog,RegisterCapability,
    RegisterService,CallService,FindObject,ForEachObjectOfClass,FindFieldClass,IsFieldA,ResolveBinding,ObjectIdentity,ResolveObjectIdentity,GetModApi};
void CompatibilityNotice(const std::string& message) {
    const auto mode=PSConfig::Get()->GetSettings().plugins.compatibilityNotices;
    if(mode=="off")return;
    if(mode=="normal")Log<RC::LogLevel::Normal>(TEXT("Plugin compatibility: {}\n"),ToWideSafe(message.c_str()));
    else Log<RC::LogLevel::Verbose>(TEXT("Plugin compatibility: {}\n"),ToWideSafe(message.c_str()));
}
}
struct PluginHost::State { std::vector<Loaded> LoadedPlugins;std::vector<std::string> Messages;std::unordered_set<std::string> SelectedIds;HelpyDesktop::Server Desktop;std::atomic<bool> UnrealReady=false;RC::Unreal::Hook::GlobalCallbackId OwnedInit=RC::Unreal::Hook::ERROR_ID; };
PluginHost::PluginHost():m_state(std::make_unique<State>()){}
PluginHost::~PluginHost(){Shutdown();}
void PluginHost::Load(const std::filesystem::path& root) {
    Shutdown();
    {std::scoped_lock lock(Gate);Services.insert_or_assign("runeschema.tools",Service{"RuneSchema.Core",CoreTools,nullptr});
        Services.insert_or_assign("runeschema.discovery",Service{"RuneSchema.Core",CoreDiscovery,nullptr});
        Services.insert_or_assign("runeschema.mapping",Service{"RuneSchema.Core",CoreMapping,nullptr});
        Services.insert_or_assign("runeschema.bindings",Service{"RuneSchema.Core",CoreBindings,nullptr});
        Capabilities.insert_or_assign("mapping.query","RuneSchema.Core");
        Capabilities.insert_or_assign("binding.resolve","RuneSchema.Core");}
    std::vector<PluginCatalog::Plugin> manifests;
    try{manifests=PluginCatalog::DiscoverRuntime(root,&m_state->Messages);}
    catch(const std::exception& error){m_state->Messages.push_back(std::string("plugin catalog rejected; RuneSchema core continues: ")+error.what());PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginCatalogRejected,TEXT("Plugin catalog rejected; RuneSchema core continues: {}\n"),PS::ToWideSafe(error.what()));return;}
    catch(...){m_state->Messages.push_back("plugin catalog rejected by an unknown error; RuneSchema core continues");PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginCatalogRejected,TEXT("Plugin catalog rejected by an unknown error; RuneSchema core continues.\n"));return;}
    LoadCatalog(manifests);
}
void PluginHost::LoadOwned(const std::filesystem::path& root,const std::vector<std::filesystem::path>& enabled){
    auto manifests=PluginCatalog::DiscoverOwned(root,enabled,&m_state->Messages);
    // GameInstance init may precede UE4SS game-thread identification; wait for an actual tick.
    RC::Unreal::Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("ModOwnedPluginInit");
    m_state->OwnedInit=RC::Unreal::Hook::RegisterEngineTickPostCallback([this,manifests=std::move(manifests)](auto& iteration,auto*,float,bool){
        if(!m_state->UnrealReady.load(std::memory_order_acquire))return;
        iteration.RemoveSelf();m_state->OwnedInit=RC::Unreal::Hook::ERROR_ID;
        try{LoadCatalog(manifests);OnUnrealInit();}
        catch(const std::exception& error){HostLog(RS_LOG_ERROR,"RuneSchema.Core",error.what());}
        catch(...){HostLog(RS_LOG_ERROR,"RuneSchema.Core","Mod-owned plugin initialization failed");}
    },options);
    if(m_state->OwnedInit==RC::Unreal::Hook::ERROR_ID)HostLog(RS_LOG_ERROR,"RuneSchema.Core","Mod-owned plugin initialization requires an available game-thread tick");
}
void PluginHost::LoadCatalog(const std::vector<PluginCatalog::Plugin>& catalog){
    // A Host=helpy plugin's UI DLL runs in Helpy.exe. Its optional GameEntryPoint is its
    // in-game native half: same ABI and identity, checked against GameCapabilities.
    // Catalog order is preserved; Helpy-only manifests stay listed and are skipped below.
    std::vector<PluginCatalog::Plugin> manifests;manifests.reserve(catalog.size());
    for(const auto& listed:catalog) {
        manifests.push_back(listed);
        if(listed.Host=="helpy"&&!listed.GameEntryPoint.empty()) {
            auto& gameHalf=manifests.back();gameHalf.Host="game";gameHalf.EntryPoint=listed.GameEntryPoint;
            gameHalf.Capabilities=listed.GameCapabilities;gameHalf.Connections.clear();
        }
    }
    for(const auto& manifest:manifests) {
        if(!manifest.Enabled||manifest.Host!="game")continue;
        auto identity=manifest.Id;std::transform(identity.begin(),identity.end(),identity.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(!m_state->SelectedIds.insert(identity).second){m_state->Messages.push_back(manifest.Id+": duplicate plugin identity rejected");continue;}
        // The desktop bridge and networking policy are available on dedicated hosts.
        if(Storefront::IsDedicatedServer()&&manifest.Id!="RuneSchema.Helpy"&&manifest.Id!="RSNetworking"
            &&!(manifest.ServerCompatible&&(!manifest.ModOwner.empty()||!manifest.GameEntryPoint.empty())))continue;
        std::vector<std::filesystem::path> packages;
        try{packages=PluginCatalog::PakDirectories(manifest);}
        catch(const std::exception& error){
            m_state->Messages.push_back(manifest.Id+": content rejected; core and other plugins continue: "+error.what());
            PS::Log<RC::LogLevel::Warning>(STR("[RS-PLUG-009] Plugin '{}' content rejected: {}; core and other plugins continue.\n"),PS::ToWideSafe(manifest.Id),PS::ToWideSafe(error.what()));continue;
        }
        if(manifest.ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION)
            CompatibilityNotice(manifest.Id+": manifest API "+std::to_string(manifest.ApiVersion)+", host API "+std::to_string(RUNESCHEMA_PLUGIN_API_VERSION)+"; trying native startup");
        if(!manifest.BuiltForRuneSchema.empty()&&manifest.BuiltForRuneSchema!=BuildInfo::Version)
            CompatibilityNotice(manifest.Id+": built for RuneSchema "+manifest.BuiltForRuneSchema+", running on "+BuildInfo::Version+"; trying load");
        for(const auto& [dependency,declaredVersion]:manifest.Dependencies)if(!declaredVersion.empty()&&declaredVersion!="*"&&
            std::none_of(manifests.begin(),manifests.end(),[&](const auto& candidate){return candidate.Id==dependency&&candidate.Version==declaredVersion;}))
            CompatibilityNotice(manifest.Id+": dependency "+dependency+" was developed against "+declaredVersion+"; attempting the available version");
        if(manifest.EntryPoint.empty()) {
            if(packages.empty()) {
                m_state->Messages.push_back(manifest.Id+": content plugin has no complete paks/<PackageName>/<name>.pak/.ucas/.utoc container");continue;
            }
            {std::scoped_lock lock(Gate);for(const auto& capability:manifest.Capabilities) {
                const auto [it,inserted]=Capabilities.emplace(capability,manifest.Id);
                if(!inserted&&it->second!=manifest.Id){m_state->Messages.push_back(manifest.Id+": capability conflict: "+capability);continue;}
            }for(const auto& connection:manifest.Connections) {
                const auto [it,inserted]=ActiveConnections.emplace(connection,manifest.Id);
                if(!inserted&&it->second!=manifest.Id)m_state->Messages.push_back(manifest.Id+": connection conflict: "+connection);
            }}
            m_state->LoadedPlugins.push_back({manifest,nullptr,nullptr,nullptr,nullptr,nullptr});
            m_state->Messages.push_back(manifest.Id+": loaded content plugin API 1");continue;
        }
        std::error_code pathError;
        auto absolute=std::filesystem::weakly_canonical(manifest.EntryPoint,pathError);
        if(pathError)absolute=manifest.EntryPoint;
        if(!std::filesystem::is_regular_file(absolute)){
            m_state->Messages.push_back(manifest.Id+": native DLL not found; PAK content is still available");
            PS::CodedLog<RC::LogLevel::Warning>(PS::Diagnostics::Id::PluginDllMissing,TEXT("Plugin {}: native DLL not found at '{}'; PAK content is still available.\n"),PS::ToWideSafe(manifest.Id),absolute.wstring());
            continue;
        }
        auto module=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module){const auto error=GetLastError();m_state->Messages.push_back(manifest.Id+": LoadLibraryExW failed ("+std::to_string(error)+")");PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginLoadFailed,TEXT("Plugin {}: LoadLibraryExW failed ({}) for '{}'.\n"),PS::ToWideSafe(manifest.Id),error,absolute.wstring());continue;}
        const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module,"RuneSchemaPlugin_Query"));
        const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module,"RuneSchemaPlugin_Initialize"));
        const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module,"RuneSchemaPlugin_Shutdown"));
        if(!query||!initialize||!shutdown){m_state->Messages.push_back(manifest.Id+": mandatory API exports are missing");PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginExportsMissing,TEXT("Plugin {}: mandatory API exports are missing.\n"),PS::ToWideSafe(manifest.Id));FreeLibrary(module);continue;}
        const RuneSchemaPluginDescriptor* descriptor=nullptr;try{descriptor=query();}catch(...){descriptor=nullptr;}
        if(!descriptor||descriptor->StructSize<sizeof(RuneSchemaPluginDescriptor)||descriptor->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION
            ||!descriptor->Id||manifest.Id!=descriptor->Id||!descriptor->Version) {
            m_state->Messages.push_back(manifest.Id+": native DLL identity/API is incompatible and was isolated; core, other plugins, and its PAK mounting continue");
            PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginIdentityRejected,TEXT("Plugin {}: native DLL identity/API is incompatible and was isolated; core, other plugins and PAK mounting continue.\n"),PS::ToWideSafe(manifest.Id));FreeLibrary(module);continue;
        }
        if(manifest.Version!=descriptor->Version)
            CompatibilityNotice(manifest.Id+": manifest version "+manifest.Version+", DLL version "+descriptor->Version+"; trying load");
        m_state->Messages.push_back(manifest.Id+": manifest="+manifest.Version+", dll="+descriptor->Version+
            ", built-for="+(manifest.BuiltForRuneSchema.empty()?std::string("unspecified"):manifest.BuiltForRuneSchema)+
            ", host="+BuildInfo::Version);
        if(!manifest.ModOwner.empty())try{ModLocalServices::Bind(manifest.Id,manifest.ModOwner);}catch(const std::exception& error){m_state->Messages.push_back(manifest.Id+": mod API binding failed: "+error.what());FreeLibrary(module);continue;}
        void* instance=nullptr;int32_t result=RS_PLUGIN_FAILED;ActiveOwner=manifest.Id.c_str();
        try{result=initialize(&HostApi,&instance);}catch(...){result=RS_PLUGIN_FAILED;}ActiveOwner=nullptr;
        bool complete=result==RS_PLUGIN_OK;{std::scoped_lock lock(Gate);for(const auto& capability:manifest.Capabilities) {
            const auto found=Capabilities.find(capability);if(found==Capabilities.end()||found->second!=manifest.Id){complete=false;break;}
        }}
        if(!complete){m_state->Messages.push_back(manifest.Id+": initialization/capability contract failed ("+std::to_string(result)+")");
            PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginInitFailed,TEXT("Plugin {}: initialization/capability contract failed ({}).\n"),PS::ToWideSafe(manifest.Id),result);
            {std::scoped_lock lock(Gate);std::erase_if(Services,[&](const auto& value){return value.second.Owner==manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==manifest.Id;});}
            if(ModLocalServices::Drop(manifest.Id))FreeLibrary(module);continue;}
        {std::scoped_lock lock(Gate);for(const auto& connection:manifest.Connections) {
            const auto [it,inserted]=ActiveConnections.emplace(connection,manifest.Id);
            if(!inserted&&it->second!=manifest.Id){complete=false;m_state->Messages.push_back(manifest.Id+": connection conflict: "+connection);}
        }}
        if(!complete){if(!ModLocalServices::Drop(manifest.Id))continue;if(shutdown)try{shutdown(instance);}catch(...){}
            {std::scoped_lock lock(Gate);std::erase_if(Services,[&](const auto& value){return value.second.Owner==manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==manifest.Id;});std::erase_if(ActiveConnections,[&](const auto& value){return value.second==manifest.Id;});}
            FreeLibrary(module);continue;}
        const auto uiInit=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(module,"RuneSchemaPlugin_OnUiInit"));
        const auto unrealInit=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(module,"RuneSchemaPlugin_OnUnrealInit"));
        m_state->LoadedPlugins.push_back({manifest,module,instance,shutdown,uiInit,unrealInit});
        if(std::find(manifest.Capabilities.begin(),manifest.Capabilities.end(),ItemDestroy::Capability)!=manifest.Capabilities.end()){std::scoped_lock lock(Gate);DestroyCallers.insert(manifest.Id);}
        {std::scoped_lock lock(Gate);auto& declared=DeclaredCapabilities[manifest.Id];declared.insert(manifest.Capabilities.begin(),manifest.Capabilities.end());}
        if(!manifest.GameEntryPoint.empty()){std::scoped_lock lock(Gate);HelpyGameHalves.insert(manifest.Id);}
        m_state->Messages.push_back(manifest.Id+": loaded API 1");
    }
}
void PluginHost::OnUiInit(){for(auto& plugin:m_state->LoadedPlugins)if(plugin.UiInit)try{
    if(plugin.UiInit(plugin.Instance)!=RS_PLUGIN_OK){m_state->Messages.push_back(plugin.Manifest.Id+": UI initialization rejected");
        PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginUiFailed,TEXT("Plugin {}: UI initialization rejected.\n"),PS::ToWideSafe(plugin.Manifest.Id));}
}catch(...){m_state->Messages.push_back(plugin.Manifest.Id+": UI initialization threw an exception");
    PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginUiFailed,TEXT("Plugin {}: UI initialization threw an exception.\n"),PS::ToWideSafe(plugin.Manifest.Id));}}
void PluginHost::StartDesktop(){
    const auto root=HostServices::ModDirectory();
    if(HelpyDesktop::Enabled(root)&&std::any_of(m_state->LoadedPlugins.begin(),m_state->LoadedPlugins.end(),[](const auto& plugin){return plugin.Manifest.Id=="RuneSchema.Helpy";})){
        try{if(!DesktopBridgeReady)DesktopBridgeReady=m_state->Desktop.Start(root,[](const nlohmann::json& request){
            if(request.value("Op",std::string{})=="Poll")return nlohmann::json{{"Code",RS_PLUGIN_OK}};
            if(request.value("Op",std::string{})=="Service")return OwnedServiceCall(request);
            if(request.value("Op",std::string{})!="Call"||!request.contains("Request")||!request["Request"].is_object())return nlohmann::json{{"Code",RS_PLUGIN_INVALID_ARGUMENT}};
            const auto capacity=request.value("Capacity",uint32_t{});if(capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return nlohmann::json{{"Code",RS_PLUGIN_INVALID_ARGUMENT}};
            const auto input=request["Request"].dump();if(input.size()>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return nlohmann::json{{"Code",RS_PLUGIN_INVALID_ARGUMENT}};
            thread_local std::vector<char> output;if(output.size()<capacity)output.resize(capacity);uint32_t size=0;
            const auto code=CoreTools(nullptr,input.c_str(),capacity?output.data():nullptr,capacity,&size);
            nlohmann::json reply={{"Code",code},{"Size",size}};
            if(code==RS_PLUGIN_OK&&size&&size<=output.size()){if(output[size-1]=='\0')--size;reply["Body"]=nlohmann::json::parse(output.data(),output.data()+size);}
            return reply;
        },[]{HelpyActivity::Visible=false;HelpyActivity::CaptureInput=false;});}catch(...){m_state->Desktop.Stop();DesktopBridgeReady=false;}
        if(DesktopBridgeReady){std::scoped_lock lock(EventGate);EventDesktop=&m_state->Desktop;}
        if(!DesktopBridgeReady)HostLog(RS_LOG_WARNING,"RuneSchema.Helpy","[RS-UI-002] Helpy game bridge unavailable; the application can still browse its cache offline.");
        else try{(void)Call("RuneSchema.Core","helpy.desktop.launch","{}");}catch(...){} // Older bridges launch at their normal Unreal-init phase.
    }
}
void PluginHost::OnUnrealInit(){
    StartDesktop();
    for(auto& plugin:m_state->LoadedPlugins){
    if(std::exchange(plugin.UnrealReady,true))continue;
    size_t packs=0;
    try{packs=PluginCatalog::PakDirectories(plugin.Manifest).size();}
    catch(const std::exception& error){
        const auto message=std::string("content pack declaration could not be enumerated: ")+error.what();
        HostLog(RS_LOG_WARNING,plugin.Manifest.Id.c_str(),message.c_str());
    }catch(...){HostLog(RS_LOG_WARNING,plugin.Manifest.Id.c_str(),"content pack declaration could not be enumerated");}
    const auto ready=plugin.Manifest.ConsoleMessage.empty()?std::string("initialized successfully"):plugin.Manifest.ConsoleMessage;
    const auto announcement="v"+plugin.Manifest.Version+": "+ready+" | "+
        (plugin.Manifest.EntryPoint.empty()?std::string("content"):std::string("native"))+" plugin | paks="+std::to_string(packs);
    // This wording is sourced from plugin.json. Updating a plugin version or
    // announcement does not require rebuilding RuneSchema core.
    if(!plugin.UnrealInit){HostLog(RS_LOG_INFO,plugin.Manifest.Id.c_str(),announcement.c_str());continue;}
    try{const auto result=plugin.UnrealInit(plugin.Instance);if(result!=RS_PLUGIN_OK){m_state->Messages.push_back(plugin.Manifest.Id+": Unreal initialization rejected");PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginUnrealFailed,TEXT("Plugin {}: Unreal initialization rejected ({}).\n"),PS::ToWideSafe(plugin.Manifest.Id),result);}else HostLog(RS_LOG_INFO,plugin.Manifest.Id.c_str(),announcement.c_str());}
    catch(...){m_state->Messages.push_back(plugin.Manifest.Id+": Unreal initialization threw an exception");PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::PluginUnrealFailed,TEXT("Plugin {}: Unreal initialization threw an exception.\n"),PS::ToWideSafe(plugin.Manifest.Id));}
}
    SpawnToolRequests::PushUpdate=[this](const char* kind,const nlohmann::json& payload,uint64_t revision){
        const auto message=nlohmann::json{{"Kind",kind},{"Revision",revision},{"Payload",payload},
            {"Available",SpawnToolRequests::Available.load()},{"Waiting",SpawnToolRequests::Waiting.load()},
            {"Generation",SpawnToolRequests::Generation.load()}}.dump();
        uint32_t ignored=0;CallService("RuneSchema.Core","helpy.tools.push",message.c_str(),nullptr,0,&ignored);
        if(DesktopBridgeReady)m_state->Desktop.Push(message);
    };
    SpawnToolRequests::CatalogMap bootstrap;uint64_t bootstrapRevision=0;{
        std::lock_guard lock(SpawnToolRequests::Mutex);
        bootstrap=SpawnToolRequests::Result;
        bootstrapRevision=SpawnToolRequests::Revision;
    }SpawnToolRequests::PushUpdate("bootstrap",{{"Result",SpawnToolRequests::Expand(bootstrap)}},bootstrapRevision);
    ModLocalServices::Start();
    m_state->UnrealReady.store(true,std::memory_order_release);
}
void PluginHost::Shutdown() noexcept {
    if(m_state)m_state->UnrealReady.store(false,std::memory_order_release);
    if(m_state&&m_state->OwnedInit!=RC::Unreal::Hook::ERROR_ID){RC::Unreal::Hook::UnregisterCallback(m_state->OwnedInit);m_state->OwnedInit=RC::Unreal::Hook::ERROR_ID;}
    ModLocalServices::Stop();
    {std::scoped_lock lock(EventGate);EventDesktop=nullptr;EventBuckets.clear();}
    {std::scoped_lock lock(Gate);HelpyGameHalves.clear();}
    if(m_state)m_state->Desktop.Stop();DesktopBridgeReady=false;
    HelpyActivity::Visible=false;HelpyActivity::CaptureInput=false;HelpyActivity::HudCaptureInput=false;HelpySourceCache::Requested=false;HelpySourceCache::Shutdown();
    AssetSearch::ShutdownItemCatalog();
    HelpyQueryService::Clear();
    if(!m_state)return;
    SpawnToolRequests::PushUpdate={};
    for(auto iterator=m_state->LoadedPlugins.rbegin();iterator!=m_state->LoadedPlugins.rend();++iterator) {
        std::unique_lock policyLock(MultiplayerPolicyLifetime,std::defer_lock);
        if(iterator->Manifest.Id=="RSNetworking")policyLock.lock();
        if(!ModLocalServices::Drop(iterator->Manifest.Id))continue;
        if(iterator->Shutdown)try{iterator->Shutdown(iterator->Instance);}catch(...){}
        {std::scoped_lock lock(Gate);DestroyCallers.erase(iterator->Manifest.Id);DeclaredCapabilities.erase(iterator->Manifest.Id);std::erase_if(Services,[&](const auto& value){return value.second.Owner==iterator->Manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==iterator->Manifest.Id;});std::erase_if(ActiveConnections,[&](const auto& value){return value.second==iterator->Manifest.Id;});}
        if(iterator->Module)FreeLibrary(iterator->Module);
    }
    m_state->LoadedPlugins.clear();m_state->SelectedIds.clear();
}
bool PluginHost::HasCapability(const std::string& capability) const {std::scoped_lock lock(Gate);return Capabilities.contains(capability);}
bool PluginHost::HasConnection(const std::string& connection) const {std::scoped_lock lock(Gate);return ActiveConnections.contains(connection);}
std::vector<std::string> PluginHost::Connections() const {std::scoped_lock lock(Gate);std::vector<std::string> result;result.reserve(ActiveConnections.size());for(const auto& [name,owner]:ActiveConnections)result.push_back(name);std::sort(result.begin(),result.end());return result;}
std::string PluginHost::Call(const std::string& caller,const std::string& service,const std::string& request) {
    uint32_t size=0;auto result=CallService(caller.c_str(),service.c_str(),request.c_str(),nullptr,0,&size);
    if(result!=RS_PLUGIN_BUFFER_TOO_SMALL&&result!=RS_PLUGIN_OK)throw std::runtime_error("Plugin service unavailable: "+service);
    if(!size||size>RUNESCHEMA_PLUGIN_MAX_MESSAGE)throw std::runtime_error("Plugin service returned an invalid size: "+service);
    std::string response(size,'\0');result=CallService(caller.c_str(),service.c_str(),request.c_str(),response.data(),size,&size);
    if(result!=RS_PLUGIN_OK||!size||size>response.size())throw std::runtime_error("Plugin service failed: "+service);
    if(response[size-1]=='\0')--size;response.resize(size);return response;
}
void PluginHost::SetServerTransport(ServerTransport transport){std::scoped_lock lock(ServerRequestGate);ServerSend=std::move(transport);}
std::string PluginHost::HandleRemotePluginRequest(RC::Unreal::UObject* pawn,const std::string& plugin,const std::string& action,const std::string& payload){
    namespace P=PluginServerRequest;
    std::string id;
    try{
        if(!P::Token(plugin,128)||!PluginCatalog::Token(plugin))throw std::runtime_error("Invalid plugin id.");
        const auto wire=nlohmann::json::parse(payload);
        if(wire.is_object()&&wire.contains("Request")&&wire["Request"].is_string())id=wire["Request"].get<std::string>().substr(0,36);
        if(!wire.is_object()||wire.size()>3)throw std::runtime_error("Invalid plugin request.");
        for(const auto& [key,value]:wire.items())if(key!="Request"&&key!="Body"&&key!="Deliveries")throw std::runtime_error("Invalid plugin request.");
        const auto request=P::Parse({{"Action",action},{"Request",wire.value("Request",std::string{})},{"Body",wire.value("Body",nlohmann::json::object())}});
        const auto deliveries=wire.find("Deliveries");
        const auto sender=P::ParseDeliveries(deliveries==wire.end()?nullptr:&*deliveries);
        auto* controller=pawn?DragonWilds::ActorHelper::GetObjectRef(pawn,TEXT("Controller")):nullptr;
        if(!controller||controller->GetWorld()!=pawn->GetWorld()||DragonWilds::ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=pawn)
            throw std::runtime_error("Plugin requests need the sender's current owned character.");
        return ServePluginRequest(controller,true,plugin,request,sender).dump();
    }catch(const std::exception& error){
        return P::Result(P::Token(action,64)?action:std::string("invalid"),id,false,nlohmann::json(std::string(error.what())),P::Invalid).dump();
    }
}
void PluginHost::DeliverPluginResult(int64_t revision,const std::string& plugin,const std::string& payload){
    namespace P=PluginServerRequest;
    try{
        const auto result=nlohmann::json::parse(payload);
        if(!result.is_object()||!result.contains("Request")||!result["Request"].is_string())return;
        {std::scoped_lock lock(ServerRequestGate);
            if(!ServerPending.Take(revision,plugin,result["Request"].get<std::string>(),P::Pending::Clock::now()))return;}
        Service service;
        {std::scoped_lock lock(Gate);const auto found=Services.find(P::ResultService(plugin));
            if(found==Services.end()||found->second.Owner!=plugin)return;service=found->second;}
        const auto text=result.dump();uint32_t ignored=0;
        try{service.Callback(service.Context,text.c_str(),nullptr,0,&ignored);}catch(...){}
    }catch(...){}
}
std::vector<std::string> PluginHost::Diagnostics() const{return m_state?m_state->Messages:std::vector<std::string>{};}
}
