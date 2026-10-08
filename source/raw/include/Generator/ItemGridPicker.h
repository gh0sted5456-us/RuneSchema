#pragma once
// Compatibility namespace retained so existing authoring panels do not change.
// UE4SS settings deliberately use compact rows; only the F2 menu has icon tiles.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>
namespace PS::ItemGridPicker {
using json=nlohmann::json;
struct State {int Page=0;int PageSize=50;std::string LastFilter;};
struct Result {bool Changed=false;std::string SelectedTitle;const json* SelectedRow=nullptr;size_t MatchCount=0;};
inline std::string Lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline const json* Find(const json& catalog,const char* selected) {
    if(!selected||!*selected||!catalog.contains("Items")||!catalog["Items"].is_array())return nullptr;
    for(const auto& row:catalog["Items"])if(row.value("Path",std::string{})==selected)return &row;
    return nullptr;
}
inline std::string Title(const json& catalog,const std::string& path){const auto* row=Find(catalog,path.c_str());return row?row->value("Name",path):path;}
inline void PageSizeControl(State& state) {
    const char* sizes[]{"25 rows","50 rows","100 rows"};int choice=state.PageSize==25?0:state.PageSize==100?2:1;
    if(ImGui::Combo("Page size",&choice,sizes,3)){state.PageSize=choice==0?25:choice==1?50:100;state.Page=0;}
}
inline Result Render(const char* id,const json& catalog,char* filter,size_t filterCapacity,char* selected,size_t selectedCapacity,State& state) {
    Result out;ImGui::PushID(id);
    if(!filter||!filterCapacity||!selected||!selectedCapacity){ImGui::TextDisabled("Item picker buffer unavailable.");ImGui::PopID();return out;}
    ImGui::InputTextWithHint("Search items","Name, path, internal name or persistence ID",filter,filterCapacity);
    const auto query=Lower(filter);if(query!=state.LastFilter){state.LastFilter=query;state.Page=0;}
    PageSizeControl(state);std::vector<const json*> rows;
    if(catalog.contains("Items")&&catalog["Items"].is_array())for(const auto& row:catalog["Items"]) {
        const auto path=row.value("Path",std::string{}),name=row.value("Name",path);
        if(query.empty()||Lower(name+" "+path+" "+row.value("InternalName",std::string{})+" "+row.value("PersistenceID",std::string{})).find(query)!=std::string::npos)rows.push_back(&row);
    }
    out.MatchCount=rows.size();const int pages=std::max(1,(static_cast<int>(rows.size())+state.PageSize-1)/state.PageSize);
    state.Page=std::clamp(state.Page,0,pages-1);
    if(ImGui::Button("Previous")&&state.Page>0)--state.Page;
    ImGui::SameLine();
    if(ImGui::Button("Next")&&state.Page+1<pages)++state.Page;
    ImGui::SameLine();
    ImGui::Text("Page %d / %d | %zu matches",state.Page+1,pages,rows.size());
    const size_t first=static_cast<size_t>(state.Page*state.PageSize),last=std::min(rows.size(),first+static_cast<size_t>(state.PageSize));
    ImGui::BeginChild("Rows",ImVec2(0,260),true);
    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(last-first));
    while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
        const auto& row=*rows[first+static_cast<size_t>(i)];const auto path=row.value("Path",std::string{}),name=row.value("Name",path);
        ImGui::PushID(path.c_str());const bool fits=path.size()<selectedCapacity;
        ImGui::BeginDisabled(!fits);
        if(ImGui::Selectable(name.c_str(),path==selected)&&fits){std::snprintf(selected,selectedCapacity,"%s",path.c_str());out.Changed=true;}
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted(path.c_str());if(!fits)ImGui::TextUnformatted("Path exceeds the field capacity; it will not be truncated.");ImGui::EndTooltip();}
        ImGui::PopID();
    }
    ImGui::EndChild();out.SelectedRow=Find(catalog,selected);out.SelectedTitle=out.SelectedRow?out.SelectedRow->value("Name",std::string(selected)):selected;
    ImGui::TextWrapped("Selected: %s",out.SelectedTitle.empty()?"None":out.SelectedTitle.c_str());ImGui::PopID();return out;
}
}
