#pragma once
#include "Runtime/PluginRuntimeServices.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/MapPanel.h"
#include "Generator/JournalPanel.h"
namespace PS::DeveloperToolsPanel {
using Json=nlohmann::json;
inline Json Metadata=Json::object(),Report=Json::object();inline std::chrono::steady_clock::time_point NextCheck;inline int Section=0;inline uint64_t Sequence=0,Pending=0,Generation=0;inline std::string Status;
inline std::chrono::steady_clock::time_point Started;
inline bool Exchange(const Json& request,Json& reply){const auto* host=PluginRuntimeServices::Host;if(!host)return false;const auto text=request.dump();char output[4096]{};uint32_t size=0;if(host->CallService("RuneSchema.Helpy","helpy.developer-tools",text.c_str(),output,sizeof(output),&size)!=RS_PLUGIN_OK||!size||size>sizeof(output))return false;try{reply=Json::parse(output);return true;}catch(...){return false;}}
inline bool Loaded(){
#if defined(HELPY_PREVIEW)
 return true;
#endif
 const auto now=std::chrono::steady_clock::now();if(now>=NextCheck){NextCheck=now+std::chrono::seconds(1);Metadata=Json::object();Exchange({{"Op","read"}},Metadata);}return Metadata.value("Loaded",false);
}
inline void Submit(const char* command){Json plan;if(!Exchange({{"Op","prepare"},{"Command",command}},plan)){Status="Advanced Tooling is unavailable. Enable its matching Helpy plugin.";return;}auto request=plan.at("Request");const auto id=++Sequence;request["_Source"]="HelpyDeveloperTools";request["_RequestId"]=id;if(SpawnToolRequests::TrySubmit(std::move(request))){Pending=id;Started=std::chrono::steady_clock::now();Status="Waiting for the game's result.";}else Status="The runtime is busy; this request was not submitted.";}
inline void Poll(){const auto generation=SpawnToolRequests::CurrentGeneration();if(generation!=Generation){Generation=generation;Pending=0;Report=Json::object();Status.clear();}if(!Pending)return;Json result;if(SpawnToolRequests::TakeCompleted(Pending,result)){Pending=0;Status=result.value("Status",std::string("No confirmed result."));if(result.contains("DeveloperContracts"))Report=std::move(result["DeveloperContracts"]);}else if(std::chrono::steady_clock::now()-Started>std::chrono::seconds(65)){Pending=0;Status="No result was confirmed. The request was not retried.";}}
inline void Overview(){
    struct Capability { const char* Name; const char* Description; int Target; };
    constexpr std::array capabilities{
        Capability{"Items", "Browse items and prepare deliveries.", 1},
        Capability{"World", "Use the map and travel tools.", 2},
        Capability{"Spawns", "Manage temporary actors and encounters.", 3},
        Capability{"Buildings", "Use the available building tools.", 4},
        Capability{"Journal", "Browse journal and authoring references.", 5}
    };
    const auto& theme=HelpyImGuiTheme::Current;
    ImGui::SeparatorText("Available capabilities");
    const int columns=ImGui::GetContentRegionAvail().x>=theme.X(520)?2:1;
    if(ImGui::BeginTable("advanced-capabilities",columns,ImGuiTableFlags_SizingStretchSame)){
        for(const auto& capability:capabilities){
            ImGui::TableNextColumn();ImGui::PushID(capability.Target);
            ImGui::SeparatorText(capability.Name);ImGui::TextWrapped("%s",capability.Description);
            const auto label=std::string("Open ")+capability.Name;
            if(HelpyImGuiTheme::SoftButton(label.c_str()))Section=capability.Target;
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::MenuTabs["advanced-tooling/"+std::string(capability.Name)]=HelpyPreviewHooks::LastRect();
#endif
            ImGui::Spacing();ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Compatibility");
    ImGui::TextWrapped("Create a read-only report of the functions available in this game build.");
    if(HelpyImGuiTheme::SoftButton("Open compatibility"))Section=6;
}
inline void Compatibility(){
    const auto& theme=HelpyImGuiTheme::Current;
    ImGui::BeginDisabled(!SpawnToolRequests::IsAvailable()||Pending||SpawnToolRequests::IsWaiting());
    if(HelpyImGuiTheme::SoftButton(Pending?"Scanning...":"Scan compatibility"))Submit("inspect");
    ImGui::EndDisabled();HelpyMenu::NextControl("Copy report");
    ImGui::BeginDisabled(Report.empty());
    if(HelpyImGuiTheme::SoftButton("Copy report"))ImGui::SetClipboardText(Report.dump(2).c_str());
    ImGui::EndDisabled();
    ImGui::TextWrapped("Enter a world, then scan its loaded functions. This check changes no game data.");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Report.empty()){ImGui::TextDisabled("No compatibility report yet.");return;}
    const auto& functions=Report.at("Functions");const auto& classes=Report.at("Classes");
    ImGui::SeparatorText("Compatibility report");
    ImGui::Text("Functions: %zu  |  Loaded classes: %zu",functions.size(),classes.size());
    if(ImGui::BeginTable("developer-contracts",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY,{0,std::max(theme.Y(80),ImGui::GetContentRegionAvail().y)})){
        ImGui::TableSetupColumn("Function",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Implementation",ImGuiTableColumnFlags_WidthFixed,theme.X(130));
        ImGui::TableSetupColumn("Parameters",ImGuiTableColumnFlags_WidthFixed,theme.X(90));ImGui::TableHeadersRow();
        ImGuiListClipper clip;clip.Begin(int(functions.size()));
        while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){
            const auto& row=functions[size_t(i)];ImGui::TableNextRow();ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.at("Path").get_ref<const std::string&>().c_str());ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.value("Native",false)?"Native":"Blueprint");ImGui::TableNextColumn();
            ImGui::Text("%zu",row.at("Parameters").size());
        }
        ImGui::EndTable();
    }
}
inline void RenderContent(){
    switch(Section){
    case 0:Overview();break;
    case 1:SpawnToolsPanel::Render(SpawnToolsPanel::View::Items);break;
    case 2:MapPanel::Render();break;
    case 3:SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);break;
    case 4:SpawnToolsPanel::Render(SpawnToolsPanel::View::Buildings);break;
    case 5:JournalPanel::Render();break;
    default:Compatibility();break;
    }
}
inline void Render(){
    if(!Loaded()){ImGui::TextWrapped("Enable the Advanced Tooling capability in Helpy's plugins.");return;}
    Poll();
    ImGui::TextDisabled("Advanced Tooling");
    ImGui::TextWrapped("Optional game tools and compatibility checks. Actions follow your current connection and permissions.");
    constexpr std::array labels{"Overview","Items","World","Spawns","Buildings","Journal","Compatibility"};
    HelpyMenu::Tabs("developer-tools",labels,Section);
    if(HelpyMenu::BeginBody("Advanced tooling body",Section))RenderContent();
    HelpyMenu::EndBody();
}
}
