#pragma once
#include <imgui.h>
#include <nlohmann/json.hpp>
namespace PS::AuthoringForms {
inline nlohmann::json Player(nlohmann::json body) {
    static bool setScale=false,setHealth=false,setStamina=false;
    static float scale=1,health=100,stamina=100;
    ImGui::Checkbox("Set scale",&setScale);ImGui::SameLine();ImGui::SetNextItemWidth(130);ImGui::InputFloat("##player-scale",&scale,.1f);
    ImGui::Checkbox("Set maximum health",&setHealth);ImGui::SameLine();ImGui::SetNextItemWidth(130);ImGui::InputFloat("##player-health",&health,10);
    ImGui::Checkbox("Set maximum stamina",&setStamina);ImGui::SameLine();ImGui::SetNextItemWidth(130);ImGui::InputFloat("##player-stamina",&stamina,10);
    if(setScale)body["Scale"]=scale;if(setHealth)body["MaxHealth"]=health;if(setStamina)body["MaxStamina"]=stamina;
    return body;
}
inline nlohmann::json Nameplate(nlohmann::json body) {
    static int mode=0,action=0;
    static char icon[2049]{},state[65]="Active",stateIcon[2049]{},function[513]{};
    static float scale=1,distance=2500,timeout=3;
    static bool client=true,server=true,addState=false,addEvent=false;
    constexpr const char* modes[]{"Name","Icon","Hidden"};constexpr const char* actions[]{"Pulse","Activate","Deactivate"};
    ImGui::Combo("Base mode",&mode,modes,3);ImGui::InputText("Base cooked icon",icon,sizeof(icon));
    ImGui::InputFloat("Base scale",&scale,.1f);ImGui::InputFloat("Visible distance (cm)",&distance,100);ImGui::InputFloat("Activity timeout (seconds)",&timeout,.5f);
    ImGui::Checkbox("Show to this client",&client);ImGui::Checkbox("Show to server / other players",&server);
    ImGui::Checkbox("Add activity state",&addState);
    if(addState){ImGui::InputText("State Id",state,sizeof(state));ImGui::InputText("State cooked icon",stateIcon,sizeof(stateIcon));ImGui::Checkbox("Bind an exact event",&addEvent);
        if(addEvent){ImGui::InputText("Function /Class.Class_C:Function",function,sizeof(function));ImGui::Combo("Event action",&action,actions,3);}}
    body["Mode"]=modes[mode];body["Scale"]=scale;body["Distance"]=distance;body["ActivityTimeoutSeconds"]=timeout;body["Client"]=client?"Yes":"No";body["Server"]=server?"Yes":"No";
    if(icon[0])body["Icon"]=icon;
    if(addState){body["States"][state]={{"Icon",stateIcon},{"Scale",.9}};if(addEvent)body["Events"]=nlohmann::json::array({{{"Function",function},{"State",state},{"Action",actions[action]}}});}
    return body;
}
}
