#pragma once
#include "Core/HelpyDeliveryFlags.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyMenu.h"
#include "Generator/SpawnToolsPanel.h"
#include "imgui.h"
#include <chrono>
// Settings > Deliveries: the player's own opt-out per character, and the delivery pauses mods
// have set. The core refuses player-driven deliveries from the same folder, so this page only
// explains and records choices; it is never the enforcement point.
namespace PS::DeliveryFlagsPanel {
namespace F=PS::HelpyDeliveryFlags;
inline F::Snapshot Snapshot;
inline std::string Status;
inline double NextRead=0;
inline std::filesystem::path Folder(){return RuneSchemaData::SharedDirectory("HELPY")/"flags";}
inline int64_t Now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline void Refresh(bool now=false){
    if(!now&&ImGui::GetTime()<NextRead)return;
    NextRead=ImGui::GetTime()+2.0;
    try{Snapshot=F::Read(Folder(),F::Signature::DefaultVerifier(),Now());}
    catch(const std::exception& error){Status=std::string("Could not read delivery flags: ")+error.what();}
}
inline void Apply(const std::function<void()>& change,const char* done){
    try{change();Status=done;}catch(const std::exception& error){Status=error.what();}
    NextRead=0; // Re-read next frame: the caller may still be iterating the current snapshot.
}
inline const char* Trust(F::ModStatus::Trust trust){
    switch(trust){
    case F::ModStatus::Trust::Registry:return "Key verified (known to Helpy)";
    case F::ModStatus::Trust::FirstSeen:case F::ModStatus::Trust::Pinned:return "Key pinned on first use";
    case F::ModStatus::Trust::Mismatch:return "Key changed: flags ignored";
    default:return "Registration invalid";
    }
}
inline std::string Short(const std::string& guid){return guid.size()>8?guid.substr(0,8)+"...":guid;}
inline void Characters(){
    ImGui::SeparatorText("Your characters");
    ImGui::TextWrapped("Turn off Helpy deliveries for a character if you want it to play without them. This applies to deliveries you make to yourself; a server host can still give you items.");
    struct Row{std::string Guid,Name;};std::vector<Row> rows;
    const auto& players=SpawnToolsPanel::UiSnapshot.contains("Players")&&SpawnToolsPanel::UiSnapshot["Players"].is_array()?SpawnToolsPanel::UiSnapshot["Players"]:nlohmann::json::array();
    for(const auto& player:players)if(player.is_object()&&player.value("IsLocal",false)){
        const auto guid=F::NormalizeGuid(player.value("Guid",std::string{}));if(!guid.empty())rows.push_back({guid,player.value("Name",std::string("Your character"))});
    }
    for(const auto& [guid,row]:Snapshot.State["characters"].items())
        if(std::none_of(rows.begin(),rows.end(),[&](const Row& known){return known.Guid==guid;}))rows.push_back({guid,row.value("name",std::string("Saved character"))});
    if(rows.empty()){ImGui::TextDisabled("Load a world to see your current character here.");return;}
    for(const auto& row:rows){
        ImGui::PushID(row.Guid.c_str());bool allowed=F::UserAllows(Snapshot.State,row.Guid);
        const auto label="Allow Helpy deliveries to "+row.Name+" ("+Short(row.Guid)+")";
        if(HelpyImGuiTheme::Toggle(label.c_str(),&allowed))
            Apply([&]{F::SetUserDeliveries(Folder(),row.Guid,allowed,Now(),row.Name);},allowed?"Deliveries are on for this character.":"Deliveries are off for this character.");
        ImGui::PopID();
    }
}
inline void Mods(){
    ImGui::SeparatorText("Pauses set by mods");
    ImGui::TextWrapped("A mod such as the Grand Exchange can pause deliveries to a character or world it manages. Only that mod can lift its pause. You can stop honouring a mod; the mod is told, and may treat the character as not taking part.");
    if(Snapshot.Mods.empty()){ImGui::TextDisabled("No mod has registered delivery flags.");return;}
    for(const auto& mod:Snapshot.Mods){
        ImGui::PushID(mod.Id.c_str());
        ImGui::TextUnformatted(mod.Name.c_str());ImGui::SameLine();ImGui::TextDisabled("%s",Trust(mod.Key));
        if(!mod.Problem.empty())ImGui::TextWrapped("%s",mod.Problem.c_str());
        if(mod.Rejected)ImGui::TextDisabled("%zu flag(s) failed their signature check and were ignored.",mod.Rejected);
        for(const auto* list:{&Snapshot.Active,&Snapshot.Declined})for(const auto& flag:*list)if(flag.Mod==mod.Id)
            ImGui::BulletText("%s %s: %s%s",flag.Target=="world"?"World":"Character",Short(flag.Id).c_str(),flag.Message.empty()?"Deliveries paused.":flag.Message.c_str(),list==&Snapshot.Declined?" (not honoured)":"");
        if(mod.Trusted()){
            bool allow=mod.AllowPause;
            if(HelpyImGuiTheme::Toggle("Let this mod pause deliveries",&allow))
                Apply([&]{F::SetModAllowed(Folder(),mod.Id,allow,Now());},allow?"This mod's pauses are honoured again.":"This mod's pauses are no longer honoured. The mod can see this choice.");
        }
        if(mod.Key==F::ModStatus::Trust::Mismatch&&mod.Problem.find("Reset")!=std::string::npos){
            if(HelpyImGuiTheme::SoftButton("Trust the new key"))Apply([&]{F::ResetKey(Folder(),mod.Id,Now());},"The new key will be pinned. The reset is recorded.");
        }
        ImGui::PopID();ImGui::Spacing();
    }
}
inline void Render(){
    Refresh();
    if(!HelpyMenu::BeginBody("Deliveries body",0)){HelpyMenu::EndBody();return;}
    ImGui::TextWrapped("Helpy deliveries you make to your own character are on by default.");
    Characters();Mods();
    for(const auto& problem:Snapshot.Problems)ImGui::TextWrapped("%s",problem.c_str());
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    HelpyMenu::EndBody();
}
}
