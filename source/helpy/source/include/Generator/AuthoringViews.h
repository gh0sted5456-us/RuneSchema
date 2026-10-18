#pragma once
#include "Generator/AuthoringClient.h"
#include <imgui.h>
#include <array>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

namespace PS::AuthoringViews {
using Json=nlohmann::json;
inline bool TraceControls(Json& options){
    bool changed=false;
    int seconds=options.value("Seconds",30),events=options.value("MaxEvents",1024),mask=options.value("Categories",15);
    changed|=ImGui::SliderInt("Recording seconds",&seconds,1,60);
    changed|=ImGui::SliderInt("Maximum events",&events,1,4096);
    constexpr const char* categories[]{"Equipment / appearance","Combat / magic","Interactions / inventory / crafting","Movement","Other reflected calls"};
    for(int i=0;i<5;++i){bool on=(mask&(1<<i))!=0;if(ImGui::Checkbox(categories[i],&on)){changed=true;if(on)mask|=1<<i;else mask&=~(1<<i);}}
    for(const auto& [key,label]:std::array{std::pair{"TraceSelectedObject","Trace selected object instead of player"},std::pair{"SuppressTicks","Suppress tick and animation-update calls"},std::pair{"CaptureParameters","Capture supported event parameters"}}){
        bool on=options.value(key,std::string_view(key)=="SuppressTicks");if(ImGui::Checkbox(label,&on)){options[key]=on;changed=true;}
    }
    char filter[129]{};std::snprintf(filter,sizeof(filter),"%s",options.value("Filter",std::string{}).c_str());
    if(ImGui::InputText("Function path contains",filter,sizeof(filter))){options["Filter"]=filter;changed=true;}
    if(changed){options["Seconds"]=seconds;options["MaxEvents"]=events;options["Categories"]=mask;}
    ImGui::TextWrapped("IncludeAny / ExcludeAny filter lists and EventCaptures field paths are editable in the JSONC below. Parameter or field capture requires a specific function filter. Native limits and validation are unchanged.");
    return changed;
}
inline void Object(const Json& object,ImGuiTextFilter& filter){
    const auto path=object.value("path",std::string{});ImGui::PushID(path.c_str());
    ImGui::TextWrapped("%s",path.c_str());
    if(ImGui::SmallButton("Copy object path"))ImGui::SetClipboardText(path.c_str());ImGui::SameLine();
    if(ImGui::SmallButton("Copy class path"))ImGui::SetClipboardText(object.value("class",std::string{}).c_str());
    if(object.contains("hierarchy")&&object["hierarchy"].is_array())for(const auto& level:object["hierarchy"]){
        ImGui::TextWrapped("%s",level.value("class",std::string{}).c_str());
        if(level.contains("fields")&&level["fields"].is_array())for(const auto& field:level["fields"]){
            const auto name=field.value("name",std::string{});
            const auto line=name+" : "+field.value("type",std::string{})+(field.contains("value")?" = "+field["value"].dump():" [metadata]");
            if(!filter.PassFilter(line.c_str()))continue;ImGui::TextWrapped("%s",line.c_str());
            if(field.value("objectReference",false)&&field.contains("value")&&field["value"].is_string()&&!field["value"].get_ref<const std::string&>().empty()){
                ImGui::PushID(name.c_str());if(ImGui::SmallButton("Inspect referenced object"))AuthoringClient::Submit("Object",{{"Path",field["value"]}});ImGui::PopID();
            }
        }
    }ImGui::PopID();
}
inline void Snapshot(const Json& report,ImGuiTextFilter& filter){
    ImGui::BeginChild("##captured-fields",ImVec2(0,260),true);
    if(report.contains("object")&&report["object"].is_object())Object(report["object"],filter);
    if(report.contains("components")&&report["components"].is_array())for(const auto& component:report["components"]){
        const auto path=component.value("path",std::string{});if(ImGui::TreeNode(path.c_str())){Object(component,filter);ImGui::TreePop();}
    }
    if(report.contains("controller")&&report["controller"].is_object()&&ImGui::TreeNode("Controller fields")){Object(report["controller"],filter);ImGui::TreePop();}
    if(report.contains("functions")&&ImGui::TreeNode("Function signatures")){for(const auto& fn:report["functions"]){const auto line=fn.dump();if(filter.PassFilter(line.c_str()))ImGui::TextWrapped("%s",line.c_str());}ImGui::TreePop();}
    ImGui::EndChild();
}
inline void SavedReport(const Json& report,bool world){
    ImGui::TextWrapped("%s",report.value("Coverage",std::string{}).c_str());
    if(report.contains("SaveFile"))ImGui::TextWrapped("Save: %s",report["SaveFile"].get_ref<const std::string&>().c_str());
    if(report.contains("Warnings"))for(const auto& warning:report["Warnings"])ImGui::TextWrapped("%s",warning.get_ref<const std::string&>().c_str());
    if(!report.contains("Rows")||!report["Rows"].is_array())return;
    static bool includeOther=false;static ImGuiTextFilter filter;
    bool changed=false;if(!world)changed=ImGui::Checkbox("Include other loaded assets",&includeOther);changed|=filter.Draw("Filter report rows");
    struct Rows {uint64_t Revision=~uint64_t{};std::string Filter;bool Include=false;std::vector<size_t> Visible;};
    static Rows caches[2];auto& cached=caches[world?1:0];
    if(changed||cached.Revision!=AuthoringClient::Revision||cached.Filter!=filter.InputBuf||cached.Include!=includeOther){
        cached.Revision=AuthoringClient::Revision;cached.Filter=filter.InputBuf;cached.Include=includeOther;cached.Visible.clear();
        for(size_t i=0;i<report["Rows"].size();++i){const auto& row=report["Rows"][i];
            if(!world&&!includeOther&&row.value("Origin",std::string{}).starts_with("Other loaded"))continue;
            if(filter.IsActive()&&!filter.PassFilter(row.dump().c_str()))continue;cached.Visible.push_back(i);}
    }
    ImGui::Text("Showing %zu of %zu entries.",cached.Visible.size(),report["Rows"].size());
    if(ImGui::BeginTable("##saved-report",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,ImVec2(0,260))){
        for(const auto* label:{"Display name","Internal name","PersistenceID",world?"Owner / state":"Location","Origin"})ImGui::TableSetupColumn(label);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(cached.Visible.size()));
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& row=report["Rows"][cached.Visible[i]];ImGui::TableNextRow();int column=0;
            for(const auto* key:{"DisplayName","InternalName","PersistenceID",world?"Owner":"Location","Origin"}){ImGui::TableSetColumnIndex(column++);
                auto value=row.contains(key)?row[key].is_string()?row[key].get<std::string>():row[key].dump():"Unresolved";
                if(world&&std::string_view(key)=="Owner"&&row.contains("State"))value+=" / "+row["State"].dump();
                ImGui::TextUnformatted(value.c_str());if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",value.c_str());}
        }ImGui::EndTable();
    }ImGui::TextDisabled("Display filters do not remove rows from exports.");
}
}
