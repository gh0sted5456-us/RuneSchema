#pragma once
#include "Core/ModMenuTemplates.h"
#include "Core/CacheBundleIO.h"
#include "Generator/HelpyMenu.h"
#include "Generator/HelpyTextInput.h"
#include <commdlg.h>

namespace PS::ModMenuTemplatePanel {
using Json=nlohmann::json;
inline Json Document,Plan;
inline std::string Message,Owner;
inline bool Requested=false;
inline char Name[81]{};
inline void Reset(){Document=Json{};Plan=Json{};Message.clear();Owner.clear();Requested=false;Name[0]=0;}
inline void Request(const Json& page){Reset();Owner=page.at("Id").get<std::string>();Requested=true;Message="Import a file or paste a shared template, then review the changes.";const auto name=page.at("Name").get<std::string>();strncpy_s(Name,name.c_str(),_TRUNCATE);}
inline void Import(const Json& page,const std::string& text){auto document=ModMenuTemplates::Parse(text);auto plan=ModMenuTemplates::Prepare(page,document);Document=std::move(document);Plan=std::move(plan);Message="Review the complete template before applying it.";}
inline std::string Text(const Json& value){return value.is_string()?value.get<std::string>():value.dump();}
inline void File(const Json& page,bool importing){
    wchar_t file[32768]=L"Character template.json";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Helpy character/settings template (*.json)\0*.json\0\0";dialog.lpstrDefExt=L"json";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(!(importing?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog)))return;
    if(importing)Import(page,CacheBundleIO::Read(file,ModMenuTemplates::MaximumBytes));
    else {CacheBundleIO::Publish(file,ModMenuTemplates::Export(page,Name).dump(2)+"\n");Message="Template exported.";}
}
inline void Render(const Json& page,bool available,const std::function<bool(Json)>& submit){
    if(Requested){Requested=false;ImGui::OpenPopup("Templates##mod-menu2");}
    if(!ImGui::IsPopupOpen("Templates##mod-menu2"))return;
    const auto& theme=HelpyImGuiTheme::Current;ImGui::SetNextWindowSize({theme.X(720),theme.Y(520)},ImGuiCond_Appearing);
    if(!HelpyImGuiTheme::BeginCenteredModal("Templates##mod-menu2",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    ImGui::TextWrapped("%s",page.at("Name").get_ref<const std::string&>().c_str());
    ImGui::TextWrapped("Share the settings declared by this mod. Import reviews changes before applying them; the mod retains its own character and save handling.");
    const auto action=[&](const char* label,auto fn){if(HelpyImGuiTheme::SoftButton(label))try{fn();}catch(const std::exception& error){Message=error.what();Plan=Json{};}};
    ImGui::BeginDisabled(!available);
    action("Import file",[&]{File(page,true);});HelpyMenu::NextControl("Paste template");
    action("Paste template",[&]{const auto* text=ImGui::GetClipboardText();const auto length=text?strnlen_s(text,ModMenuTemplates::MaximumBytes+1):0;ModMenuTemplates::Require(length&&length<=ModMenuTemplates::MaximumBytes,"Clipboard does not contain a bounded template.");Import(page,std::string(text,length));});
    ImGui::TextUnformatted("Template name");ImGui::SetNextItemWidth(-1);HelpyTextInput::InputText("##template-name",Name,sizeof(Name));
    action("Export file",[&]{File(page,false);});HelpyMenu::NextControl("Copy template");
    action("Copy template",[&]{const auto text=ModMenuTemplates::Export(page,Name).dump(2);ImGui::SetClipboardText(text.c_str());Message="Template copied.";});
    ImGui::EndDisabled();
    if(!Message.empty())ImGui::TextWrapped("%s",Message.c_str());
    const bool reviewed=Plan.is_object()&&!Plan.empty();const bool current=reviewed&&Owner==page.at("Id").get<std::string>()&&ModMenuTemplates::Current(page,Plan);
    if(reviewed){
        size_t changes=0;for(const auto& [key,value]:Plan.at("Values").items())changes+=value!=Plan.at("Expected").at(key);
        ImGui::Text("%zu settings; %zu changes",Plan.at("Values").size(),changes);
        if(!current)ImGui::TextColored(theme.warning,"The target or settings changed. Review this template again.");
        if(HelpyImGuiTheme::SoftButton("Review again"))try{Plan=ModMenuTemplates::Prepare(page,Document);Message="Review updated for the current target.";}catch(const std::exception& error){Message=error.what();Plan=Json{};}
        if(Plan.is_object()&&!Plan.empty()&&ImGui::BeginTable("template-review",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY,{0,std::max(theme.Y(80),ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*2)})){
            ImGui::TableSetupColumn("Setting");ImGui::TableSetupColumn("Current");ImGui::TableSetupColumn("Template");ImGui::TableHeadersRow();
            const auto settings=ModMenuTemplates::Settings(page);
            for(const auto& [key,value]:Plan.at("Values").items()){
                ImGui::TableNextRow();ImGui::TableNextColumn();const auto label=settings.at(key)->value("label",key);ImGui::TextWrapped("%s",label.c_str());ImGui::TableNextColumn();ImGui::TextWrapped("%s",Text(Plan.at("Expected").at(key)).c_str());ImGui::TableNextColumn();ImGui::TextWrapped("%s",Text(value).c_str());
            }ImGui::EndTable();
        }
    }
    const bool canApply=available&&Plan.is_object()&&!Plan.empty()&&Owner==page.at("Id").get<std::string>()&&ModMenuTemplates::Current(page,Plan);
    ImGui::BeginDisabled(!canApply);if(HelpyImGuiTheme::SoftButton("Apply template")){if(submit(Plan)){Message="Template queued; waiting for the mod's result.";Plan=Json{};}else Message="Wait for the current Mod Menu changes before applying this template.";}ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MenuTabs["mod-menu2-template/Apply"]=HelpyPreviewHooks::LastRect();
#endif
    HelpyMenu::NextControl("Close");if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();
    if((ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false)||ImGui::IsKeyPressed(ImGuiKey_Escape,false))&&ImGui::GetCurrentContext()->ActiveIdPreviousFrame==0&&HelpyMenu::PopupDepthAtFrameStart==ImGui::GetCurrentContext()->OpenPopupStack.Size)ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
}
