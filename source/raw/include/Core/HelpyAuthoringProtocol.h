#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>

namespace PS::HelpyAuthoring {
using Json=nlohmann::json;
inline constexpr std::array Operations{
    "EventSuggestions","DiagnosticGuide","TraceTargetClear","PlayerPresetSave","Cancel","ExportCaptured","Catalogue","QuestStatus","NiagaraPresets","AssetSearch","AssetCapture","AssetReference","StarterBuild","StarterCheck","Schema","SchemaExport","ExportDocument",
    "PlayerBuild","NameplateBuild","PresetsList","PresetSave","Preset","TraceProfilesList","TraceProfileSave","TraceProfileLoad",
    "TraceTarget","TraceBrowse","TraceSelect","PlayerTraceStart","PlayerTraceStop","PlayerTraceMarker","TraceStart","TraceStop","EventRuleBuild",
    "Target","Player","Browse","Object","SavedItems","SavedWorld","Clear","Imports","NpcPreview","NpcExport",
    "CleanupInspect","CleanupPreview","CleanupExport","SaveEditorList","SaveEditorOpen","SaveEditorClose","SaveEditorBrowse","SaveEditorSet","SaveEditorPreview","SaveEditorExport","SaveEditorInventory","SaveEditorStack","SaveEditorRemoveItems","NiagaraAttach","NiagaraRemove","NiagaraInspect","NiagaraArmorStart","NiagaraArmorStop"};
inline void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
inline std::string Text(const Json& data,const char* key,size_t maximum=4096,bool required=false){
    if(!data.contains(key)){Require(!required,"Required text is missing");return {};}
    Require(data[key].is_string(),"Expected a text field");auto value=data[key].get<std::string>();
    Require(value.size()<=maximum&&value.find('\0')==value.npos,"Text exceeds its bound or contains NUL");Require(!required||!value.empty(),"Required text is empty");return value;
}
inline void Structure(const Json& value,unsigned depth=0){
    Require(depth<=32,"Authoring JSON is nested too deeply");
    if(value.is_number_float())Require(std::isfinite(value.get<double>()),"Numbers must be finite");
    if(value.is_array()||value.is_object()){Require(value.size()<=8192,"Authoring collection exceeds its bound");for(const auto& child:value)Structure(child,depth+1);}
}
inline Json Validate(Json request){
    Require(request.is_object()&&request.dump().size()<=256*1024,"Authoring request must be an object no larger than 256 KiB");Structure(request);
    for(const auto& [key,value]:request.items())Require(key=="RequestId"||key=="Generation"||key=="Operation"||key=="Arguments","Unknown authoring request field");
    Require(request.contains("RequestId")&&request["RequestId"].is_number_unsigned()&&request["RequestId"].get<uint64_t>()>0,"A positive RequestId is required");
    Require(request.contains("Generation")&&request["Generation"].is_number_unsigned(),"A world generation is required");
    const auto operation=Text(request,"Operation",64,true);
    Require(std::find(Operations.begin(),Operations.end(),operation)!=Operations.end(),"Unsupported authoring operation");
    if(!request.contains("Arguments"))request["Arguments"]=Json::object();const auto& args=request["Arguments"];
    Require(args.is_object(),"Arguments must be an object");
    for(const auto* key:{"Loader","Name","Mod","Id","Path","Query","SelectedObject","Token","Player","Definition","State","Icon","Marker"})Text(args,key,key==std::string("Query")?256:4096);
    for(const auto* key:{"Confirm","Offline","ReadValues","EraseProgress","PruneInvalid","IncludeLoadedTables","Extended","Jsonc","DelayCrosshair","UseProfileTarget"})if(args.contains(key))Require(args[key].is_boolean(),"Expected a boolean option");
    for(const auto* key:{"Limit","Mode","DelaySeconds","ArmorSlots"})if(args.contains(key))Require(args[key].is_number_integer(),"Expected an integer option");
    if(args.contains("Limit"))Require(args["Limit"]>=1&&args["Limit"]<=500,"Object limit must be 1..500");
    if(args.contains("Mode"))Require(args["Mode"]>=0&&args["Mode"]<=4,"Starter mode must be 0..4");
    if(args.contains("DelaySeconds"))Require(args["DelaySeconds"]>=0&&args["DelaySeconds"]<=30,"Capture delay must be 0..30 seconds");
    if(args.contains("RangeCm"))Require(args["RangeCm"].is_number()&&args["RangeCm"]>=100&&args["RangeCm"]<=100000,"Range must be 100..100000 cm");
    for(const auto* key:{"Fields","Options","Advanced","Entry"})if(args.contains(key))Require(args[key].is_object(),"Expected an object option");
    if(args.contains("Export")){
        const auto& output=args["Export"];Require(output.is_object()&&output.size()<=2,"Invalid export preferences");
        for(const auto& [key,value]:output.items())Require(key=="Name"||key=="Jsonc","Unknown export preference");
        Text(output,"Name",96);if(output.contains("Jsonc"))Require(output["Jsonc"].is_boolean(),"Export Jsonc must be boolean");
    }
    if(operation=="CleanupExport")Require(args.value("Confirm",false)&&!Text(args,"Token",128,true).empty(),"Review and confirm a cleanup preview before export");
    if(operation=="CleanupInspect"||operation=="NpcPreview")Text(args,"Path",4096,true);
    if(operation=="SaveEditorOpen"){Text(args,"Path",4096,true);Require(args.value("Offline",false),"Confirm the character is offline");}
    if(operation=="SaveEditorBrowse"||operation=="SaveEditorSet")Text(args,"Pointer",4096);
    if(args.contains("Offset"))Require(args["Offset"].is_number_unsigned()&&args["Offset"]<=262144,"Invalid page offset");
    if(operation=="SaveEditorSet")Require(args.contains("Value")&&args.contains("Revision")&&args["Revision"].is_number_unsigned(),"Value and editor revision required");
    if(operation=="SaveEditorInventory"||operation=="SaveEditorStack"||operation=="SaveEditorRemoveItems")Require(!args.contains("Offset")||(args["Offset"].is_number_integer()&&args["Offset"]>=0&&args["Offset"]<=8192),"Inventory offset must be 0..8192");
    if(operation=="SaveEditorStack"){
        Text(args,"Pointer",4096,true);Require(args.contains("Count")&&args["Count"].is_number_integer()&&args["Count"]>=1&&args["Count"]<=10000,"Stack Count must be 1..10000");
        Require(args.contains("Revision")&&args["Revision"].is_number_unsigned(),"Current editor revision required");
    }
    if(operation=="SaveEditorRemoveItems"){
        Require(args.contains("Pointers")&&args["Pointers"].is_array()&&!args["Pointers"].empty()&&args["Pointers"].size()<=128,"Select 1..128 inventory items");
        for(const auto& pointer:args["Pointers"])Require(pointer.is_string()&&pointer.get_ref<const std::string&>().size()<=4096,"Invalid inventory pointer");
        Require(args.value("Confirm",false)&&args.contains("Revision")&&args["Revision"].is_number_unsigned(),"Confirm removals using the current editor revision");
    }
    if(operation=="SaveEditorExport")Require(args.value("Confirm",false)&&!Text(args,"Token",128,true).empty(),"Review and confirm the save-copy export");
    return request;
}

// Owns JSON values only. CallService may admit/read requests on any thread;
// only the game-thread callback consumes work or publishes engine results.
class Session {
    mutable std::mutex gate;
    bool enabled=false,ready=false,active=false,activation=false;
    uint64_t revision=1,generation=1,inflight=0,lastId=0;
    std::optional<Json> pending,cancellation;
    Json result=Json::object(),history=Json::array();
    std::string status="Authoring is waiting for the runtime.",operation,lastOperation;
    std::deque<uint64_t> admitted;
public:
    void Configure(bool allow,bool runtimeReady){std::lock_guard lock(gate);enabled=allow;ready=runtimeReady;++revision;status=!enabled?"Authoring tools are disabled in RuneSchema settings. Enable them and restart.":!ready?"Authoring runtime is unavailable.":"Activate authoring for this session.";}
    Json Activate(){std::lock_guard lock(gate);if(!enabled||!ready)return {{"Accepted",false},{"Status",status}};activation=true;return {{"Accepted",true}};}
    bool TickActivation(){std::lock_guard lock(gate);if(activation&&enabled&&ready){activation=false;active=true;status="Authoring ready. Exports do not install content.";++revision;}return active;}
    bool Active() const {std::lock_guard lock(gate);return active;}
    Json Submit(Json request){request=Validate(std::move(request));std::lock_guard lock(gate);
        if(!enabled||!ready||!active)return {{"Accepted",false},{"Status",status}};
        if(request["Generation"]!=generation)return {{"Accepted",false},{"Status","World changed; refresh authoring before submitting."}};
        const auto id=request["RequestId"].get<uint64_t>();
        if(std::find(admitted.begin(),admitted.end(),id)!=admitted.end())return {{"Accepted",true},{"Duplicate",true}};
        if(request["Operation"]=="Cancel"){if(cancellation)return {{"Accepted",false},{"Status","Cancellation is already queued."}};admitted.push_back(id);while(admitted.size()>64)admitted.pop_front();cancellation=std::move(request);++revision;return {{"Accepted",true}};}
        if(pending||inflight||cancellation)return {{"Accepted",false},{"Status","Another authoring operation is still running."}};
        admitted.push_back(id);while(admitted.size()>64)admitted.pop_front();pending=std::move(request);status="Queued for the game thread.";++revision;return {{"Accepted",true}};
    }
    std::optional<Json> TakeCancellation(){std::lock_guard lock(gate);if(!cancellation)return {};auto value=std::move(cancellation);cancellation.reset();pending.reset();inflight=(*value)["RequestId"];operation="Cancel";return value;}
    std::optional<Json> Take(){std::lock_guard lock(gate);if(!active||inflight||!pending)return {};auto value=std::move(pending);pending.reset();inflight=(*value)["RequestId"];operation=(*value)["Operation"];return value;}
    void Complete(uint64_t id,Json data,std::string message,bool success=true){std::lock_guard lock(gate);if(id!=inflight||!inflight)return;
        // Preserve full exported files, but bound plugin response copying.
        if(data.dump().size()>768*1024)data={{"Truncated",true},{"Status","Result exceeds the desktop preview limit. Export the complete result."}};
        if(!success&& !data.contains("Error"))data["Error"]=message;
        result=std::move(data);lastOperation=operation;lastId=id;inflight=0;status=std::move(message);++revision;
        history.push_back({{"RequestId",id},{"Operation",operation},{"Success",success},{"Status",status}});while(history.size()>24)history.erase(history.begin());
    }
    Json Snapshot(uint64_t prior=0) const {std::lock_guard lock(gate);Json data={{"Enabled",enabled},{"Ready",ready},{"Active",active},{"Busy",pending.has_value()||cancellation.has_value()||inflight!=0},{"Revision",revision},{"Generation",generation},{"Status",status},{"RequestId",lastId},{"Operation",lastOperation}};if(prior!=revision){data["Result"]=result;data["History"]=history;}return data;}
    void WorldChanged(){std::lock_guard lock(gate);pending.reset();cancellation.reset();inflight=0;lastId=0;result=Json::object();history=Json::array();operation.clear();lastOperation.clear();admitted.clear();++generation;++revision;status="World changed. Previous authoring results were cleared.";}
};
inline Session Bridge;
}
