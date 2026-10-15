#pragma once
#include "Core/ItemAuthoringDraft.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ToolRequest.h"
#include "Runtime/HostServices.h"
#include <shellapi.h>

namespace PS::ItemAuthoringPanel {
using namespace ItemAuthoring;
inline std::array<Draft,2> Drafts;
inline int Active=0;
inline uint64_t Sequence=0x7400000000000000ull,Generation=0;
inline void Submit(Draft& draft,Json request){
    const auto id=++Sequence;request["_RequestId"]=id;request["_Source"]="HelpyItemAuthoring";
    if(!SpawnToolRequests::TrySubmit(std::move(request)))throw std::runtime_error("Runtime is busy. Try again when the current action finishes.");
    draft.Pending=id;draft.SentRevision=draft.Revision;draft.Requested=std::chrono::steady_clock::now();draft.Error.clear();
}
inline void Poll(){
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Generation!=generation){Generation=generation;for(auto& draft:Drafts){draft.Pending=0;draft.Ready=false;draft.Prepared=Json::object();draft.ExportWanted=false;draft.InspectWanted=!draft.Source.empty();}}
    for(auto& draft:Drafts){
        Json receipt;
        if(draft.Pending&&SpawnToolRequests::TakeCompleted(draft.Pending,receipt)){
            draft.Pending=0;
            if(draft.Revision!=draft.SentRevision&&!receipt.contains("CloneSource"))continue;
            draft.Status=receipt.value("Status",std::string{});
            if(receipt.contains("CloneSource")&&receipt["CloneSource"].value("Source",std::string{})==draft.Source.value("Path",std::string{})){
                draft.Inspection=receipt["CloneSource"];draft.Ready=true;
            }else if(receipt.contains("CloneResult")){
                const auto& result=receipt["CloneResult"];
                if(result.value("State",std::string{})=="Prepared"){
                    draft.Prepared=result;
                    if(draft.ExportWanted)try{draft.ExportPath=Export(HostServices::ExportsDirectory()/"items",result).string();draft.Status="Exported JSONC: "+draft.ExportPath;}catch(const std::exception& error){draft.Error=error.what();}
                }else if(result.value("State",std::string{})!="Stopped"){
                    draft.Created=true;
                    if(result.value("Permanent",false)&&result.value("CompanionsReady",true)&&result.contains("Documents")){draft.Prepared=result;draft.Prepared["State"]="Prepared";}
                }
                draft.Acknowledged=false;
            }
            draft.ExportWanted=false;
        }
        if(draft.Pending&&std::chrono::steady_clock::now()-draft.Requested>std::chrono::seconds(30)){
            draft.Pending=0;draft.ExportWanted=false;draft.Error="No completion received. Check the game before retrying a creation.";
        }
        if(draft.InspectWanted&&!draft.Pending&&SpawnToolRequests::IsAvailable()&&!SpawnToolRequests::IsWaiting())try{
            Submit(draft,{{"Action","InspectClone"},{"Source",draft.Source.at("Path")}});draft.InspectWanted=false;
        }catch(const std::exception& error){draft.Error=error.what();}
    }
}
inline void Card(const char* id,const Json& row,const char* empty){
    const auto& theme=HelpyImGuiTheme::Current;const auto at=ImGui::GetCursorScreenPos();const ImVec2 size(ImGui::GetContentRegionAvail().x,theme.Y(68));
    ImGui::InvisibleButton(id,size);
    if(row.empty()){ImGui::GetWindowDrawList()->AddRect(at,ImVec2(at.x+size.x,at.y+size.y),ImGui::GetColorU32(theme.border),theme.X(5));ImGui::GetWindowDrawList()->AddText(ImVec2(at.x+8,at.y+22),ImGui::GetColorU32(theme.textDisabled),empty);}
    else {ItemGridPicker::DrawPlacard(row,at,ImVec2(at.x+size.x,at.y+size.y));ItemGridPicker::DragItem(row);}
}
inline void RenderDraft(Draft& draft,Mode mode,const Json& catalog,const std::string& player){
    const bool transmog=mode==Mode::Transmog;const auto& theme=HelpyImGuiTheme::Current;
    ImGui::TextWrapped("%s",transmog?"Source stats + compatible appearance = a new item. Drop into each slot, then create.":"Choose a base item, edit its native values, then create it or export RuneSchema JSONC.");
    ImGui::BeginDisabled(draft.Pending!=0);
    const int columns=ImGui::GetContentRegionAvail().x>=theme.X(650)?(transmog?3:2):1;
    if(ImGui::BeginTable("item-authoring-flow",columns,ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextColumn();ImGui::BeginChild("source",ImVec2(0,theme.Y(220)),true);
        ImGui::SeparatorText("Source item");Card("source-card",draft.Source,"Drop source item here");
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::AuthoringSource=HelpyPreviewHooks::LastRect();
#endif
        if(ImGui::BeginDragDropTarget()){if(auto row=ItemGridPicker::AcceptItemDrop(draft.Error))draft.SetSource(*row);ImGui::EndDragDropTarget();}
        const auto* selected=ItemGridPicker::Find(catalog,ItemGridPicker::Bay.Selected);
        ImGui::BeginDisabled(!selected);if(HelpyImGuiTheme::SoftButton("Use selected as source")&&selected)draft.SetSource(*selected);ImGui::EndDisabled();
        if(!draft.Source.empty()&&ImGui::SmallButton("Refresh source values")){draft.Ready=false;draft.InspectWanted=true;draft.Changed();}
        ImGui::TextWrapped("%s",draft.Ready?"Native values loaded. Browsing keeps this source.":draft.Source.empty()?"The source supplies stats and behavior.":"Reading native source values...");
        ImGui::EndChild();
        if(transmog){
            ImGui::TableNextColumn();ImGui::BeginChild("appearance",ImVec2(0,theme.Y(220)),true);
            ImGui::SeparatorText("Appearance item");Card("appearance-card",draft.Appearance,"Drop appearance item here");
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::AuthoringAppearance=HelpyPreviewHooks::LastRect();
#endif
            if(ImGui::BeginDragDropTarget()){if(auto row=ItemGridPicker::AcceptItemDrop(draft.Error)){draft.Appearance=*row;draft.Changed();}ImGui::EndDragDropTarget();}
            ImGui::BeginDisabled(!selected);if(HelpyImGuiTheme::SoftButton("Use selected appearance")&&selected){draft.Appearance=*selected;draft.Changed();}ImGui::EndDisabled();
            if(!draft.Appearance.empty()&&ImGui::SmallButton("Clear appearance")){draft.Appearance=Json::object();draft.Changed();}
            ImGui::TextWrapped("Only the look is copied. The game checks that both items are compatible. The icon follows the appearance unless overridden.");
            ImGui::EndChild();
        }
        ImGui::TableNextColumn();ImGui::BeginChild("result",ImVec2(0,theme.Y(220)),true);
        ImGui::SeparatorText("New item");auto preview=draft.Source;
        if(!preview.empty()){if(draft.Name[0])preview["Name"]=draft.Name;if(draft.Icon[0])preview["Icon"]=draft.Icon;else if(transmog&&!draft.Appearance.empty())preview["Icon"]=draft.Appearance.value("Icon",preview.value("Icon",std::string{}));}
        Card("result-card",preview,"Choose a source to begin");
        ImGui::TextUnformatted("Display name");ImGui::SetNextItemWidth(-1);if(ImGui::InputText("##name",draft.Name,sizeof(draft.Name)))draft.Changed();
        if(ImGui::CollapsingHeader("Custom inventory icon")){ImGui::SetNextItemWidth(-1);if(ImGui::InputText("##icon",draft.Icon,sizeof(draft.Icon)))draft.Changed();
            if(ImGui::BeginDragDropTarget()){if(auto row=ItemGridPicker::AcceptItemDrop(draft.Error)){const auto icon=row->value("Icon",std::string{});if(icon.empty())draft.Error="This item has no resolved icon.";else{std::snprintf(draft.Icon,sizeof(draft.Icon),"%s",icon.c_str());draft.Changed();}}ImGui::EndDragDropTarget();}}
        ImGui::EndChild();ImGui::EndTable();
    }
    if(!transmog){
        ImGui::SeparatorText("Item values (JSONC)");
        ImGui::TextWrapped("Add a native field below, then change its value. Omitted fields inherit the source. Linked stats are exported with their matching /raw definitions.");
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##field-filter","Find a native field...",draft.Filter,sizeof(draft.Filter));
        if(ImGui::BeginChild("native-fields",ImVec2(0,theme.Y(120)),true)){
            if(draft.Ready&&draft.Inspection.contains("Fields"))for(const auto& field:draft.Inspection["Fields"]){
                const auto key=field.value("Name",std::string{});std::string hay=key,needle=draft.Filter;
                for(auto& c:hay)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));for(auto& c:needle)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if(!needle.empty()&&hay.find(needle)==hay.npos)continue;
                const bool editable=field.value("Editable",false);ImGui::BeginDisabled(!editable);ImGui::PushID(key.c_str());
                const auto label=key+" = "+(field.value("HasValue",false)?field["Value"].dump():"[inherited]");
                if(ImGui::Selectable(label.c_str()))try{
                    if(!field.value("HasValue",false))throw std::runtime_error("This field is inherited. Enter its schema-compatible value in the editor.");
                    auto values=ParseValues(draft.Editor.data());values[key]=field["Value"];const auto text=values.dump(2);
                    if(text.size()>=draft.Editor.size())throw std::runtime_error("Item values exceed the editor limit.");
                    std::snprintf(draft.Editor.data(),draft.Editor.size(),"%s",text.c_str());draft.Changed();
                }catch(const std::exception& error){draft.Error=error.what();}
                ImGui::PopID();ImGui::EndDisabled();
            }else ImGui::TextDisabled("Drop a source item to load its editable values.");
        }ImGui::EndChild();
        if(ImGui::InputTextMultiline("##values",draft.Editor.data(),draft.Editor.size(),ImVec2(-1,theme.Y(180))))draft.Changed();
        if(ImGui::SmallButton("Format JSONC"))try{const auto text=ParseValues(draft.Editor.data()).dump(2);if(text.size()>=draft.Editor.size())throw std::runtime_error("Formatted values exceed the editor limit.");std::snprintf(draft.Editor.data(),draft.Editor.size(),"%s",text.c_str());}catch(const std::exception& error){draft.Error=error.what();}
    }
    ImGui::SeparatorText("Create or export");
    ImGui::TextUnformatted("Mod folder");ImGui::SameLine();ImGui::SetNextItemWidth(theme.X(180));if(ImGui::InputText("##mod",draft.Mod,sizeof(draft.Mod))){draft.Id.clear();draft.Changed();}
    const bool eligible=draft.Ready&&(!transmog||!draft.Appearance.empty())&&!SpawnToolRequests::IsWaiting()&&Authoring::SafeModFolder(draft.Mod);
    ImGui::BeginDisabled(!eligible);
    if(HelpyImGuiTheme::SoftButton("Preview /assets JSONC")&&draft.Prepared.empty())try{Submit(draft,Request(draft,mode,player,true));}catch(const std::exception& error){draft.Error=error.what();}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Export JSONC"))try{
        if(!draft.Prepared.empty()){draft.ExportPath=Export(HostServices::ExportsDirectory()/"items",draft.Prepared).string();draft.Status="Exported JSONC: "+draft.ExportPath;}
        else {if(draft.Created)draft.Id.clear();Submit(draft,Request(draft,mode,player,true));draft.ExportWanted=true;}
    }catch(const std::exception& error){draft.Error=error.what();}
    ImGui::EndDisabled();
    if(!draft.ExportPath.empty()){if(ImGui::SmallButton("Open export folder"))ShellExecuteW(nullptr,L"open",std::filesystem::path(draft.ExportPath).c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    if(!draft.Prepared.empty()&&ImGui::CollapsingHeader("Prepared RuneSchema files",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::TextWrapped("These are the complete loader files. Edit values above and prepare again to update them.");
        for(const auto& [kind,document]:draft.Prepared.at("Documents").items())if(!document.empty()){
            ImGui::PushID(kind.c_str());ImGui::Text("/%s/%s.jsonc",kind.c_str(),draft.Prepared.at("InternalName").get<std::string>().c_str());
            const auto text=Jsonc(document);if(ImGui::SmallButton("Copy JSONC"))ImGui::SetClipboardText(text.c_str());
            ImGui::BeginChild("document",ImVec2(0,theme.Y(150)),true,ImGuiWindowFlags_HorizontalScrollbar);ImGui::TextUnformatted(text.c_str());ImGui::EndChild();ImGui::PopID();
        }
    }
    ImGui::TextWrapped("Export saves reusable files without creating or giving an item. Keep all exported folders together in your RuneSchema content mod.");
    if(ImGui::Checkbox("Save created item to mod folder",&draft.Permanent)){draft.Id.clear();draft.Changed();}
    ImGui::Checkbox("Give to selected player",&draft.Give);ImGui::SameLine();ImGui::SetNextItemWidth(theme.X(100));ImGui::InputInt("Quantity",&draft.Count);draft.Count=std::clamp(draft.Count,1,10000);
    ImGui::TextWrapped("Live creation uses RuneSchema's single-player authoring support. Test new items on a backed-up save.");
    ImGui::Checkbox("Use my backed-up test save",&draft.Acknowledged);
    const bool temporaryAllowed=draft.Permanent||!draft.Inspection.value("SoftDeleteField",std::string{}).empty();
    if(!temporaryAllowed)ImGui::TextWrapped("This source requires Save created item to mod folder.");
    ImGui::BeginDisabled(!eligible||!draft.Acknowledged||player.empty()||!temporaryAllowed||draft.Created);
    if(HelpyImGuiTheme::SoftButton(transmog?"Create transmog item":"Create item"))try{Submit(draft,Request(draft,mode,player,false));draft.Acknowledged=false;}catch(const std::exception& error){draft.Error=error.what();}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringCreate=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();if(draft.Created&&ImGui::SmallButton("Start another item")){draft.Id.clear();draft.Changed();}
    ImGui::EndDisabled();
    if(draft.Pending)ImGui::TextDisabled("Waiting for RuneSchema...");
    if(!draft.Error.empty())ImGui::TextWrapped("%s",draft.Error.c_str());if(!draft.Status.empty())ImGui::TextWrapped("%s",draft.Status.c_str());
}
inline void Render(const Json& catalog,uint64_t revision,const std::string& player){
    Poll();
    if(ImGui::BeginTabBar("Item authoring tabs")){
        const bool transmog=ImGui::BeginTabItem("Transmog");
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::TransmogTab=HelpyPreviewHooks::LastRect();
#endif
        if(transmog){Active=0;RenderDraft(Drafts[0],Mode::Transmog,catalog,player);ImGui::EndTabItem();}
        const bool creation=ImGui::BeginTabItem("Creation");
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::CreationTab=HelpyPreviewHooks::LastRect();
#endif
        if(creation){Active=1;RenderDraft(Drafts[1],Mode::Creation,catalog,player);ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
    ItemGridPicker::RenderShared(catalog,revision);
}
}
