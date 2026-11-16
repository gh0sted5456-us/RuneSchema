#pragma once
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/HelpyBindingActions.h"
#include "Generator/HelpyTextInput.h"
#include "Core/ProcessExitFuture.h"
#include <deque>

namespace PS::HudExtensionsPanel {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline bool HideRequested=false,OpenRequested=false,Direct=false,CloseRequested=false,ShowRequested=false;
inline Json Clients=Json::array(),Interfaces=Json::array(),Declarations=Json::array();
inline std::string Status,PanelId;
inline Json PendingActivation;
inline ProcessExitFuture<Json> Worker;inline std::deque<Json> Commands;
inline uint32_t Pid=0;inline uint64_t Revision=~uint64_t{};inline Clock::time_point Next{};
inline int Selected=0,Section=0;
struct Draft{Json Value;uint64_t Revision;};inline std::map<std::string,Draft> Inputs;
inline const Json* Find(const std::string& id){for(const auto& row:Interfaces)if(row.at("Id")==id)return &row;return nullptr;}
inline bool Ready(const Json& row){
    if(row.at("Kind")=="panel")return row.value("Ready",false);
    size_t count=0;bool ready=false;for(const auto& client:Clients)if(client.value("Owner",std::string{})==row.value("Client",std::string{})){++count;ready=client.value("UiReady",false);}return count==1&&ready;
}
inline void Rebuild(){
    Interfaces=Declarations;std::set<std::string> declared;
    for(const auto& row:Declarations)if(row.at("Kind")=="client")declared.insert(row.at("Client").get<std::string>());
    for(const auto& client:Clients){const auto owner=client.value("Owner",std::string{});if(!declared.contains(owner))Interfaces.push_back({{"Id","client/"+owner},{"Name",owner},{"Owner",owner},{"Kind","client"},{"Client",owner},{"Keyboard","None"},{"Controller","None"}});}
    HelpyBindingActions::Extensions.clear();
    for(const auto& row:Interfaces){
        const auto id=row.at("Id").get<std::string>();const auto& overrides=HelpyImGuiTheme::Current.hudBindings;
        const auto& binding=overrides.contains(id)?overrides.at(id):row;
        if(Ready(row))HelpyBindingActions::Extensions["hud:"+id]={"hud:"+id,row.at("Name"),binding.value("Keyboard",std::string("None")),binding.value("Controller",std::string("None"))};
    }
}
inline bool Queue(Json request){if(Commands.size()>=32){Status="Wait for pending HUD actions.";return false;}Commands.push_back(std::move(request));Next={};return true;}
inline void AfterHide(){if(!PendingActivation.is_null()){if(!Queue(std::move(PendingActivation)))ShowRequested=true;PendingActivation=nullptr;}}
inline void Activate(const std::string& id,bool toggle=false,bool direct=false){
    const auto* row=Find(id);if(!row||!Ready(*row)){Status="This HUD interface is not ready.";return;}
    if(row->at("Kind")=="panel"){
        if(toggle&&PanelId==id){CloseRequested=true;return;}
        PanelId=id;Direct=direct;OpenRequested=true;CloseRequested=false;Inputs.clear();
    }else{
        Json request={{"Action","HudExtensions"},{"Payload",{{"Action",toggle?"Toggle":"Activate"},{"Owner",row->at("Client")}}},{"_Activation",true}};
        if(direct){if(!Queue(std::move(request)))ShowRequested=true;}else if(Commands.size()<32){PendingActivation=std::move(request);HideRequested=true;}else Status="Wait for pending HUD actions.";
    }
}
inline void Poll(bool visible){
    const auto pid=PluginRuntimeServices::GameProcessId.load();
    if(pid!=Pid){Pid=pid;Revision=~uint64_t{};Clients=Declarations=Interfaces=Json::array();Commands.clear();PendingActivation=nullptr;ShowRequested=false;PanelId.clear();CloseRequested=true;Next={};Inputs.clear();HelpyBindingActions::Extensions.clear();}
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{const auto reply=Worker.get();if(reply.value("_Pid",uint32_t{})==pid){
            if(reply.contains("Clients"))Clients=reply.at("Clients");
            if(reply.contains("Interfaces")){Declarations=reply.at("Interfaces");Revision=reply.at("Revision").get<uint64_t>();}
            if(reply.value("_Write",false)){Status=reply.value("Accepted",false)?std::string{}:reply.value("Status",reply.value("Error",std::string("HUD action failed.")));if(!reply.value("Accepted",false)&&reply.value("_Activation",false))ShowRequested=true;Next={};}
            else Status=reply.value("Status",std::string{});
            Rebuild();
        }}catch(const std::exception& error){Status=error.what();}
    }
    if(!pid||Worker.valid()||!RuneSchemaSettingsPanel::Remote)return;
    const auto now=Clock::now();if(Commands.empty()&&now<Next)return;Next=now+std::chrono::milliseconds(visible||!PanelId.empty()?200:1500);
    const auto remote=RuneSchemaSettingsPanel::Remote;const auto revision=Revision;
    Json command;if(!Commands.empty()){command=std::move(Commands.front());Commands.pop_front();}
    Worker=std::async(std::launch::async,[remote,pid,revision,command]{
        Json reply;if(!command.is_null()){
            auto request=command;request.erase("_Activation");if(!remote(request,reply))reply={{"Accepted",false},{"Status","The HUD service is unavailable."}};
            reply["_Write"]=true;reply["_Activation"]=command.value("_Activation",false);
        }else{
            Json clients;if(remote({{"Action","HudExtensions"},{"Payload",{{"Action","List"}}}},clients))reply["Clients"]=clients.value("Clients",Json::array());
            Json interfaces;if(remote({{"Action","HudInterfacesRead"},{"Revision",revision}},interfaces)){reply.update(interfaces);}
            else reply["Status"]="Connect to a matching RuneSchema Core/HUD build to use declared mod panels.";
        }reply["_Pid"]=pid;return reply;
    });
}
inline void Shutdown(){
    Commands.clear();PendingActivation=nullptr;
    if(Worker.valid()&&!ProcessExitInProgress.load())try{(void)Worker.get();}catch(...){}
    Clients=Declarations=Interfaces=Json::array();PanelId.clear();HelpyBindingActions::Extensions.clear();Pid=0;
}
inline void Binding(const Json& row){
    auto& theme=HelpyImGuiTheme::Current;const auto id=row.at("Id").get<std::string>(),action="hud:"+id;
    const auto& binding=theme.hudBindings.contains(id)?theme.hudBindings.at(id):row;
    auto keyboard=binding.value("Keyboard",std::string("None")),controller=binding.value("Controller",std::string("None"));
    if(HelpyBindingPicker::Edit(action.c_str(),"HUD shortcut",keyboard,controller,HelpyBindingActions::Check(action.c_str()),row.value("Keyboard",std::string("None")).c_str(),row.value("Controller",std::string("None")).c_str())){
        theme.hudBindings[id]={{"Keyboard",keyboard},{"Controller",controller}};HelpyImGuiTheme::SaveCurrent();Rebuild();
    }
}
inline void Render(){
    ImGui::TextWrapped("Choose a mod panel or registered app. Shortcuts use the same input capture as Helpy.");
    if(HelpyImGuiTheme::SoftButton("Refresh"))Next={};
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Interfaces.empty()){ImGui::TextDisabled("No enabled HUD interfaces are available.");return;}
    Selected=std::clamp(Selected,0,int(Interfaces.size())-1);const auto& row=Interfaces.at(size_t(Selected));
    ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("Interface",row.at("Name").get_ref<const std::string&>().c_str())){
        for(size_t i=0;i<Interfaces.size();++i)if(ImGui::Selectable(Interfaces[i].at("Name").get_ref<const std::string&>().c_str(),Selected==int(i)))Selected=int(i);ImGui::EndCombo();
    }
    const auto id=row.at("Id").get<std::string>();constexpr std::array tabs{"Overview","Bindings"};HelpyMenu::Tabs("hud-interface-tabs",tabs,Section,false);
    if(Section==1)Binding(row);
    else{
        ImGui::TextDisabled("%s",row.at("Owner").get_ref<const std::string&>().c_str());const bool ready=Ready(row);
        ImGui::TextWrapped("%s",ready?"Ready":row.at("Kind")=="panel"?"Waiting for the mod.":"Open the app and connect its HUD interface.");
        ImGui::BeginDisabled(!ready);if(HelpyImGuiTheme::SoftButton("Open in HUD"))Activate(id);ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
        if(row.at("Kind")=="client")for(const auto& client:Clients)if(client.value("Owner",std::string{})==row.at("Client").get<std::string>())HelpyPreviewHooks::MenuTabs["hud-open-"+std::to_string(client.at("Client").get<uint64_t>())]=HelpyPreviewHooks::LastRect();
#endif
        if(row.at("Kind")=="client"){HelpyMenu::NextControl("Close interface");if(HelpyImGuiTheme::SoftButton("Close interface"))Queue({{"Action","HudExtensions"},{"Payload",{{"Action","Deactivate"},{"Owner",row.at("Client")}}}});}
        ImGui::TextWrapped("%s",row.at("Kind")=="panel"?"The mod saves and applies its settings.":"Keep the app running while using its in-game interface.");
    }
}

