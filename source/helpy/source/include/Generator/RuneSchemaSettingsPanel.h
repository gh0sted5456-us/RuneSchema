#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HelpySettings.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <vector>
#include <cctype>

namespace PS::RuneSchemaSettingsPanel {
using json=nlohmann::json;
inline std::function<bool(const json&,json&)> Remote;
inline json Snapshot=json::object(),Draft=json::object();
inline bool Loaded=false,Dirty=false;
inline std::string Status;
inline char Filter[128]{};
inline std::map<std::string,std::string> ArrayText;
inline std::map<std::string,bool> ArrayErrors;
inline std::string Label(std::string value){
    std::string result;for(size_t i=0;i<value.size();++i){const auto c=static_cast<unsigned char>(value[i]);
        if(i&&std::isupper(c)&&!std::isupper(static_cast<unsigned char>(value[i-1])))result+=' ';
        result+=i?static_cast<char>(std::tolower(c)):static_cast<char>(std::toupper(c));}return result;
}
inline std::string Lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline void Accept(const json& result){Snapshot=result;Draft=result.at("Document");Loaded=true;Dirty=false;ArrayText.clear();ArrayErrors.clear();}
inline void Load(){
    json result;if(!Remote||!Remote({{"Action","SettingsRead"}},result)){Status="RuneSchema settings service is unavailable. Update the matching core and RSMK together.";return;}
    if(!result.value("Ok",false)){Status=result.value("Status",std::string("Could not read settings.ini."));return;}
    Accept(result);Status="Changes are saved for the next launch. Running loaders remain unchanged.";
}
inline json Patch(){json result=json::object();if(!Loaded)return result;
    for(const auto& [path,type]:Snapshot.at("Editable").items()){
        const auto pointer=json::json_pointer(path);if(Draft.at(pointer)!=Snapshot.at("Document").at(pointer))result[path]=Draft.at(pointer);
    }return result;
}
inline void Save(){
    const auto patch=Patch();if(patch.empty())return;
    json result;if(!Remote||!Remote({{"Action","SettingsSave"},{"ExpectedContents",Snapshot.at("ExpectedContents")},{"Patch",patch}},result)){
        Status="Save was not confirmed. Reload settings from disk before retrying.";return;}
    if(!result.value("Ok",false)){Status=result.value("Status",std::string("Settings were not saved."));return;}
    Accept(result);Status=result.value("Status",std::string("Saved. Restart Dragonwilds to apply."));
}
inline void SaveQuick(const std::string& path,bool value){
    if(!Loaded||!Patch().empty()){Status="Finish or discard the Settings draft before using quick toggles.";return;}
    json result;if(!Remote||!Remote({{"Action","SettingsSave"},{"ExpectedContents",Snapshot.at("ExpectedContents")},{"Patch",{{path,value}}}},result)){
        Status="Save was not confirmed; reload settings before retrying.";return;}
    if(!result.value("Ok",false)){Status=result.value("Status",std::string("Setting was not saved."));return;}
    Accept(result);Status="Saved for the next game launch. The running session has not changed.";
}
inline void QuickToggle(const char* label,const char* path,bool risky=false){
    const auto pointer=json::json_pointer(path);if(!Snapshot.at("Editable").contains(path)||!Snapshot.at("Document").contains(pointer))return;
    ImGui::PushID(path);bool value=Snapshot.at("Document").at(pointer).get<bool>();const bool dirty=!Patch().empty();ImGui::BeginDisabled(dirty);
    if(ImGui::Checkbox(label,&value)){if(risky&&value)ImGui::OpenPopup("Confirm save bypass");else SaveQuick(path,value);}ImGui::EndDisabled();
    if(ImGui::BeginPopupModal("Confirm save bypass",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+HelpyImGuiTheme::Current.X(390));
        ImGui::TextWrapped("Enable Corrupt Save Bypass for the next launch? Back up your character first and disable any standalone bypass. This does not repair the save; later saves may persist incomplete progress.");ImGui::PopTextWrapPos();
        if(HelpyImGuiTheme::SoftButton("Save for next launch")){SaveQuick(path,true);ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }ImGui::PopID();
}
inline void QuickSettings(){
    static std::string startupStatus;
    bool autoOpen=HelpySettings::OpenOnGameStart();
    if(ImGui::Checkbox("Open Helpy when the game starts",&autoOpen)){HelpySettings::SetOpenOnGameStart(autoOpen);startupStatus=HelpySettings::Message();}
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Opens the Helpy splash once per game launch. F2 still closes/reopens it; it never loads a world or changes saves.");
    if(!startupStatus.empty())ImGui::TextWrapped("%s",startupStatus.c_str());
    if(!Loaded&&Status.empty())Load();
    if(Loaded){
        QuickToggle("Corrupt Save Bypass","/persistence/allowCorruptCharacterEntry",true);
        ImGui::TextWrapped("Bypass is not a repair. Keep a backup; disabling a separate bypass may also be required.");
        ImGui::TextWrapped("Current session: %s",Snapshot.value("BypassStatus",std::string("Not reported")).c_str());
        QuickToggle("Automatic reflection mapping updates","/autoUpdateMappings");
        QuickToggle("Detailed RuneSchema logging","/advancedLogging");
        QuickToggle("Archive RuneSchema logs","/archiveLogs");
        ImGui::TextDisabled("These switches save settings for the next launch, not live changes.");
        if(!Patch().empty())ImGui::TextWrapped("An unsaved Settings draft is open. Save or discard it there before using quick switches.");
    }
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(HelpyImGuiTheme::SoftButton("Reload quick settings"))Load();
}
inline void Field(const std::string& key,json& value,const std::string& path){
    ImGui::PushID(path.c_str());const auto label=Label(key);bool changed=false;
    if(value.is_boolean()){bool v=value.get<bool>();if(ImGui::Checkbox(label.c_str(),&v)){value=v;changed=true;}}
    else if(value.is_number_integer()||value.is_number_unsigned()){
        int64_t v=value.get<int64_t>();ImGui::SetNextItemWidth(180);if(ImGui::InputScalar(label.c_str(),ImGuiDataType_S64,&v)){value=v;changed=true;}
    }else if(value.is_number()){double v=value.get<double>();ImGui::SetNextItemWidth(180);if(ImGui::InputDouble(label.c_str(),&v)){value=v;changed=true;}}
    else if(value.is_string()){
        const auto current=value.get<std::string>();if(current.size()>8192){ImGui::TextWrapped("%s: value is too large for this field; edit the INI externally.",label.c_str());ImGui::PopID();return;}
        std::vector<char> text(8193);std::snprintf(text.data(),text.size(),"%s",current.c_str());ImGui::SetNextItemWidth(300);
        if(ImGui::InputText(label.c_str(),text.data(),text.size())){value=std::string(text.data());changed=true;}
    }else if(value.is_array()){
        auto [it,inserted]=ArrayText.try_emplace(path,value.dump());if(it->second.size()>16384){ImGui::TextWrapped("%s: list is too large for this field; edit the INI externally.",label.c_str());ImGui::PopID();return;}
        std::vector<char> text(16385);std::snprintf(text.data(),text.size(),"%s",it->second.c_str());
        ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);
        if(ImGui::InputText("##array",text.data(),text.size())){it->second=text.data();const auto parsed=json::parse(it->second,nullptr,false);
            ArrayErrors[path]=!parsed.is_array();if(parsed.is_array()){value=parsed;changed=true;}}
        if(ArrayErrors[path])ImGui::TextColored(HelpyImGuiTheme::Current.error,"Enter a JSON list, for example [\"value\"].");
    }
    if(changed)Dirty=true;
    if(path=="/persistence/allowCorruptCharacterEntry"){
        ImGui::TextWrapped("Runs native character validation and player-state loading, then accepts rejected return results at both verified native return gates. Restart required. Disable the standalone bypass first.");
        ImGui::TextWrapped("This does not repair unreadable data. Back up the character; later game saves can persist incomplete progress.");
        ImGui::TextWrapped("Current session: %s",Snapshot.value("BypassStatus",std::string("Not reported")).c_str());
    }
    if(path=="/defaultRecovery/enabled")ImGui::TextWrapped("Default recovery is separate from the validation bypass. Enabling bypass does not enable progress restoration.");
    if(path=="/archiveLogs")ImGui::TextWrapped("Starts a fresh continuous RuneSchema.log at launch. Keep the previous run in logs/ with a UTC date-time filename. Restart required. Archives are not automatically deleted; UE4SS.log is unchanged.");
    ImGui::PopID();
}
inline void Render(){
    if(!Loaded&&Status.empty())Load();
    if(Loaded)Dirty=!Patch().empty();
    ImGui::TextUnformatted("RuneSchema configuration");
    ImGui::TextWrapped("Edit this installation's settings.ini. Saves create a backup and take effect after restarting the game or server; they do not change a connected remote server.");
    if(HelpyImGuiTheme::SoftButton(Dirty?"Discard / reload":"Reload from disk"))Load();
    bool invalid=false;for(const auto& [path,error]:ArrayErrors)invalid|=error;
    ImGui::SameLine();ImGui::BeginDisabled(!Loaded||!Dirty||invalid);
    if(HelpyImGuiTheme::SoftButton("Save settings.ini")){
        const auto pointer=json::json_pointer("/persistence/allowCorruptCharacterEntry");
        if(Draft.at(pointer).get<bool>()&&!Snapshot.at("Document").at(pointer).get<bool>())ImGui::OpenPopup("Enable character validation bypass?");else Save();
    }ImGui::EndDisabled();
    if(ImGui::BeginPopupModal("Enable character validation bypass?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Save the opt-in bypass for the next launch? Keep an original character backup and fully disable CorruptCharacterBypass. This does not guarantee complete character loading or progress retention.");
        if(HelpyImGuiTheme::SoftButton("Save for next launch")){Save();ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(!Loaded)return;
    ImGui::TextDisabled("%s",Snapshot.value("Path",std::string("settings/settings.ini")).c_str());
    ImGui::InputTextWithHint("##settings-filter","Filter settings...",Filter,sizeof(Filter));const auto filter=Lower(Filter);
    const auto matches=[&](const std::string& path){return filter.empty()||Lower(path+" "+Label(path.substr(path.find_last_of('/')+1))).find(filter)!=std::string::npos;};
    const auto& editable=Snapshot.at("Editable");
    const auto render=[&](auto&& self,json& object,const std::string& prefix)->void{
        for(auto& [key,value]:object.items()){
            const auto path=prefix+"/"+key;
            if(value.is_object()){
                bool has=false;for(const auto& [field,type]:editable.items())if(field.starts_with(path+"/")&&matches(field)){has=true;break;}
                if(has&&ImGui::CollapsingHeader(Label(key).c_str(),filter.empty()?0:ImGuiTreeNodeFlags_DefaultOpen))self(self,value,path);
            }else if(editable.contains(path)&&matches(path))Field(key,value,path);
        }
    };
    render(render,Draft,"");
    if(!Snapshot.value("Backup",std::string{}).empty())ImGui::TextWrapped("Backup: %s",Snapshot["Backup"].get_ref<const std::string&>().c_str());
}
}
