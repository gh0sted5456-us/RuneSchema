#include "Generator/HelpyBindingActions.h"
#pragma once
#include "Generator/HelpyFeatureSettings.h"
#include "Generator/HelpyTextInput.h"
#include "Generator/ModMenuControls.h"
#include "Generator/ModMenuTemplatePanel.h"
#include "Core/ModMenuTemplates.h"
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
inline Json Snapshot=Json::object();inline std::deque<Json> Commands;
inline uint64_t Revision=~uint64_t{},OpenSequence=0;inline uint32_t Pid=0;inline bool Available=false,OpenRequested=false;
inline bool LicenseSelected=false;
inline std::string Status,SelectedId,Compatibility,FocusRow;inline Clock::time_point Next{};
inline std::function<void()> OpenUeMods;
inline std::string PreviewChannel,InputOwner,PublishedPage,PublishedRect;inline Clock::time_point PreviewRetry{};
struct Draft {Json Value;uint64_t Revision;bool Pending=false;};inline std::map<std::string,Draft> Inputs;
inline bool Queue(Json request){if(request.value("Op",std::string{})=="page")for(auto& pending:Commands)if(pending.value("Op",std::string{})=="page"){pending=std::move(request);Next={};return true;}if(Commands.size()<32){Commands.push_back(std::move(request));Next={};return true;}Status="Wait for queued Mod Menu changes to finish.";return false;}
inline bool Backend(const Json& request,Json& reply){
    const auto* host=PluginRuntimeServices::Host;if(!host||!host->CallService)return false;
    const auto text=request.dump();std::vector<char> output(8192);uint32_t size=0;
    auto code=host->CallService("RuneSchema.Helpy","helpy.mod-menu2",text.c_str(),output.data(),uint32_t(output.size()),&size);
    if(code==RS_PLUGIN_BUFFER_TOO_SMALL&&request.value("Op",std::string{})=="read"&&size>output.size()&&size<=RUNESCHEMA_PLUGIN_MAX_MESSAGE){output.resize(size);code=host->CallService("RuneSchema.Helpy","helpy.mod-menu2",text.c_str(),output.data(),uint32_t(output.size()),&size);}
    if(code!=RS_PLUGIN_OK||!size||size>output.size())return false;if(output[size-1]=='\0')--size;reply=Json::parse(output.data(),output.data()+size);return true;
}
inline bool Loaded(){
#if defined(HELPY_PREVIEW)
    return true;
#else
    Json reply;return Backend({{"Op","read"},{"MetadataOnly",true}},reply)&&reply.value("Loaded",false);
#endif
}
inline void Poll(bool visible){
    HelpyFeatureSettings::Load();const auto pid=PluginRuntimeServices::GameProcessId.load();
    if(pid!=Pid){Pid=pid;Revision=~uint64_t{};Snapshot=Json::object();Available=false;Commands.clear();Next={};Inputs.clear();PublishedPage.clear();PublishedRect.clear();OpenSequence=0;FocusRow.clear();}
#if defined(HELPY_PREVIEW)
    if(!RuneSchemaSettingsPanel::Remote)return;
#endif
    const auto now=Clock::now();if(Commands.empty()&&now<Next)return;Next=now+std::chrono::milliseconds(visible?16:250);
    Json reply;
    while(!Commands.empty()){
        if(!Backend({{"Op","queue"},{"Command",Commands.front()}},reply)||!reply.value("Accepted",false)){Status=reply.value("Status",std::string("Mod Menu plugin is unavailable."));break;}Commands.pop_front();
    }
    if(!Backend({{"Op","tick"},{"Pid",pid},{"Unlocked",HelpyFeatureSettings::Key},{"Enabled",HelpyFeatureSettings::Menu.Enabled},{"Visible",visible}},reply)){Available=false;Status="The native Mod Menu 2 Helpy plugin is not installed or enabled.";return;}
    if(!Backend({{"Op","read"},{"Revision",Revision}},reply)){Available=false;return;}
    Available=reply.value("Available",false);Status=reply.value("Status",Status);
    if(reply.contains("Snapshot")){Snapshot=reply.at("Snapshot");Revision=reply.at("Revision").get<uint64_t>();const auto sequence=Snapshot.value("OpenSequence",uint64_t{});if(sequence!=OpenSequence&&sequence){OpenSequence=sequence;SelectedId=Snapshot.value("OpenId",std::string{});FocusRow=Snapshot.value("OpenRow",std::string{});LicenseSelected=false;OpenRequested=true;}}
}
inline void ClosePreview(){if(!PreviewChannel.empty()&&CharacterPreviewPanel::Channel.Name()==PreviewChannel)CharacterPreviewPanel::Close();PreviewChannel.clear();}
inline void Visibility(bool visible,const std::string& rect={}){visible=visible&&!LicenseSelected;if(!visible)ClosePreview();const auto id=visible?SelectedId:std::string{},bounds=visible?rect:std::string{};if(Available&&(PublishedPage!=id||PublishedRect!=bounds)){Queue({{"Op","page"},{"Id",id},{"Rect",bounds}});PublishedPage=id;PublishedRect=bounds;}}
inline void Shutdown(){ClosePreview();Commands.clear();try{Json ignored;Backend({{"Op","close"}},ignored);}catch(...){}PublishedPage.clear();PublishedRect.clear();}
inline void Settings(){
    HelpyFeatureSettings::Load();auto& settings=HelpyFeatureSettings::Menu;ImGui::TextWrapped("Based off Maxxfilth's Mod Menu, adapted natively for RuneSchema. The original MIT license is included with RuneSchema.");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(HelpyImGuiTheme::SoftButton("Check activation key")){HelpyFeatureSettings::CheckKey();try{Compatibility=ModMenuGate::Blocked(HostServices::ModDirectory());}catch(const std::exception& error){Compatibility=error.what();}}
    ImGui::TextWrapped("%s",HelpyFeatureSettings::Status.c_str());if(!HelpyFeatureSettings::Key)return;
    if(HelpyImGuiTheme::Toggle("Enable Mod Menu 2",&settings.Enabled)){if(!settings.Enabled&&Available){Queue({{"Op","stop"}});Available=false;}HelpyFeatureSettings::SaveMenu();}
    auto keyboard=ModMenu2Settings::Keyboard(settings),controller=settings.Controller;
    if(HelpyBindingPicker::Edit("mod-menu2","Mod Menu 2 shortcut",keyboard,controller,HelpyBindingActions::Check("mod-menu2"))){settings.Keyboard=keyboard;settings.Controller=controller;settings.Hotkey=0;HelpyFeatureSettings::SaveMenu();}
    ImGui::TextWrapped("Activation changes need a game restart. The shortcut takes effect immediately once this session's bridge is active. The separate Mod Menu must be disabled or removed; a running copy cannot be unloaded by changing mods.txt.");
    if(!Compatibility.empty())ImGui::TextWrapped("%s",Compatibility.c_str());
    if(OpenUeMods&&HelpyImGuiTheme::SoftButton("Manage separate UE4SS mods"))OpenUeMods();
}
inline bool ShowIf(const Json& setting,const Json& values){if(!setting.contains("showIf"))return true;const auto& rule=setting["showIf"];const auto key=rule.at("key").get<std::string>();if(!values.contains(key))return false;const auto& expected=rule.at("is");return expected.is_array()?std::find(expected.begin(),expected.end(),values.at(key))!=expected.end():values.at(key)==expected;}
inline void Control(const Json& page,const Json& setting){
    const auto id=page.at("Id").get<std::string>(),type=setting.at("type").get<std::string>(),key=setting.value("key",std::string{}),label=setting.value("label",key);const auto& values=page.at("Values");if(!ShowIf(setting,values))return;
    ImGui::PushID(key.empty()?label.c_str():key.c_str());const auto role=setting.value("runsOn",page.value("RunsOn",std::string{}));const auto session=RuntimeConnectionPanel::Read();const bool hostOnly=role=="host"||role=="server";const bool blocked=hostOnly&&(!session.Fresh||(session.Role!="standalone"&&session.Role!="listen-server"&&session.Role!="dedicated-server"));ImGui::BeginDisabled(!Available||!HelpyFeatureSettings::Menu.Enabled||blocked||setting.value("locked",false));
    const auto current=key.empty()?Json{}:values.value(key,setting.value("default",Json{}));Json next=current;bool changed=false;const auto draftKey=id+"|"+key;
    const auto inputId=ImGui::GetID(type=="slider"?"##value":"##text");auto [draft,inserted]=Inputs.try_emplace(draftKey,Draft{current,Revision});const bool editing=ImGui::GetActiveID()==inputId||ImGui::GetCurrentContext()->ActiveIdPreviousFrame==inputId;if(draft->second.Revision!=Revision&&!editing&&!draft->second.Pending)draft->second={current,Revision};
    if(type=="toggle"){bool value=current.is_boolean()&&current.get<bool>();changed=HelpyImGuiTheme::Toggle(label.c_str(),&value);next=value;}
    else if(type=="slider"){double value=draft->second.Value.is_number()?draft->second.Value.get<double>():setting.value("default",0.);const auto lo=setting.value("min",0.),hi=setting.value("max",100.);char format[16]{};snprintf(format,sizeof(format),"%%.%df",std::clamp(setting.value("decimals",3),0,6));ImGui::TextWrapped("%s %s",label.c_str(),setting.value("suffix",std::string{}).c_str());ImGui::SetNextItemWidth(-1);changed=setting.value("presentation",std::string{})=="dial"?ModMenuControls::Dial("##value",value,lo,hi,setting.value("step",1.),setting.value("decimals",3)):ImGui::SliderScalar("##value",ImGuiDataType_Double,&value,&lo,&hi,format,ImGuiSliderFlags_AlwaysClamp);const auto step=setting.value("step",1.);if(changed&&step>0)value=std::clamp(lo+std::round((value-lo)/step)*step,lo,hi);draft->second.Value=value;next=value;changed=ImGui::IsItemDeactivatedAfterEdit();}
    else if(type=="choice"){ImGui::TextWrapped("%s",label.c_str());const auto& options=setting.at("options");if(setting.value("presentation",std::string{})=="radial"&&options.size()<=12)changed=ModMenuControls::Radial("##radial",options,current,next);else for(size_t i=0;i<options.size();++i){const auto value=options[i].get<std::string>();if(i&&ImGui::GetContentRegionAvail().x>ImGui::CalcTextSize(value.c_str()).x+60)ImGui::SameLine();if(ImGui::RadioButton(value.c_str(),current==options[i])){next=options[i];changed=true;}}}
    else if(type=="text"&&setting.value("presentation",std::string{})=="color"){
        auto color=draft->second.Value.is_string()?draft->second.Value.get<std::string>():setting.value("default",std::string("#FFFFFF"));
        ImGui::TextWrapped("%s  %s",label.c_str(),color.c_str());ImGui::SetNextItemWidth(-1);
        if(ModMenuControls::Color("##color",color,setting.value("colorAlpha",false))){draft->second.Value=color;draft->second.Pending=true;}
        if(draft->second.Pending&&!ImGui::IsAnyItemActive()&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){
            next=draft->second.Value;changed=next!=current;draft->second.Pending=false;
        }
    }
    else if(type=="text"||type=="key"){char text[1024]{};strncpy_s(text,draft->second.Value.is_string()?draft->second.Value.get_ref<const std::string&>().c_str():"",_TRUNCATE);ImGui::TextWrapped("%s",label.c_str());ImGui::SetNextItemWidth(-1);if(HelpyTextInput::InputText("##text",text,sizeof(text)))draft->second.Value=std::string(text);if(ImGui::IsItemDeactivatedAfterEdit()){next=draft->second.Value;changed=next!=current;}}
    else if(type=="button"){if(HelpyImGuiTheme::SoftButton(label.c_str()))Queue({{"Op","button"},{"Id",id},{"Action",setting.at("action")}});}
    if(changed){const bool accepted=Queue({{"Op","set"},{"Id",id},{"Key",key},{"Value",next},{"Expected",current}});if(type=="text"&&setting.value("presentation",std::string{})=="color"&&!accepted)draft->second.Pending=true;}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MenuTabs["mod-menu2-controls/"+key]=HelpyPreviewHooks::LastRect();HelpyPreviewHooks::ItemFocus["mod-menu2/"+key]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    ImGui::EndDisabled();
    const bool hasTooltip=setting.contains("tooltip")&&setting.at("tooltip").is_string()&&!setting.at("tooltip").get_ref<const std::string&>().empty();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&(hasTooltip||hostOnly||setting.value("restart",false))){HelpyImGuiTheme::BeginDetailsTooltip();if(hasTooltip)HelpyImGuiTheme::DetailsText("%s",setting.at("tooltip").get_ref<const std::string&>().c_str());if(hostOnly)HelpyImGuiTheme::DetailsTextUnformatted(blocked?"The host controls this setting.":"Host setting");if(setting.value("restart",false))HelpyImGuiTheme::DetailsTextUnformatted("Applies after restarting the game");HelpyImGuiTheme::EndDetailsTooltip();}
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
    const auto& page=pages.at(selected);SelectedId=page.at("Id").get<std::string>();if(before!=selected)Visibility(true);if(InputOwner!=SelectedId){ModMenuTemplatePanel::Reset();Inputs.clear();ClosePreview();InputOwner=SelectedId;tab=0;}
    if(HelpyMenu::BeginBody("Mod Menu 2 settings",selected)){
        ImGui::SeparatorText(page.at("Name").get_ref<const std::string&>().c_str());if(page.contains("Description")&&page["Description"].is_string())ImGui::TextWrapped("%s",page["Description"].get_ref<const std::string&>().c_str());if(page.contains("Off")&&page["Off"].is_string())ImGui::TextWrapped("%s",page["Off"].get_ref<const std::string&>().c_str());
        if(page.contains("Helpy")&&page["Helpy"].contains("templates")){
            if(HelpyImGuiTheme::SoftButton("Import template..."))ModMenuTemplatePanel::Request(page);
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::MenuTabs["mod-menu2-template/OpenImport"]=HelpyPreviewHooks::LastRect();
#endif
            HelpyMenu::NextControl("Templates");if(HelpyImGuiTheme::SoftButton("Templates"))ModMenuTemplatePanel::Request(page);HelpyMenu::NextControl("More");
        }
        if(HelpyMenu::BeginMore("mod-menu-page")){if(ImGui::MenuItem("Refresh"))Queue({{"Op","refresh"}});if(ImGui::MenuItem("Reset page"))Queue({{"Op","reset"},{"Id",SelectedId}});ImGui::EndPopup();}if(Snapshot.contains("Status"))ImGui::TextWrapped("%s",Snapshot["Status"].get_ref<const std::string&>().c_str());
        if(page.contains("Errors")&&page["Errors"].is_array())for(const auto& error:page["Errors"])ImGui::TextColored(theme.warning,"%s",error.get_ref<const std::string&>().c_str());
        const auto extension=page.value("Helpy",Json::object());const auto tabs=extension.value("tabs",Json::array());std::vector<const char*> labels;if(tabs.is_array())for(size_t i=0;i<tabs.size();++i){const auto& entry=tabs[i];labels.push_back(entry.at("name").get_ref<const std::string&>().c_str());if(!FocusRow.empty()){const auto& keys=entry.at("keys");if(std::find(keys.begin(),keys.end(),FocusRow)!=keys.end())tab=int(i);}}if(!labels.empty())HelpyMenu::Tabs("editor-groups",labels,tab,false);
        const auto channel=page.value("PreviewChannel",std::string{});const bool preview=extension.value("layout",std::string{})=="character";const bool columns=preview&&ImGui::GetContentRegionAvail().x>theme.X(520)&&ImGui::BeginTable("character-editor",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp);
        if(columns){ImGui::TableSetupColumn("Controls",ImGuiTableColumnFlags_WidthStretch,1.1f);ImGui::TableSetupColumn("Character",ImGuiTableColumnFlags_WidthStretch,1.f);ImGui::TableNextColumn();}
        bool dialTable=false;int dialGroup=0;const auto endDials=[&]{if(dialTable){ImGui::EndTable();ImGui::PopID();dialTable=false;}};
        bool section=true;for(const auto& setting:page.at("Settings")){if(!labels.empty()&&setting.contains("key")){const auto& keys=tabs.at(tab).at("keys");if(std::find(keys.begin(),keys.end(),setting.at("key"))==keys.end())continue;}if(setting.value("type",std::string{})=="header"){endDials();const auto label=setting.value("label",std::string{});ImGui::PushID(setting.value("idx",0));ImGui::SetNextItemOpen(FocusRow.empty()?!setting.value("collapsed",false):true,FocusRow.empty()?ImGuiCond_Once:ImGuiCond_Always);section=ImGui::CollapsingHeader(label.c_str());ImGui::PopID();}else if(section){const bool dial=setting.value("presentation",std::string{})=="dial";if(!dial)endDials();if(dial&&!dialTable){ImGui::PushID(dialGroup++);dialTable=ImGui::BeginTable("dial-controls",std::clamp(int(ImGui::GetContentRegionAvail().x/theme.X(150)),1,4),ImGuiTableFlags_SizingStretchSame);if(!dialTable)ImGui::PopID();}if(dialTable)ImGui::TableNextColumn();const bool focus=!FocusRow.empty()&&setting.value("key",std::string{})==FocusRow;if(focus)ImGui::SetKeyboardFocusHere();Control(page,setting);if(focus){ImGui::SetScrollHereY(.3f);FocusRow.clear();}ImGui::Spacing();}}
        endDials();
        if(preview){if(columns)ImGui::TableNextColumn();ImGui::SeparatorText("Character preview");if(!channel.empty()&&Clock::now()>=PreviewRetry&&(CharacterPreviewPanel::Channel.Name().empty()||CharacterPreviewPanel::Channel.Name()==PreviewChannel))try{if(channel!=PreviewChannel){CharacterPreviewPanel::Attach(channel);PreviewChannel=channel;}}catch(const std::exception& error){PreviewRetry=Clock::now()+std::chrono::seconds(1);Status=error.what();}
            if(!PreviewChannel.empty()&&CharacterPreviewPanel::Channel.Name()==PreviewChannel)CharacterPreviewPanel::Draw(std::max(theme.Y(220),ImGui::GetContentRegionAvail().y-theme.Y(150)));
            else ImGui::TextWrapped("The mod can provide a live preview through its Helpy preview channel. Its existing settings work independently.");}
        if(columns)ImGui::EndTable();
        ModMenuTemplatePanel::Render(page,Available&&HelpyFeatureSettings::Menu.Enabled,[](Json command){return Queue(std::move(command));});
    }HelpyMenu::EndBody();
}
}