inline void Control(const Json& row,const Json& control){
    const auto panel=row.at("Id").get<std::string>(),id=control.at("Id").get<std::string>(),type=control.at("Type").get<std::string>(),label=control.at("Label").get<std::string>(),key=panel+"/"+id;
    ImGui::PushID(id.c_str());const auto current=control.at("Value");
    const bool enabled=row.value("Ready",false)&&row.contains("CallbackControls")&&std::find(row.at("CallbackControls").begin(),row.at("CallbackControls").end(),id)!=row.at("CallbackControls").end();
    ImGui::BeginDisabled(type!="label"&&!enabled);bool changed=false;Json value=current;
    const auto inputId=ImGui::GetID(type=="slider"?"##value":"##text");auto [draft,inserted]=Inputs.try_emplace(key,Draft{current,Revision});
    const bool editing=ImGui::GetActiveID()==inputId||ImGui::GetCurrentContext()->ActiveIdPreviousFrame==inputId;
    if(draft->second.Revision!=Revision&&!editing)draft->second={current,Revision};
    if(type=="label"){ImGui::TextUnformatted(label.c_str());ImGui::TextWrapped("%s",current.get_ref<const std::string&>().c_str());}
    else if(type=="toggle"){bool selected=current.get<bool>();changed=HelpyImGuiTheme::Toggle(label.c_str(),&selected);value=selected;}
    else if(type=="slider"){double number=draft->second.Value.get<double>(),lo=control.at("Min"),hi=control.at("Max");ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);if(ImGui::SliderScalar("##value",ImGuiDataType_Double,&number,&lo,&hi,"%.3f",ImGuiSliderFlags_AlwaysClamp)){const double step=control.value("Step",0.);if(step>0)number=std::clamp(lo+std::round((number-lo)/step)*step,lo,hi);draft->second.Value=number;}if(ImGui::IsItemDeactivatedAfterEdit()){value=draft->second.Value;changed=value!=current;}}
    else if(type=="choice"){ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##choice",current.get_ref<const std::string&>().c_str())){for(const auto& option:control.at("Options"))if(ImGui::Selectable(option.get_ref<const std::string&>().c_str(),option==current)){value=option;changed=true;}ImGui::EndCombo();}}
    else if(type=="text"){char text[2049]{};strncpy_s(text,draft->second.Value.get_ref<const std::string&>().c_str(),_TRUNCATE);ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);if(HelpyTextInput::InputText("##text",text,sizeof(text)))draft->second.Value=std::string(text);if(ImGui::IsItemDeactivatedAfterEdit()){value=draft->second.Value;changed=value!=current;}}
    else if(type=="button")changed=HelpyImGuiTheme::SoftButton(label.c_str());
    if(changed)Queue({{"Action","HudInterfacesWrite"},{"Payload",{{"Id",panel},{"Control",id},{"Value",value},{"Expected",current}}}});
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MenuTabs["hud-control/"+id]=HelpyPreviewHooks::LastRect();
    HelpyPreviewHooks::MenuFocus["hud-control/"+id]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    ImGui::EndDisabled();ImGui::PopID();
}
inline void RenderActive(bool wasEditing=false,bool childPopup=false){
    if(PanelId.empty()&&!CloseRequested)return;
    HelpyImGuiTheme::Scope skin(HelpyImGuiTheme::Current);
    const auto* row=Find(PanelId);if(!row||!Ready(*row))CloseRequested=true;
    if(OpenRequested){ImGui::OpenPopup("Mod HUD");OpenRequested=false;}
    const auto area=ImGui::GetMainViewport()->WorkSize;ImGui::SetNextWindowSizeConstraints({std::min(360.f,area.x-24),std::min(240.f,area.y-24)},{std::max(1.f,area.x-24),std::max(1.f,area.y-24)});
    const float height=row?std::clamp(140.f+float(row->at("Controls").size())*56.f,240.f,560.f):240.f;
    ImGui::SetNextWindowSize({std::min(620.f,area.x-24),std::min(height,area.y-24)},ImGuiCond_FirstUseEver);
    bool open=true;if(ImGui::BeginPopupModal("Mod HUD",&open,ImGuiWindowFlags_NoCollapse)){
        if(CloseRequested){ImGui::CloseCurrentPopup();CloseRequested=false;PanelId.clear();if(Direct)HideRequested=true;}
        else{
            ImGui::SeparatorText(row->at("Name").get_ref<const std::string&>().c_str());
            if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());if(row->contains("Error"))ImGui::TextWrapped("%s",row->at("Error").get_ref<const std::string&>().c_str());
            if(HelpyImGuiTheme::BeginPanel("mod-hud-controls",{0,std::max(1.f,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing())},true))for(const auto& control:row->at("Controls"))Control(*row,control);HelpyImGuiTheme::EndPanel();
            if(HelpyImGuiTheme::SoftButton("Close")||(!wasEditing&&!childPopup&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))){ImGui::CloseCurrentPopup();PanelId.clear();if(Direct)HideRequested=true;}
        }ImGui::EndPopup();
    }
    if(!open){PanelId.clear();if(Direct)HideRequested=true;}
}
}
