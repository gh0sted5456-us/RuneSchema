#include "Runtime/PluginHost.h"
#include "Runtime/PluginCatalog.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Generator/ToolRequest.h"
#include "Utility/BuildInfo.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include <windows.h>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace PS {
namespace {
struct Service { std::string Owner;RuneSchemaServiceFn Callback{};void* Context{}; };
struct Loaded { PluginCatalog::Plugin Manifest;HMODULE Module{};void* Instance{};RuneSchemaPluginShutdownFn Shutdown{};RuneSchemaPluginPhaseFn UiInit{};RuneSchemaPluginPhaseFn UnrealInit{}; };
std::mutex Gate;std::unordered_map<std::string,Service> Services;std::unordered_map<std::string,std::string> Capabilities;
std::unordered_map<std::string,std::string> ActiveConnections;
thread_local const char* ActiveOwner=nullptr;
bool Token(const char* value){return value&&PluginCatalog::Token(value);}
int32_t JsonResponse(const nlohmann::json& value,char* response,uint32_t capacity,uint32_t* size) {
    if(!size)return RS_PLUGIN_INVALID_ARGUMENT;
    const auto text=value.dump();
    if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;
    *size=static_cast<uint32_t>(text.size()+1);
    if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
int32_t RS_PLUGIN_CALL CoreTools(void*,const char* requestJson,char* response,uint32_t capacity,uint32_t* size) {
    try {
        const auto request=nlohmann::json::parse(requestJson);const auto action=request.value("Action",std::string{});
        nlohmann::json result;
        if(action=="Read") {
            const auto prior=request.value("Revision",uint64_t{});const auto category=request.value("Category",-1);std::lock_guard lock(SpawnToolRequests::Mutex);
            result={{"Available",SpawnToolRequests::Available.load()},{"Waiting",SpawnToolRequests::Waiting.load()},
                {"Generation",SpawnToolRequests::Generation.load()},{"Revision",SpawnToolRequests::Revision},
                {"Changed",prior!=SpawnToolRequests::Revision}};
            if(prior!=SpawnToolRequests::Revision) {
                nlohmann::json snapshot={{"Status",SpawnToolRequests::Result.value("Status",std::string("Ready."))},{"_Category",category}};
                const auto copy=[&](const char* key){
                    if(SpawnToolRequests::QuickCatalogData.contains(key))snapshot[key]=SpawnToolRequests::QuickCatalogData[key];
                    else if(SpawnToolRequests::Result.contains(key))snapshot[key]=SpawnToolRequests::Result[key];
                };
                if(category==0){copy("Items");copy("CookedVisuals");copy("CatalogIssues");}
                else if(category==1||category==2) {
                    const auto* source=SpawnToolRequests::QuickCatalogData.contains("Definitions")?&SpawnToolRequests::QuickCatalogData:
                        SpawnToolRequests::Result.contains("Definitions")?&SpawnToolRequests::Result:nullptr;
                    if(source) {
                        snapshot["Definitions"]=nlohmann::json::array();
                        for(const auto& row:(*source)["Definitions"]) {
                            const auto type=row.value("Type",std::string{});
                            if((category==1&&(type=="AI"||type=="NPC"))||(category==2&&type=="Resource"))snapshot["Definitions"].push_back(row);
                        }
                    }
                } else {
                    snapshot=SpawnToolRequests::Result;
                    for(auto it=SpawnToolRequests::QuickCatalogData.begin();it!=SpawnToolRequests::QuickCatalogData.end();++it)snapshot[it.key()]=it.value();
                }
                for(const auto* key:{"Players","CatalogReady","_CatalogIndexing","CatalogStatus","RegistryRecords","UnresolvedAssets",
                        "MetadataUnavailable","UnclassifiedRecords","CatalogAI","CatalogResources","CatalogCoverage",
                        "IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})copy(key);
                result["Snapshot"]=std::move(snapshot);
            }
        } else if(action=="Submit") result={{"Accepted",SpawnToolRequests::TrySubmit(request.at("Request"))}};
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
        std::scoped_lock lock(Gate);
        for(const auto& [name,service]:Services)services.push_back({{"Name",name},{"Owner",service.Owner}});
        for(const auto& [name,owner]:Capabilities)capabilities.push_back({{"Name",name},{"Owner",owner}});
        for(const auto& [name,owner]:ActiveConnections)connections.push_back({{"Name",name},{"Owner",owner}});
        return JsonResponse({{"ApiVersion",RUNESCHEMA_PLUGIN_API_VERSION},{"Services",services},
            {"Capabilities",capabilities},{"Connections",connections}},response,capacity,size);
    }catch(...){return RS_PLUGIN_FAILED;}
}
void RS_PLUGIN_CALL HostLog(uint32_t level,const char* plugin,const char* message) {
    const auto id=ToWideSafe(plugin?plugin:"unknown"),text=ToWideSafe(message?message:"");
    if(level==RS_LOG_ERROR)RC::Output::send<RC::LogLevel::Error>(TEXT("[RuneSchema] Plugin {}: {}\n"),id,text);
    else if(level==RS_LOG_WARNING)RC::Output::send<RC::LogLevel::Warning>(TEXT("[RuneSchema] Plugin {}: {}\n"),id,text);
    else RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Plugin {}: {}\n"),id,text);
}
int32_t RS_PLUGIN_CALL RegisterCapability(const char* plugin,const char* capability) {
    if(!Token(plugin)||!Token(capability)||!ActiveOwner||std::string_view(plugin)!=ActiveOwner)return RS_PLUGIN_INVALID_ARGUMENT;
    std::scoped_lock lock(Gate);auto [it,inserted]=Capabilities.emplace(capability,plugin);
    return inserted||it->second==plugin?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;
}
int32_t RS_PLUGIN_CALL RegisterService(const char* plugin,const char* name,RuneSchemaServiceFn callback,void* context) {
    if(!Token(plugin)||!Token(name)||!callback||!ActiveOwner||std::string_view(plugin)!=ActiveOwner)return RS_PLUGIN_INVALID_ARGUMENT;
    std::scoped_lock lock(Gate);return Services.emplace(name,Service{plugin,callback,context}).second?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;
}
int32_t RS_PLUGIN_CALL CallService(const char* caller,const char* name,const char* request,char* response,uint32_t capacity,uint32_t* size) {
    if(!Token(caller)||!Token(name)||!request||!size||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE
        ||(!response&&capacity)||strnlen_s(request,RUNESCHEMA_PLUGIN_MAX_MESSAGE+1)>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    Service service;{std::scoped_lock lock(Gate);const auto found=Services.find(name);if(found==Services.end())return RS_PLUGIN_NOT_FOUND;service=found->second;}
    try{return service.Callback(service.Context,request,response,capacity,size);}catch(...){return RS_PLUGIN_FAILED;}
}
constexpr RuneSchemaHostApi HostApi{sizeof(RuneSchemaHostApi),RUNESCHEMA_PLUGIN_API_VERSION,HostLog,RegisterCapability,RegisterService,CallService};
void CompatibilityNotice(const std::string& message) {
    const auto mode=PSConfig::Get()->GetSettings().plugins.compatibilityNotices;
    if(mode=="off")return;
    if(mode=="normal")Log<RC::LogLevel::Normal>(TEXT("Plugin compatibility: {}\n"),ToWideSafe(message.c_str()));
    else Log<RC::LogLevel::Verbose>(TEXT("Plugin compatibility: {}\n"),ToWideSafe(message.c_str()));
}
}
struct PluginHost::State { std::vector<Loaded> LoadedPlugins;std::vector<std::string> Messages; };
PluginHost::PluginHost():m_state(std::make_unique<State>()){}
PluginHost::~PluginHost(){Shutdown();}
void PluginHost::Load(const std::filesystem::path& root) {
    Shutdown();
    {std::scoped_lock lock(Gate);Services.insert_or_assign("runeschema.tools",Service{"RuneSchema.Core",CoreTools,nullptr});
        Services.insert_or_assign("runeschema.discovery",Service{"RuneSchema.Core",CoreDiscovery,nullptr});}
    std::vector<PluginCatalog::Plugin> manifests;
    try{manifests=PluginCatalog::Discover(root,&m_state->Messages);}
    catch(const std::exception& error){m_state->Messages.push_back(std::string("plugin catalog rejected; RuneSchema core continues: ")+error.what());return;}
    catch(...){m_state->Messages.push_back("plugin catalog rejected by an unknown error; RuneSchema core continues");return;}
    for(const auto& manifest:manifests) {
        if(!manifest.Enabled)continue;
        if(!manifest.BuiltForRuneSchema.empty()&&manifest.BuiltForRuneSchema!=BuildInfo::Version)
            CompatibilityNotice(manifest.Id+": built for RuneSchema "+manifest.BuiltForRuneSchema+", running on "+BuildInfo::Version+"; attempting best-effort load");
        for(const auto& [dependency,declaredVersion]:manifest.Dependencies)if(!declaredVersion.empty()&&declaredVersion!="*"&&
            std::none_of(manifests.begin(),manifests.end(),[&](const auto& candidate){return candidate.Id==dependency&&candidate.Version==declaredVersion;}))
            CompatibilityNotice(manifest.Id+": dependency "+dependency+" was developed against "+declaredVersion+"; attempting the available version");
        if(manifest.EntryPoint.empty()) {
            if(PluginCatalog::PakDirectories(manifest).empty()) {
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
        const auto absolute=std::filesystem::weakly_canonical(manifest.EntryPoint);
        if(!std::filesystem::is_regular_file(absolute))continue;
        auto module=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module){m_state->Messages.push_back(manifest.Id+": LoadLibraryExW failed ("+std::to_string(GetLastError())+")");continue;}
        const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module,"RuneSchemaPlugin_Query"));
        const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module,"RuneSchemaPlugin_Initialize"));
        const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module,"RuneSchemaPlugin_Shutdown"));
        if(!query||!initialize||!shutdown){m_state->Messages.push_back(manifest.Id+": mandatory API exports are missing");FreeLibrary(module);continue;}
        const RuneSchemaPluginDescriptor* descriptor=nullptr;try{descriptor=query();}catch(...){descriptor=nullptr;}
        if(!descriptor||descriptor->StructSize<sizeof(RuneSchemaPluginDescriptor)||descriptor->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION
            ||!descriptor->Id||manifest.Id!=descriptor->Id||!descriptor->Version||manifest.Version!=descriptor->Version) {
            m_state->Messages.push_back(manifest.Id+": DLL descriptor does not match its manifest/API");FreeLibrary(module);continue;
        }
        void* instance=nullptr;int32_t result=RS_PLUGIN_FAILED;ActiveOwner=manifest.Id.c_str();
        try{result=initialize(&HostApi,&instance);}catch(...){result=RS_PLUGIN_FAILED;}ActiveOwner=nullptr;
        bool complete=result==RS_PLUGIN_OK;{std::scoped_lock lock(Gate);for(const auto& capability:manifest.Capabilities) {
            const auto found=Capabilities.find(capability);if(found==Capabilities.end()||found->second!=manifest.Id){complete=false;break;}
        }}
        if(!complete){m_state->Messages.push_back(manifest.Id+": initialization/capability contract failed ("+std::to_string(result)+")");
            {std::scoped_lock lock(Gate);std::erase_if(Services,[&](const auto& value){return value.second.Owner==manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==manifest.Id;});}
            FreeLibrary(module);continue;}
        {std::scoped_lock lock(Gate);for(const auto& connection:manifest.Connections) {
            const auto [it,inserted]=ActiveConnections.emplace(connection,manifest.Id);
            if(!inserted&&it->second!=manifest.Id){complete=false;m_state->Messages.push_back(manifest.Id+": connection conflict: "+connection);}
        }}
        if(!complete){if(shutdown)try{shutdown(instance);}catch(...){}
            {std::scoped_lock lock(Gate);std::erase_if(Services,[&](const auto& value){return value.second.Owner==manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==manifest.Id;});std::erase_if(ActiveConnections,[&](const auto& value){return value.second==manifest.Id;});}
            FreeLibrary(module);continue;}
        const auto uiInit=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(module,"RuneSchemaPlugin_OnUiInit"));
        const auto unrealInit=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(module,"RuneSchemaPlugin_OnUnrealInit"));
        m_state->LoadedPlugins.push_back({manifest,module,instance,shutdown,uiInit,unrealInit});
        m_state->Messages.push_back(manifest.Id+": loaded API 1");
    }
}
void PluginHost::OnUiInit(){for(auto& plugin:m_state->LoadedPlugins)if(plugin.UiInit)try{if(plugin.UiInit(plugin.Instance)!=RS_PLUGIN_OK)m_state->Messages.push_back(plugin.Manifest.Id+": UI initialization rejected");}catch(...){m_state->Messages.push_back(plugin.Manifest.Id+": UI initialization threw an exception");}}
void PluginHost::OnUnrealInit(){for(auto& plugin:m_state->LoadedPlugins){
    const auto ready=plugin.Manifest.ConsoleMessage.empty()?std::string("initialized successfully"):plugin.Manifest.ConsoleMessage;
    if(!plugin.UnrealInit){HostLog(RS_LOG_INFO,plugin.Manifest.Id.c_str(),ready.c_str());continue;}
    try{const auto result=plugin.UnrealInit(plugin.Instance);if(result!=RS_PLUGIN_OK){m_state->Messages.push_back(plugin.Manifest.Id+": Unreal initialization rejected");HostLog(RS_LOG_ERROR,plugin.Manifest.Id.c_str(),"Unreal initialization rejected");}else HostLog(RS_LOG_INFO,plugin.Manifest.Id.c_str(),ready.c_str());}
    catch(...){m_state->Messages.push_back(plugin.Manifest.Id+": Unreal initialization threw an exception");HostLog(RS_LOG_ERROR,plugin.Manifest.Id.c_str(),"Unreal initialization threw an exception");}
}
    SpawnToolRequests::PushUpdate=[](const char* kind,const nlohmann::json& payload,uint64_t revision){
        const auto message=nlohmann::json{{"Kind",kind},{"Revision",revision},{"Payload",payload},
            {"Available",SpawnToolRequests::Available.load()},{"Waiting",SpawnToolRequests::Waiting.load()},
            {"Generation",SpawnToolRequests::Generation.load()}}.dump();
        uint32_t ignored=0;CallService("RuneSchema.Core","helpy.tools.push",message.c_str(),nullptr,0,&ignored);
    };
    nlohmann::json bootstrap;uint64_t bootstrapRevision=0;{
        std::lock_guard lock(SpawnToolRequests::Mutex);
        bootstrap={{"Result",SpawnToolRequests::Result},{"Catalog",SpawnToolRequests::QuickCatalogData}};
        bootstrapRevision=SpawnToolRequests::Revision;
    }SpawnToolRequests::PushUpdate("bootstrap",bootstrap,bootstrapRevision);
}
void PluginHost::Shutdown() noexcept {
    if(!m_state)return;
    SpawnToolRequests::PushUpdate={};
    for(auto iterator=m_state->LoadedPlugins.rbegin();iterator!=m_state->LoadedPlugins.rend();++iterator) {
        if(iterator->Shutdown)try{iterator->Shutdown(iterator->Instance);}catch(...){}
        {std::scoped_lock lock(Gate);std::erase_if(Services,[&](const auto& value){return value.second.Owner==iterator->Manifest.Id;});std::erase_if(Capabilities,[&](const auto& value){return value.second==iterator->Manifest.Id;});std::erase_if(ActiveConnections,[&](const auto& value){return value.second==iterator->Manifest.Id;});}
        if(iterator->Module)FreeLibrary(iterator->Module);
    }
    m_state->LoadedPlugins.clear();
}
bool PluginHost::HasCapability(const std::string& capability) const {std::scoped_lock lock(Gate);return Capabilities.contains(capability);}
bool PluginHost::HasConnection(const std::string& connection) const {std::scoped_lock lock(Gate);return ActiveConnections.contains(connection);}
std::vector<std::string> PluginHost::Connections() const {std::scoped_lock lock(Gate);std::vector<std::string> result;result.reserve(ActiveConnections.size());for(const auto& [name,owner]:ActiveConnections)result.push_back(name);std::sort(result.begin(),result.end());return result;}
std::string PluginHost::Call(const std::string& caller,const std::string& service,const std::string& request) const {
    uint32_t size=0;auto result=CallService(caller.c_str(),service.c_str(),request.c_str(),nullptr,0,&size);
    if(result!=RS_PLUGIN_BUFFER_TOO_SMALL&&result!=RS_PLUGIN_OK)throw std::runtime_error("Plugin service unavailable: "+service);
    if(!size||size>RUNESCHEMA_PLUGIN_MAX_MESSAGE)throw std::runtime_error("Plugin service returned an invalid size: "+service);
    std::string response(size,'\0');result=CallService(caller.c_str(),service.c_str(),request.c_str(),response.data(),size,&size);
    if(result!=RS_PLUGIN_OK||!size||size>response.size())throw std::runtime_error("Plugin service failed: "+service);
    if(response[size-1]=='\0')--size;response.resize(size);return response;
}
std::vector<std::string> PluginHost::Diagnostics() const{return m_state?m_state->Messages:std::vector<std::string>{};}
}
