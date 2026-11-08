#pragma once
#include "Generator/ToolRequest.h"
#include "Generator/HelpyMenu.h"
#include "Generator/HelpyImGuiTheme.h"
#include "NativeServerAdmin.h"

namespace PS::ServerPlayersPanel {
using Json=nlohmann::json;
inline Json Snapshot=Json::object(),Target=Json::object();
inline std::string Status,Operation;
inline uint64_t Sequence=0x5352000000000000ull,Pending=0,Generation=~uint64_t{};
inline std::chrono::steady_clock::time_point Received{},Started{};
inline bool Attempted=false;
inline int Tab=0;
inline bool Fresh(){return Generation==SpawnToolRequests::CurrentGeneration()&&!Snapshot.empty()&&std::chrono::steady_clock::now()-Received<std::chrono::seconds(12);}
inline void Submit(std::string operation,const Json& target=Json::object()){
    if(Pending)return;
    Json request={{"Action","NativeServerAdmin"},{"Operation",operation},{"Generation",SpawnToolRequests::CurrentGeneration()},{"_Source","HelpyStandalone"},{"_RequestId",++Sequence}};
    if(operation!="Read"){
        const bool access=operation=="Authorize"||operation=="Unauthorize"||operation=="AuthorizeAll"||operation=="AutoAuthorize";
        if(!Fresh()||!Snapshot.value(access?"CanAuthorize":"CanManage",false)){Status="Refresh the current server and player permissions first.";return;}
        request["WorldToken"]=Snapshot.at("WorldToken");
        if(operation=="AutoAuthorize")request["Enabled"]=target.at("Enabled");
        else if(operation!="AuthorizeAll"){request["PlayerGuid"]=target.at("Guid");request["TargetToken"]=target.at("TargetToken");}
    }
    if(SpawnToolRequests::TrySubmit(request)){Operation=std::move(operation);Pending=Sequence;Started=std::chrono::steady_clock::now();Status="Waiting for the server's result…";}
    else Status="Runtime is busy. This request was not submitted.";
}
inline void Poll(){
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Generation!=generation){Snapshot=Json::object();Target=Json::object();Pending=0;Attempted=false;Generation=generation;Status.clear();}
    if(!Pending)return;Json result;
    if(SpawnToolRequests::TakeCompleted(Pending,result)){
        Pending=0;Status=result.value("Status",std::string("No confirmed administration result."));Snapshot=Json::object();
        const auto found=result.find("ServerControl");
        if(found!=result.end()&&NativeServerAdmin::Snapshot(*found)){
            Snapshot=*found;Received=std::chrono::steady_clock::now();
            const auto change=Snapshot.find("Change");
            if(change!=Snapshot.end()&&change->is_object()&&change->contains("Confirmed")&&(*change)["Confirmed"].is_boolean()&&!(*change)["Confirmed"].get<bool>()){
                Snapshot["CanManage"]=false;Snapshot["Reason"]="Role change unconfirmed. Refresh current roles before another change.";
            }
        }
    }else if(std::chrono::steady_clock::now()-Started>std::chrono::seconds(65)){Pending=0;Snapshot=Json::object();Status="Server result unconfirmed. Refresh the current roles before retrying; no automatic retry was made.";}
}
inline void Render(){
    Poll();
    if(!Attempted&&SpawnToolRequests::IsAvailable()){Attempted=true;Submit("Read");}
    HelpyMenu::Tabs("server-player-access",std::array{"RuneSchema access","Native Admin"},Tab,false);
    ImGui::TextWrapped(Tab==0?"Manage saved RuneSchema access for connected players.":"Manage the game's Admin role for connected players.");
    ImGui::BeginDisabled(Pending||SpawnToolRequests::IsWaiting());
    if(HelpyImGuiTheme::SoftButton("Refresh players & roles"))Submit("Read");
    ImGui::EndDisabled();
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Snapshot.empty()){ImGui::TextDisabled("Connect to a RuneSchema world to read server controls.");return;}
    const bool fresh=Fresh(),allowed=fresh&&Snapshot.value("CanManage",false);
    const bool canAuthorize=fresh&&Snapshot.value("CanAuthorize",false);
    if(Tab==0){
    ImGui::BeginDisabled(!canAuthorize||Pending||SpawnToolRequests::IsWaiting());
    if(HelpyImGuiTheme::SoftButton("Authorize all connected"))Submit("AuthorizeAll");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringButtons["server-authorize-all"]=HelpyPreviewHooks::LastRect();
#endif
    bool automatic=Snapshot.value("AutoAuthorize",false);
    if(HelpyImGuiTheme::Toggle("Automatically authorize connecting players",&automatic))Submit("AutoAuthorize",{{"Enabled",automatic}});
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringButtons["server-authorize-automatic"]=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)||ImGui::IsItemFocused())
        HelpyImGuiTheme::Tooltip("Grants saved access to currently connected players and future arrivals. Access covers items, map travel and temporary spawns within server limits.");
    if(automatic)ImGui::TextDisabled("Turn off automatic authorization before removing saved access.");
    }
    const auto reason=Snapshot.value("Reason",std::string{});if(Tab==1&&!reason.empty())ImGui::TextWrapped("%s",reason.c_str());
    if(!fresh)ImGui::TextDisabled("Refresh expired roles before changing admin rights.");
    if(Snapshot.contains("Change"))ImGui::TextWrapped("The game manages saved admin rights. This view confirms the current session's role.");
    const auto& players=Snapshot.at("Players");
    if(players.empty())ImGui::TextDisabled("No active player characters are available.");
    for(const auto& player:players){
        if(!player.is_object()||!player.contains("Guid")||!player.contains("TargetToken"))continue;
        const auto name=player.value("Name",std::string("Player")),guid=player.at("Guid").get<std::string>();
        const auto role=player.value("RoleStatus",Json::object());const bool owner=player.value("Owner",false),known=role.value("NativeKnown",false),admin=role.value("Role",std::string{})=="admin";
        ImGui::PushID(guid.c_str());ImGui::SeparatorText(name.c_str());
        ImGui::Text("Role: %s",owner?"Owner":known?(admin?"Admin":"Player"):"Unknown");
        if(Tab==0){
        const bool authorized=player.value("Authorized",false);
        ImGui::BeginDisabled(!canAuthorize||owner||(Snapshot.value("AutoAuthorize",false)&&authorized)||Pending||SpawnToolRequests::IsWaiting());
        if(HelpyImGuiTheme::SoftButton(authorized?"Remove RuneSchema access":"Authorize player"))Submit(authorized?"Unauthorize":"Authorize",player);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::AuthoringButtons["server-authorize-"+guid]=HelpyPreviewHooks::LastRect();
#endif
        ImGui::EndDisabled();
        const auto account=player.value("PlatformAccountName",std::string{}),platform=player.value("Platform",std::string("Unknown"));
        ImGui::TextWrapped("%s | %s | Player ID: %d",account.empty()?"Account name unavailable":account.c_str(),platform.c_str(),player.value("PlayerId",-1));
        ImGui::TextWrapped("Character: %s",guid.c_str());
        ImGui::PopID();continue;
        }
        ImGui::BeginDisabled(!allowed||!known||owner||Pending||SpawnToolRequests::IsWaiting());
        if(HelpyImGuiTheme::SoftButton(admin?"Revoke Admin":"Grant Admin")){Target=player;Operation=admin?"Revoke":"Grant";ImGui::OpenPopup("Change native admin role?");}
        ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::AuthoringButtons["server-admin-"+guid]=HelpyPreviewHooks::LastRect();
#endif
        if(HelpyImGuiTheme::BeginCenteredModal("Change native admin role?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped("%s Admin for %s?",Operation.c_str(),Target.value("Name",std::string("Player")).c_str());
            ImGui::TextWrapped("The server rechecks ownership and this player connection before applying the change.");
            ImGui::BeginDisabled(!allowed||Pending||SpawnToolRequests::IsWaiting());
            if(HelpyImGuiTheme::SoftButton("Confirm role change")){Submit(Operation,Target);ImGui::CloseCurrentPopup();}ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::AuthoringButtons["server-admin-confirm"]=HelpyPreviewHooks::LastRect();
#endif
            if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}
}
