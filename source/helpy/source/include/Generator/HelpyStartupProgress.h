#pragma once
#include "Core/ProcessExitFuture.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

namespace PS::HelpyStartupProgress {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline ProcessExitFuture<Json> Work;inline uint32_t Pid=0,RequestPid=0;
inline bool Finished=false;inline Clock::time_point Next{},Started{},FinishedAt{};
inline std::string Stage="Starting Dragonwilds";
inline std::vector<std::string> Tips;
inline void SetTips(const Json& rows){
    Tips.clear();if(!rows.is_array())return;
    for(const auto& row:rows)if(row.value("Enabled",false)&&row.value("Present",false)&&row.contains("Metadata")){
        const auto& metadata=row["Metadata"];if(!metadata.contains("tips")||!metadata["tips"].is_array())continue;const auto name=metadata.value("name",row.value("Name",std::string{}));
        for(const auto& tip:metadata["tips"]){if(Tips.size()>=1024)return;if(!tip.is_string())continue;const auto value=tip.get<std::string>();if(!value.empty()&&value.size()<=512&&std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))Tips.push_back(name+": "+value);}
    }
}
inline std::string Tip(double elapsed){return Tips.empty()?std::string{}:Tips[size_t(std::max(0.,elapsed)/8.)%Tips.size()];}
inline bool Presentation(bool external,bool dedicated,bool gameOverlay,bool nativeOverlay,bool startup){return external&&!dedicated&&((gameOverlay&&nativeOverlay)||startup);}
inline std::string Label(){const auto tip=Tip(std::chrono::duration<double>(Clock::now()-Started).count());return Stage+(tip.empty()?std::string{}:"\n"+tip);}
inline bool Visible(){return Pid&&(!Finished||Clock::now()-FinishedAt<std::chrono::milliseconds(500));}
inline void Apply(const Json& doc){
    if(!doc.is_object())return;
    if(doc.value("Complete",false)&&doc.value("FrontendReady",true)){Finished=true;FinishedAt=Clock::now();Stage="Ready";return;}
    Stage=doc.value("Complete",false)?"Waiting for the game menu":"Starting Dragonwilds";
    const auto rows=doc.find("Checkpoints");if(rows==doc.end()||!rows->is_array()||rows->empty())return;
    const auto step=rows->back().value("Stage",std::string{});
    if(doc.value("Complete",false))return;
    if(step.starts_with("config"))Stage="Loading settings";
    else if(step.starts_with("signature")||step.starts_with("offsets"))Stage="Preparing game compatibility";
    else if(step.starts_with("GameInstanceInit")||step.starts_with("data registrar")||step.starts_with("InitCore"))Stage="Registering game content";
    else if(step.starts_with("construct")||step.starts_with("PostEngineInit"))Stage="Preparing RuneSchema";
    else if(step.starts_with("ERROR"))Stage="Startup needs attention — check RuneSchema.log";
}
inline void Poll(bool enabled,uint32_t pid,const std::function<bool(const Json&,Json&)>& read){
    if(!enabled)pid=0;
    if(pid!=Pid){Pid=pid;Finished=false;Next={};Started=Clock::now();Stage="Starting Dragonwilds";}
    if(Work.valid()&&Work.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{const auto doc=Work.get();if(pid&&RequestPid==pid)Apply(doc);}catch(...){Stage="Waiting for the game";}}
    if(!pid||Finished||!read||Work.valid()||Clock::now()<Next)return;
    Next=Clock::now()+std::chrono::seconds(1);RequestPid=pid;
    Work=std::async(std::launch::async,[read]{Json result;if(!read({{"Action","StartupStats"}},result))return Json::object();return result;});
}
inline void Shutdown(){if(Work.valid())try{Work.get();}catch(...){}Pid=0;Finished=false;}
}
