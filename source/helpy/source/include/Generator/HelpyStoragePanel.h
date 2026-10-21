#pragma once
#include "Core/HelpyStorage.h"
#include "Generator/ItemGridPicker.h"
#include "Runtime/HostServices.h"
#include <future>
#include <commdlg.h>
namespace PS::HelpyStoragePanel {
using Json=nlohmann::json;
inline std::string Character,Message,Selected;
inline Json Snapshot={{"Items",Json::array()}};
inline std::future<Json> Worker;
inline char Search[257]{};
inline std::filesystem::path OpenedFile;
inline void Load(const std::filesystem::path& file={}){
    if(Worker.valid())return;const auto character=Character;OpenedFile=file;
    const auto directory=HostServices::StateDirectory()/"storage";
    Worker=std::async(std::launch::async,[character,file,directory]{
        try{return file.empty()?HelpyStorage::Store(directory,character).Rows():HelpyStorage::ReadFile(file,character);}
        catch(const std::exception& error){return Json{{"Error",error.what()},{"CharacterGuid",character},{"Items",Json::array()}};}
    });
}
inline void Poll(){
    if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    auto value=Worker.get();if(value.value("CharacterGuid",std::string{})==Character){
        Message=value.value("Error",std::string{});Snapshot=std::move(value);
    }
}
inline void Render(const std::string& guid){
    Poll();const auto character=HelpyPersonalization::Guid(guid);
    if(character!=Character){Character=character;Snapshot={{"Items",Json::array()}};Message.clear();Selected.clear();OpenedFile.clear();Search[0]=0;}
    if(Character.empty()){ImGui::TextDisabled("Open a character save with a stable GUID.");return;}
    if(!Worker.valid()&&!Snapshot.contains("CharacterGuid"))Load();
    ImGui::BeginDisabled(Worker.valid());
    if(HelpyImGuiTheme::SoftButton("Refresh"))Load(OpenedFile);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Open storage file…")){
        wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();
        dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Helpy Storage (*.json)\0*.json\0\0";
        dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&dialog))Load(std::filesystem::path(file));
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##storage-search","Search stored items…",Search,sizeof(Search));
    ImGui::TextDisabled("Stored items only · native recovery is not active yet");
    if(Worker.valid())ImGui::TextDisabled("Reading storage…");
    if(!Message.empty())ImGui::TextWrapped("%s",Message.c_str());
    const auto query=HelpyPersonalization::Fold(Search);std::vector<const Json*> rows;
    for(const auto& row:Snapshot.at("Items")){
        const auto text=HelpyPersonalization::Fold(row.value("Name",std::string{})+" "+row.value("AssetPath",std::string{})+" "+row.value("PersistenceID",std::string{}));
        if(query.empty()||text.find(query)!=text.npos)rows.push_back(&row);
    }
    ImGui::TextDisabled("%zu stored entries",rows.size());const auto& theme=HelpyImGuiTheme::Current;
    if(ImGui::BeginChild("##helpy-storage-items",{0,theme.Y(360)},true)){
        const float gap=ImGui::GetStyle().ItemSpacing.x,available=std::max(1.f,ImGui::GetContentRegionAvail().x);
        const int columns=ItemBayLayout::Columns(available,theme.X(theme.gridMinCardWidth),gap,theme.itemGridColumns);
        const float width=std::max(1.f,(available-gap*(columns-1))/columns),height=theme.Y(theme.gridCardHeight);
        ImGuiListClipper clipper;clipper.Begin(int((rows.size()+columns-1)/columns),height+ImGui::GetStyle().ItemSpacing.y);
        while(clipper.Step())for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;++line)for(int column=0;column<columns;++column){
            const auto index=size_t(line*columns+column);if(index>=rows.size())break;if(column)ImGui::SameLine();
            const auto& row=*rows[index];const auto id=row.at("StorageId").get<std::string>();ImGui::PushID(id.c_str());
            const auto a=ImGui::GetCursorScreenPos();const ImVec2 b(a.x+width,a.y+height);
            if(ImGui::InvisibleButton("##stored-item",{width,height}))Selected=id;
            const bool hovered=ImGui::IsItemHovered();ItemGridPicker::DrawPlacard(row,a,b,Selected==id,hovered);
            if(HelpyImGuiTheme::TooltipReady("storage:"+id,hovered)){
                HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextWrapped("%s",ItemPresentation::Name(row).c_str());
                ImGui::Text("Quantity: %lld",static_cast<long long>(row.at("Count").get<int64_t>()));
                ImGui::Text("State: %s",row.at("StorageState").get_ref<const std::string&>().c_str());
                ImGui::TextWrapped("Restore requires confirmed native deletion and a verified native restore adapter.");
                HelpyImGuiTheme::EndDetailsTooltip();
            }
            if(ImGui::BeginPopupContextItem("##storage-menu")){
                PersonalizationPanel::Menu(row);
                if(ImGui::MenuItem("Copy item path"))ImGui::SetClipboardText(row.at("AssetPath").get_ref<const std::string&>().c_str());
                ImGui::EndPopup();
            }ImGui::PopID();
        }
        if(rows.empty()&&!Worker.valid()&&Message.empty())ImGui::TextDisabled("No recovered items for this character.");
    }ImGui::EndChild();
}
inline void Shutdown()noexcept{if(Worker.valid())try{(void)Worker.get();}catch(...){};}
}
