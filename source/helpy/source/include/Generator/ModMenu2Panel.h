#pragma once
#include "Generator/HelpyFeatureSettings.h"
#include "Generator/HelpyTextInput.h"
#include "Generator/HelpyMenu.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/RuntimeConnectionPanel.h"
#include "Generator/CharacterPreviewPanel.h"
#include "Core/ProcessExitFuture.h"
#include "Core/ModMenuGate.h"
#include "Core/ModMenuLicense.h"
#include <deque>
#include <map>

namespace PS::ModMenu2Panel {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline ProcessExitFuture<Json> Worker;inline Json Snapshot=Json::object();inline std::deque<Json> Commands;
inline uint64_t Revision=~uint64_t{},OpenSequence=0;inline uint32_t Pid=0;inline bool Available=false,OpenRequested=false;
inline bool LicenseSelected=false;
inline std::string Status,SelectedId,Compatibility,FocusRow;inline Clock::time_point Next{};
inline std::function<void()> OpenUeMods;
inline std::string PreviewChannel,InputOwner,PublishedPage,PublishedRect;inline Clock::time_point PreviewRetry{};
struct Draft {Json Value;uint64_t Revision;};inline std::map<std::string,Draft> Inputs;
inline void Queue(Json request){if(request.value("Op",std::string{})=="page")for(auto& pending:Commands)if(pending.value("Op",std::string{})=="page"){pending=std::move(request);Next={};return;}if(Commands.size()<32){Commands.push_back(std::move(request));Next={};}else Status="Wait for queued Mod Menu changes to finish.";}
inline void Poll(bool visible){
    HelpyFeatureSettings::Load();const auto pid=PluginRuntimeServices::GameProcessId.load();if(pid!=Pid){Pid=pid;Revision=~uint64_t{};Snapshot=Json::object();Available=false;Commands.clear();Next={};Inputs.clear();PublishedPage.clear();PublishedRect.clear();OpenSequence=0;FocusRow.clear();}
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{const auto reply=Worker.get();if(reply.value("_Pid",uint32_t{})==pid){if(reply.contains("Available"))Available=reply.at("Available").get<bool>();Status=reply.value("Status",Status);if(reply.contains("Snapshot")){Snapshot=reply["Snapshot"];Revision=reply.at("Revision").get<uint64_t>();const auto sequence=Snapshot.value("OpenSequence",uint64_t{});if(sequence!=OpenSequence&&sequence){OpenSequence=sequence;SelectedId=Snapshot.value("OpenId",std::string{});FocusRow=Snapshot.value("OpenRow",std::string{});LicenseSelected=false;OpenRequested=true;}}}}catch(const std::exception& error){Status=error.what();Available=false;}}
    if(!pid){Available=false;Status="Connect to a running game to use Mod Menu 2.";return;}
    if(!HelpyFeatureSettings::Key||(!HelpyFeatureSettings::Menu.Enabled&&Commands.empty())||Worker.valid()||!RuneSchemaSettingsPanel::Remote)return;
    const auto now=Clock::now();if(Commands.empty()&&now<Next)return;Next=now+std::chrono::milliseconds(visible?200:1000);const auto remote=RuneSchemaSettingsPanel::Remote;
    Json request={{"Action","HelpyModMenuRead"},{"Revision",Revision}};if(!Commands.empty()){request={{"Action","HelpyModMenuWrite"},{"Payload",std::move(Commands.front())}};Commands.pop_front();}
    Worker=std::async(std::launch::async,[remote,pid,request]{Json reply;if(!remote(request,reply))reply={{"Available",false},{"Status","Mod Menu 2 is unavailable through this connection. Check the runtime connection and matching Core/Helpy installation."}};reply["_Pid"]=pid;return reply;});
}
inline void ClosePreview(){if(!PreviewChannel.empty()&&CharacterPreviewPanel::Channel.Name()==PreviewChannel)CharacterPreviewPanel::Close();PreviewChannel.clear();}
inline void Visibility(bool visible,const std::string& rect={}){visible=visible&&!LicenseSelected;if(!visible)ClosePreview();const auto id=visible?SelectedId:std::string{},bounds=visible?rect:std::string{};if(Available&&(PublishedPage!=id||PublishedRect!=bounds)){Queue({{"Op","page"},{"Id",id},{"Rect",bounds}});PublishedPage=id;PublishedRect=bounds;}}
inline void Shutdown(){ClosePreview();Commands.clear();if(Worker.valid())try{Worker.get();}catch(...){}if(Pid&&Available&&RuneSchemaSettingsPanel::Remote)try{Json ignored;RuneSchemaSettingsPanel::Remote({{"Action","HelpyModMenuWrite"},{"Payload",{{"Op","page"},{"Id",""},{"Rect",""}}}},ignored);}catch(...){}PublishedPage.clear();PublishedRect.clear();}
inline void Settings(){
    HelpyFeatureSettings::Load();auto& settings=HelpyFeatureSettings::Menu;ImGui::TextWrapped("Based off Maxxfilth's Mod Menu, adapted natively for RuneSchema. The original MIT license is included with RuneSchema.");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(HelpyImGuiTheme::SoftButton("Check activation key")){HelpyFeatureSettings::CheckKey();try{Compatibility=ModMenuGate::Blocked(HostServices::ModDirectory());}catch(const std::exception& error){Compatibility=error.what();}}
    ImGui::TextWrapped("%s",HelpyFeatureSettings::Status.c_str());if(!HelpyFeatureSettings::Key)return;
    if(HelpyImGuiTheme::Toggle("Enable Mod Menu 2",&settings.Enabled)){if(!settings.Enabled&&Available){Queue({{"Op","stop"}});Available=false;}HelpyFeatureSettings::SaveMenu();}
    std::string key=settings.Hotkey?"F"+std::to_string(settings.Hotkey-VK_F1+1):"None";ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(220));
    if(ImGui::BeginCombo("Mod Menu 2 shortcut",key.c_str())){if(ImGui::Selectable("None",!settings.Hotkey)){settings.Hotkey=0;HelpyFeatureSettings::SaveMenu();}for(int i=VK_F1;i<=VK_F24;++i){const auto name="F"+std::to_string(i-VK_F1+1);ImGui::BeginDisabled(i==HelpySettings::Hotkey.load());if(ImGui::Selectable(name.c_str(),settings.Hotkey==i)){settings.Hotkey=i;HelpyFeatureSettings::SaveMenu();}ImGui::EndDisabled();}ImGui::EndCombo();}
    ImGui::TextWrapped("Activation changes need a game restart. The shortcut takes effect immediately once this session's bridge is active. The separate Mod Menu must be disabled or removed; a running copy cannot be unloaded by changing mods.txt.");
    if(!Compatibility.empty())ImGui::TextWrapped("%s",Compatibility.c_str());
    if(OpenUeMods&&HelpyImGuiTheme::SoftButton("Manage separate UE4SS mods"))OpenUeMods();
}
inline bool ShowIf(const Json& setting,const Json& values){if(!setting.contains("showIf"))return true;const auto& rule=setting["showIf"];const auto key=rule.at("key").get<std::string>();if(!values.contains(key))return false;const auto& expected=rule.at("is");return expected.is_array()?std::find(expected.begin(),expected.end(),values.at(key))!=expected.end():values.at(key)==expected;}
inline void Control(const Json& page,const Json& setting){
    const auto id=page.at("Id").get<std::string>(),type=setting.at("type").get<std::string>(),key=setting.value("key",std::string{}),label=setting.value("label",key);const auto& values=page.at("Values");if(!ShowIf(setting,values))return;
    ImGui::PushID(key.empty()?label.c_str():key.c_str());const auto role=setting.value("runsOn",page.value("RunsOn",std::string{}));const auto session=RuntimeConnectionPanel::Read();const bool hostOnly=role=="host"||role=="server";const bool blocked=hostOnly&&(!session.Fresh||(session.Role!="standalone"&&session.Role!="listen-server"&&session.Role!="dedicated-server"));ImGui::BeginDisabled(!Available||!HelpyFeatureSettings::Menu.Enabled||blocked||setting.value("locked",false));
    const auto current=key.empty()?Json{}:values.value(key,setting.value("default",Json{}));Json next=current;bool changed=false;const auto draftKey=id+"|"+key;
    const auto inputId=ImGui::GetID(type=="slider"?"##value":"##text");auto [draft,inserted]=Inputs.try_emplace(draftKey,Draft{current,Revision});const bool editing=ImGui::GetActiveID()==inputId||ImGui::GetCurrentContext()->ActiveIdPreviousFrame==inputId;if(draft->second.Revision!=Revision&&!editing)draft->second={current,Revision};
    if(type=="toggle"){bool value=current.is_boolean()&&current.get<bool>();changed=HelpyImGuiTheme::Toggle(label.c_str(),&value);next=value;}
    else if(type=="slider"){double value=draft->second.Value.is_number()?draft->second.Value.get<double>():setting.value("default",0.);const auto lo=setting.value("min",0.),hi=setting.value("max",100.);ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);changed=ImGui::SliderScalar("##value",ImGuiDataType_Double,&value,&lo,&hi,"%.3f",ImGuiSliderFlags_AlwaysClamp);const auto step=setting.value("step",1.);if(changed&&step>0)value=std::clamp(lo+std::round((value-lo)/step)*step,lo,hi);draft->second.Value=value;next=value;changed=ImGui::IsItemDeactivatedAfterEdit();}
    else if(type=="choice"){ImGui::TextUnformatted(label.c_str());const auto& options=setting.at("options");for(size_t i=0;i<options.size();++i){const auto value=options[i].get<std::string>();if(i&&ImGui::GetContentRegionAvail().x>ImGui::CalcTextSize(value.c_str()).x+60)ImGui::SameLine();if(ImGui::RadioButton(value.c_str(),current==options[i])){next=options[i];changed=true;}}}
    else if(type=="text"||type=="key"){char text[1024]{};strncpy_s(text,draft->second.Value.is_string()?draft->second.Value.get_ref<const std::string&>().c_str():"",_TRUNCATE);ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);if(HelpyTextInput::InputText("##text",text,sizeof(text)))draft->second.Value=std::string(text);if(ImGui::IsItemDeactivatedAfterEdit()){next=draft->second.Value;changed=next!=current;}}
    else if(type=="button"){if(HelpyImGuiTheme::SoftButton(label.c_str()))Queue({{"Op","button"},{"Id",id},{"Action",setting.at("action")}});}
    if(changed)Queue({{"Op","set"},{"Id",id},{"Key",key},{"Value",next},{"Expected",current}});
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MenuTabs["mod-menu2-controls/"+key]=HelpyPreviewHooks::LastRect();HelpyPreviewHooks::ItemFocus["mod-menu2/"+key]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){HelpyImGuiTheme::BeginDetailsTooltip();if(setting.contains("tooltip"))HelpyImGuiTheme::DetailsText("%s",setting.at("tooltip").get_ref<const std::string&>().c_str());if(hostOnly)HelpyImGuiTheme::DetailsTextUnformatted(blocked?"The host controls this setting.":"Host setting");if(setting.value("restart",false))HelpyImGuiTheme::DetailsTextUnformatted("Applies after restarting the game");HelpyImGuiTheme::EndDetailsTooltip();}
    ImGui::PopID();
}
inline void License(){
    static const auto text=ModMenuLicense::Text();
    ImGui::SeparatorText("Mod Menu license");
    ImGui::TextWrapped("Based off Maxxfilth's Mod Menu, adapted natively for RuneSchema.");
    ImGui::BeginDisabled(text.empty());if(HelpyImGuiTheme::SoftButton("Copy license"))ImGui::SetClipboardText(std::string(text).c_str());ImGui::EndDisabled();
    ImGui::SeparatorText("Permissions & conditions");
    ImGui::TextWrapped("The MIT license permits use, copying, modification, merging, publication, distribution, sublicensing and sale of the licensed code. Keep the original copyright and permission notice in copies or substantial portions. The software is provided without warranty.");
    ImGui::SeparatorText("Original MIT notice");
    if(text.empty())ImGui::TextWrapped("The embedded license is unavailable. See RuneSchema's complete legal notice.");
    else ImGui::TextWrapped("%.*s",int(text.size()),text.data());
}
inline void Render(){
    Poll(true);auto& theme=HelpyImGuiTheme::Current;
    ImGui::TextDisabled("Based off Maxxfilth's Mod Menu, adapted natively for RuneSchema");
    const Json empty=Json::array();const bool ready=HelpyFeatureSettings::Key&&Available;
    const auto& pages=ready&&Snapshot.contains("Pages")&&Snapshot["Pages"].is_array()?Snapshot["Pages"]:empty;
    static int selected=0,tab=0;std::vector<const char*> names;
    for(size_t i=0;i<pages.size();++i){names.push_back(pages[i].at("Name").get_ref<const std::string&>().c_str());if(!LicenseSelected&&pages[i].at("Id")==SelectedId)selected=int(i);}
    if(names.empty())names.push_back("Setup");
    selected=LicenseSelected?int(names.size()):std::clamp(selected,0,int(names.size())-1);
    const int before=selected;HelpyMenu::Categories("mod-menu2-pages",names,selected,"License");LicenseSelected=selected==int(names.size());
    if(LicenseSelected){if(HelpyMenu::BeginBody("Mod Menu 2 license",selected))License();HelpyMenu::EndBody();return;}
    if(!ready||pages.empty()){
        if(HelpyMenu::BeginBody("Mod Menu 2 setup",selected)){
            if(!ready)Settings();else{if(HelpyImGuiTheme::SoftButton("Refresh configurable mods"))Queue({{"Op","refresh"}});ImGui::TextWrapped("No supported modmenu.json or modmenu.txt pages are registered yet.");}
        }HelpyMenu::EndBody();return;
    }
    const auto& page=pages.at(selected);SelectedId=page.at("Id").get<std::string>();if(before!=selected)Visibility(true);if(InputOwner!=SelectedId){Inputs.clear();ClosePreview();InputOwner=SelectedId;tab=0;}
    if(HelpyMenu::BeginBody("Mod Menu 2 settings",selected)){
        ImGui::SeparatorText(page.at("Name").get_ref<const std::string&>().c_str());if(page.contains("Description")&&page["Description"].is_string())ImGui::TextWrapped("%s",page["Description"].get_ref<const std::string&>().c_str());if(page.contains("Off")&&page["Off"].is_string())ImGui::TextWrapped("%s",page["Off"].get_ref<const std::string&>().c_str());
        if(HelpyImGuiTheme::SoftButton("Refresh"))Queue({{"Op","refresh"}});ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Reset page"))Queue({{"Op","reset"},{"Id",SelectedId}});if(Snapshot.contains("Status"))ImGui::TextWrapped("%s",Snapshot["Status"].get_ref<const std::string&>().c_str());
        if(page.contains("Errors")&&page["Errors"].is_array())for(const auto& error:page["Errors"])ImGui::TextColored(theme.warning,"%s",error.get_ref<const std::string&>().c_str());
        const auto extension=page.value("Helpy",Json::object());const auto tabs=extension.value("tabs",Json::array());std::vector<const char*> labels;if(tabs.is_array())for(size_t i=0;i<tabs.size();++i){const auto& entry=tabs[i];labels.push_back(entry.at("name").get_ref<const std::string&>().c_str());if(!FocusRow.empty()){const auto& keys=entry.at("keys");if(std::find(keys.begin(),keys.end(),FocusRow)!=keys.end())tab=int(i);}}if(!labels.empty())HelpyMenu::Tabs("editor-groups",labels,tab);
        const auto channel=page.value("PreviewChannel",std::string{});const bool preview=extension.value("layout",std::string{})=="character";const bool columns=preview&&ImGui::GetContentRegionAvail().x>theme.X(520)&&ImGui::BeginTable("character-editor",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp);
        if(columns){ImGui::TableSetupColumn("Controls",ImGuiTableColumnFlags_WidthStretch,1.1f);ImGui::TableSetupColumn("Character",ImGuiTableColumnFlags_WidthStretch,1.f);ImGui::TableNextColumn();}
        bool section=true;for(const auto& setting:page.at("Settings")){if(!labels.empty()&&setting.contains("key")){const auto& keys=tabs.at(tab).at("keys");if(std::find(keys.begin(),keys.end(),setting.at("key"))==keys.end())continue;}if(setting.value("type",std::string{})=="header"){const auto label=setting.value("label",std::string{});ImGui::PushID(setting.value("idx",0));ImGui::SetNextItemOpen(FocusRow.empty()?!setting.value("collapsed",false):true,FocusRow.empty()?ImGuiCond_Once:ImGuiCond_Always);section=ImGui::CollapsingHeader(label.c_str());ImGui::PopID();}else if(section){const bool focus=!FocusRow.empty()&&setting.value("key",std::string{})==FocusRow;if(focus)ImGui::SetKeyboardFocusHere();Control(page,setting);if(focus){ImGui::SetScrollHereY(.3f);FocusRow.clear();}ImGui::Spacing();}}
        if(preview){if(columns)ImGui::TableNextColumn();ImGui::SeparatorText("Character preview");if(!channel.empty()&&Clock::now()>=PreviewRetry&&(CharacterPreviewPanel::Channel.Name().empty()||CharacterPreviewPanel::Channel.Name()==PreviewChannel))try{if(channel!=PreviewChannel){CharacterPreviewPanel::Attach(channel);PreviewChannel=channel;}}catch(const std::exception& error){PreviewRetry=Clock::now()+std::chrono::seconds(1);Status=error.what();}
            if(!PreviewChannel.empty()&&CharacterPreviewPanel::Channel.Name()==PreviewChannel)CharacterPreviewPanel::Draw(std::max(theme.Y(220),ImGui::GetContentRegionAvail().y-theme.Y(150)));
            else ImGui::TextWrapped("The mod can provide a live preview through its Helpy preview channel. Its existing settings work independently.");}
        if(columns)ImGui::EndTable();
    }HelpyMenu::EndBody();
}
}
