#pragma once
#include "Core/HelpyAuthoringProtocol.h"
#include <chrono>
#include <functional>
#include <map>
namespace PS::AuthoringClient {
using Json=nlohmann::json;
inline std::function<bool(const Json&,Json&)> Remote;
inline Json Snapshot=Json::object();
inline std::map<std::string,Json> Results;
inline std::string Error;
inline Json ExportOptions={{"Name",""},{"Jsonc",false}};
inline uint64_t Sequence=0x7500000000000000ull,Revision=0,LastReceipt=0,Pending=0;
inline auto NextPoll=std::chrono::steady_clock::time_point{};
inline bool Busy(){return Pending!=0||Snapshot.value("Busy",false);}
inline void Poll(bool force=false){
    if(!Remote)return;const auto now=std::chrono::steady_clock::now();if(!force&&now<NextPoll)return;NextPoll=now+std::chrono::milliseconds(300);
    Json reply;if(!Remote({{"Action","AuthoringSnapshot"},{"Revision",Revision}},reply)){Error="Authoring bridge unavailable. Use a matching RuneSchema core build.";Pending=0;return;}
    if(!reply.is_object()){Error="Invalid authoring snapshot.";Pending=0;return;}
    if(reply.value("Generation",uint64_t{})!=Snapshot.value("Generation",uint64_t{})){Results.clear();Pending=0;LastReceipt=0;}
    if(!reply.contains("Result")&&Snapshot.contains("Result"))reply["Result"]=Snapshot["Result"];
    if(!reply.contains("History")&&Snapshot.contains("History"))reply["History"]=Snapshot["History"];
    Snapshot=std::move(reply);Revision=Snapshot.value("Revision",uint64_t{});
    const auto id=Snapshot.value("RequestId",uint64_t{});
    if(id&&id!=LastReceipt){LastReceipt=id;Results[Snapshot.value("Operation",std::string{})]=Snapshot.value("Result",Json::object());if(Pending==id)Pending=0;}
    if(!Snapshot.value("Busy",false))Pending=0;
}
inline bool Activate(){if(!Remote)return false;Json reply;if(!Remote({{"Action","AuthoringActivate"}},reply)||!reply.value("Accepted",false)){Error=reply.value("Status",std::string("Activation request failed."));return false;}NextPoll={};return true;}
inline bool Submit(const char* operation,Json arguments=Json::object()){
    if(Busy()&&std::string_view(operation)!="Cancel"){Error="Wait for the current authoring operation.";return false;}
    try{if(!arguments.contains("Export"))arguments["Export"]=ExportOptions;const auto id=++Sequence;Json request={{"RequestId",id},{"Generation",Snapshot.value("Generation",uint64_t{})},{"Operation",operation},{"Arguments",std::move(arguments)}};
        request=HelpyAuthoring::Validate(std::move(request));Json reply;
        if(!Remote||!Remote({{"Action","AuthoringSubmit"},{"Request",request}},reply)||!reply.value("Accepted",false)){Error=reply.value("Status",std::string("Authoring request was not accepted."));return false;}
        Pending=id;Error.clear();NextPoll={};return true;
    }catch(const std::exception& error){Error=error.what();return false;}
}
inline const Json& Result(const char* operation){static const Json empty=Json::object();const auto found=Results.find(operation);return found==Results.end()?empty:found->second;}
inline void Clear(){Remote={};Snapshot=Json::object();Results.clear();Pending=0;Revision=0;LastReceipt=0;Error.clear();NextPoll={};}
}
