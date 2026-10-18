#pragma once
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/ToolRequest.h"
namespace PS::RuntimeConnectionPanel {
inline nlohmann::json Snapshot=nlohmann::json::object();
inline std::chrono::steady_clock::time_point Received{},Next{};
inline uint64_t Generation=0;
#if defined(HELPY_PREVIEW)
inline std::string PreviewRole="standalone";
#endif
struct View {std::string Role="unknown",Session="Not reported",Health="Not reported";bool Fresh=false,Host=false,Available=false,Waiting=false;int64_t AgeMs=-1;int Players=-1;};
inline View Read(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){
    View view;if(Snapshot.empty()||Generation!=SpawnToolRequests::CurrentGeneration())return view;
    view.Available=Snapshot.value("Available",false);view.Waiting=Snapshot.value("Waiting",false);
    if(Snapshot.contains("AgeMs")&&Snapshot["AgeMs"].is_number_integer()&&Snapshot["AgeMs"].get<int64_t>()>=0)
        view.AgeMs=Snapshot["AgeMs"].get<int64_t>()+std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(now-Received).count());
    view.Fresh=view.Available&&Snapshot.value("Fresh",false)&&view.AgeMs>=0&&view.AgeMs<=12000;
    if(view.Fresh){const auto role=Snapshot.value("Role",std::string{});
        if(role=="listen-server"){view.Role=role;view.Session="Co-op host";view.Host=true;}
        else if(role=="client"){view.Role=role;view.Session="Multiplayer client";}
        else if(role=="dedicated-server"){view.Role=role;view.Session="Dedicated server";}
        else if(role=="standalone"){view.Role=role;view.Session="Single player";}
        else view.Fresh=false;
    }
    if(view.Fresh&&Snapshot.contains("KnownPlayers")&&Snapshot["KnownPlayers"].is_number_integer()){
        const auto count=Snapshot["KnownPlayers"].get<int>();if(count>=0&&count<=256)view.Players=count;
    }
    view.Health=!view.Available?"World tools unavailable":!view.Fresh?"Awaiting fresh native observation":view.Waiting?"Native runtime responding · operation pending":"Native runtime responding";
    return view;
}
inline void Poll(){
    const auto now=std::chrono::steady_clock::now();const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Generation!=generation){Snapshot=nlohmann::json::object();Next={};Generation=generation;}
    if(now<Next||!RuneSchemaSettingsPanel::Remote)return;Next=now+std::chrono::seconds(2);
    try{nlohmann::json reply;if(RuneSchemaSettingsPanel::Remote({{"Action","NetworkStatus"}},reply)&&reply.is_object()
        &&reply.value("Generation",uint64_t{})==generation){Snapshot=std::move(reply);Received=now;}}
    catch(...){} // A diagnostic failure must not interrupt gameplay or UI.
}
inline void Render(){
    const auto view=Read();ImGui::SeparatorText("Connection & health");
    ImGui::Text("Session: %s",view.Session.c_str());ImGui::TextWrapped("Health: %s",view.Health.c_str());
    if(view.AgeMs>=0)ImGui::Text("Native observation age: %.1f s",double(view.AgeMs)/1000);
    if(view.Players>=0)ImGui::Text("Known connected players: %d",view.Players);
    ImGui::TextDisabled("Network RTT / packet loss: not measured");
    ImGui::TextWrapped("Role and freshness come from read-only game-thread observations. Runtime responsiveness is not a network latency or synchronization guarantee.");
}
}
