#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HelpySettings.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/PluginRuntimeServices.h"
#include <shellapi.h>
#include "Generator/LazyCatalog.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyPreviewHooks.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <vector>
#include <cctype>
#include <algorithm>

namespace PS::RuneSchemaSettingsPanel {
using json=nlohmann::json;
inline std::function<bool(const json&,json&)> Remote;
inline json Snapshot=json::object(),Draft=json::object();
inline bool Loaded=false,Dirty=false;
inline bool QuickToggleError=false;
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
inline void Accept(const json& result){Snapshot=result;Draft=result.at("Document");Loaded=true;Dirty=false;ArrayText.clear();ArrayErrors.clear();HelpySettings::ApplyHotkey(Draft.value("helpyHotkey",std::string("F2")));}
inline void Load(){
    json result;if(!Remote||!Remote({{"Action","SettingsRead"}},result)){Status="RuneSchema settings service is unavailable. Update the matching core and RSMK together.";return;}
    if(!result.value("Ok",false)){Status=result.value("Status",std::string("Could not read settings.ini."));return;}
    Accept(result);Status="The Helpy shortcut applies immediately. Cleanup follows its activation choice; other startup changes require a restart.";
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
    Accept(result);Status=result.value("Status",std::string("Saved. Restart Dragonwilds to apply."))+" The Helpy shortcut is active now.";
}
inline void SaveQuick(const std::string& path,const json& value){
    QuickToggleError=true;
    if(!Loaded||!Patch().empty()){Status="Finish or discard the Settings draft before using quick toggles.";return;}
    json result;if(!Remote||!Remote({{"Action","SettingsSave"},{"ExpectedContents",Snapshot.at("ExpectedContents")},{"Patch",{{path,value}}}},result)){
        Status="Save was not confirmed; reload settings before retrying.";return;}
    if(!result.value("Ok",false)){Status=result.value("Status",std::string("Setting was not saved."));return;}
    Accept(result);QuickToggleError=false;Status=path=="/helpyHotkey"?"Saved. "+HelpySettings::HotkeyHint()+"; active now.":
        (path=="/persistence/blanketCleanup"||path=="/persistence/cleanupOnWorldEntry")?result.value("Status",std::string("Saved. Cleanup follows its activation choice.")):"Saved for the next game launch. The running session has not changed.";
}
inline void QuickToggle(const char* label,const char* path,bool risky=false){
    const auto pointer=json::json_pointer(path);if(!Snapshot.at("Editable").contains(path)||!Snapshot.at("Document").contains(pointer))return;
    ImGui::PushID(path);bool value=Snapshot.at("Document").at(pointer).get<bool>();const bool dirty=!Patch().empty();ImGui::BeginDisabled(dirty);
    if(ImGui::Checkbox(label,&value)){if(risky&&value)ImGui::OpenPopup("Confirm save bypass");else SaveQuick(path,value);}ImGui::EndDisabled();
    if(HelpyImGuiTheme::BeginCenteredModal("Confirm save bypass",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+HelpyImGuiTheme::Current.X(390));
        ImGui::TextWrapped("Use Save Bypass by default next launch, rather than failure-only fallback? Back up your character and disable any standalone bypass. This does not repair the save; later saves may persist incomplete progress.");ImGui::PopTextWrapPos();
        if(HelpyImGuiTheme::SoftButton("Save for next launch")){SaveQuick(path,true);ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }ImGui::PopID();
}
inline void CachePreferences(){
    using namespace HelpyRuntimePreferences;
    ImGui::SeparatorText("Content & cache");
    int order=CacheFirst.load()?0:1;
    bool changed=ImGui::RadioButton("Cache first (recommended)",&order,0);
    changed|=ImGui::RadioButton("Game first, cache fallback",&order,1);
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Cache first opens saved/imported item references immediately. Game first discovers loaded game items in bounded slices, using saved references when a live page is unavailable. Neither mode loads every PAK asset or bypasses gameplay validation.");
    bool passive=PassiveRefresh.load();changed|=ImGui::Checkbox("Passive content updates",&passive);
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Allow bounded background item-catalogue discovery after content changes. Turn off to update through searches and explicit refresh instead. Source verification and live action checks remain active; this does not disable every map or icon update.");
    if(changed){CacheFirst=order==0;PassiveRefresh=passive;HelpyImGuiTheme::SaveCurrent();auto& list=LazyCatalog::For(0);list.Cache.clear();if(list.Started)LazyCatalog::Retry(0);}
    if(HelpyImGuiTheme::Status.starts_with("[RS-CFG-003]"))ImGui::TextWrapped("%s",HelpyImGuiTheme::Status.c_str());
}
inline void QuickSettings(){
    const auto hint=[](const std::string& text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){
        PS::HelpyImGuiTheme::Tooltip("%s",text.c_str());}};
    if(!Loaded&&Status.empty())Load();
    ImGui::SeparatorText("Startup & shortcuts");
    int startup=HelpySettings::OpenOnGameStart()?1:0;
    ImGui::TextUnformatted("When Dragonwilds starts");
    if(ImGui::RadioButton("Start in system tray (default)",&startup,0))HelpySettings::SetOpenOnGameStart(false);
    if(ImGui::RadioButton("Open window at startup",&startup,1))HelpySettings::SetOpenOnGameStart(true);
    hint("The app runs as its own process. The system tray is beside the Windows clock; use the tray icon or your Helpy shortcut to open it. Startup changes apply next game launch.\n"+HelpySettings::Message());
    bool alongside=HelpySettings::StartAlongsideGame();if(ImGui::Checkbox("Start Helpy with Dragonwilds",&alongside))HelpySettings::SetBool("startWithGame",alongside);
    if(PluginRuntimeServices::ExternalDesktop){bool exit=HelpySettings::ExitWhenGameCloses();if(ImGui::Checkbox("Exit Helpy when the game closes",&exit))HelpySettings::SetBool("exitWithGame",exit);
        hint("Applies immediately. Helpy exits fully when its attached game or dedicated server process ends. A temporary bridge disconnect does not close it; starting Helpy offline still works. Off by default.\n"+HelpySettings::Message());}
    bool minimize=HelpySettings::CloseToTaskbar();if(ImGui::Checkbox("Close to system tray",&minimize))HelpySettings::SetBool("minimizeOnClose",minimize);
    hint("X and Alt+F4 return Helpy to the system tray when checked, or exit it when unchecked. Applies immediately.\n"+HelpySettings::Message());
    if(PluginRuntimeServices::ExternalDesktop&&ImGui::Button("Exit Helpy"))PostQuitMessage(0);
    ImGui::TextUnformatted("Helpy shortcut");
    static std::string hotkeyStatus;
    const bool editable=Loaded&&Snapshot.at("Editable").contains("/helpyHotkey")&&Patch().empty();
    ImGui::BeginDisabled(!editable);
    const auto current=HelpySettings::HotkeyName();
    const float resetWidth=ImGui::CalcTextSize("Reset to F2").x+ImGui::GetStyle().FramePadding.x*2;
    ImGui::SetNextItemWidth(std::max(HelpyImGuiTheme::Current.X(55),ImGui::GetContentRegionAvail().x-resetWidth-ImGui::GetStyle().ItemSpacing.x));
    if(ImGui::BeginCombo("##helpy-hotkey",current.c_str())){
        for(const auto& binding:HelpyShortcut::Choices){
            const bool selected=binding.Name==current;
            if(ImGui::Selectable(binding.Name.data(),selected)&&!selected){SaveQuick("/helpyHotkey",std::string(binding.Name));hotkeyStatus=QuickToggleError?Status:"Saved. Shortcut active immediately.";}
            if(selected)ImGui::SetItemDefaultFocus();
        }ImGui::EndCombo();
    }
    hint("Choose one key to open or close Helpy. Saved to [RuneSchema] helpyHotkey in settings.ini and applied immediately. Pick a key that does not conflict with your game or other mods; single-key bindings only.");
    ImGui::SameLine();ImGui::BeginDisabled(current=="F2");
    if(HelpyImGuiTheme::SoftButton("Reset to F2")){SaveQuick("/helpyHotkey","F2");hotkeyStatus=QuickToggleError?Status:"Saved. Shortcut active immediately.";}
    ImGui::EndDisabled();ImGui::EndDisabled();
    if(!editable){ImGui::TextDisabled("Shortcut unavailable. Hover for details.");hint(Loaded&&!Patch().empty()?"Finish or discard the Settings draft before changing this shortcut.":"Update RuneSchema and Helpy together, then reload RuneSchema settings.");}
    if(!hotkeyStatus.empty())ImGui::TextWrapped("%s",hotkeyStatus.c_str());
    if(HelpyImGuiTheme::SoftButton("Reload theme"))HelpyImGuiTheme::RequestReload();
    hint("Reload the current theme from disk. This does not reload mods or game content.");
    if(ImGui::TreeNode("Content & cache")){CachePreferences();ImGui::TreePop();}
    const bool performanceOpen=ImGui::TreeNode("Performance");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringButtons["Quick performance"]=HelpyPreviewHooks::LastRect();
#endif
    if(performanceOpen){
        auto& theme=HelpyImGuiTheme::Current;
        bool save=ImGui::Checkbox("Show FPS in Helpy's window",&theme.showFps);
        hint("Shows engine tick FPS and Helpy's own rendered FPS in the window frame. The counter reads cached game timing only while visible; it does not scan game objects. Dedicated servers show server tick rate.");
        save|=ImGui::Checkbox("Show game FPS in game",&theme.showGameFps);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::AuthoringButtons["Game FPS overlay"]=HelpyPreviewHooks::LastRect();
#endif
        hint("A small counter over the game in windowed/borderless mode. Hiding it stops its timing requests and drawing. Helpy's window can stay closed.");
        ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(150));
        if(ImGui::BeginCombo("FPS shortcut",theme.fpsHotkey.c_str())){
            for(const auto& binding:HelpyShortcut::Choices){
                ImGui::BeginDisabled(binding.Key==HelpySettings::Hotkey.load());
                if(ImGui::Selectable(binding.Name.data(),binding.Name==theme.fpsHotkey)){theme.fpsHotkey=binding.Name;save=true;}
                ImGui::EndDisabled();
            }ImGui::EndCombo();
        }
        hint("Toggle the in-game counter while Dragonwilds or Helpy has focus. F10 by default. Choose a different key from Helpy's own shortcut; function keys avoid typing conflicts.");
        if(const auto* binding=HelpyShortcut::Find(theme.fpsHotkey);binding&&binding->Key==HelpySettings::Hotkey.load())ImGui::TextWrapped("Choose a different FPS shortcut from Helpy's own key.");
        ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(150));
        ImGui::SliderInt("Helpy FPS limit",&theme.helpyFpsLimit,15,144,"%d",ImGuiSliderFlags_AlwaysClamp);
        save|=ImGui::IsItemDeactivatedAfterEdit();
        hint("Limits Helpy rendering only. Default 60, adjustable up to 144; unfocused Helpy is limited to 10 FPS and hidden/minimized windows stop rendering. The game frame-rate limit is unchanged.");
        int interval=theme.fpsRefreshMs<=500?0:theme.fpsRefreshMs<=1000?1:2;
        const char* intervals[]{"0.5 seconds","1 second","2 seconds"};
        ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(150));
        if(ImGui::Combo("Counter refresh",&interval,intervals,3)){theme.fpsRefreshMs=interval==0?500:interval==1?1000:2000;save=true;}
        if(save)HelpyImGuiTheme::SaveCurrent();
        ImGui::TreePop();
    }
    if(ImGui::TreeNodeEx("Game launch & saves",ImGuiTreeNodeFlags_DefaultOpen)){
        static json quick=json::object();static char character[257]{},world[257]{};static int mode=1;static bool dirty=false,unconfirmed=false;static std::string message;
        static bool saved=false;static int choicePolls=0;static double nextChoicePoll=0,retryRead=0;
        const auto load=[&](bool discard){json result;if(!Remote||!Remote({{"Action","QuickLoadRead"}},result)||!result.is_object()||!result.value("Ok",false)){message=result.is_object()?result.value("Status",std::string("Quick Launch service unavailable; use a matching core build.")):"Quick Launch service unavailable; use a matching core build.";return;}
            const auto& selectionDoc=result.at("Document");
            if(!selectionDoc.value("Mode",json{}).is_string()||(selectionDoc["Mode"]!="vanilla"&&selectionDoc["Mode"]!="lessRestrictive"&&selectionDoc["Mode"]!="quickLaunch")){message="Loading modes require a matching RuneSchema core build.";unconfirmed=true;return;}
            if(!discard&&!quick.empty()){
                if(quick.at("ExpectedContents")!=result.at("ExpectedContents")){
                    if(quick.at("Document")!=result.at("Document")){message="Launch options changed on disk. Reload launch options before saving.";unconfirmed=true;return;}
                    quick["ExpectedContents"]=result.at("ExpectedContents"); // Only unrelated settings changed; retain this launch draft.
                }
                quick["Characters"]=result.value("Characters",json::array());quick["Worlds"]=result.value("Worlds",json::array());message=result.value("Status",std::string{});return;
            }
            quick=result;const auto& doc=result.at("Document");const auto selection=doc.at("Mode").get<std::string>();mode=selection=="quickLaunch"?2:selection=="lessRestrictive"?1:0;std::snprintf(character,sizeof(character),"%s",doc.value("CharacterName",std::string{}).c_str());std::snprintf(world,sizeof(world),"%s",doc.value("WorldName",std::string{}).c_str());dirty=false;unconfirmed=false;message=result.value("Status",std::string{});};
        if(quick.empty()&&ImGui::GetTime()>=retryRead){load(true);retryRead=ImGui::GetTime()+1;choicePolls=3;nextChoicePoll=ImGui::GetTime()+.5;}
        if(!quick.empty()&&choicePolls&&ImGui::GetTime()>=nextChoicePoll){load(false);--choicePolls;nextChoicePoll=ImGui::GetTime()+.5;}
        ImGui::BeginDisabled(quick.empty()||quick.value("DedicatedServer",false));
        if(!quick.value("DedicatedServer",false)){
        ImGui::TextUnformatted("Launch mode");
        if(ImGui::RadioButton("Normal",mode!=2)){mode=1;dirty=true;saved=false;}
        if(ImGui::RadioButton("Quick Launch",&mode,2)){dirty=true;saved=false;}
        hint("Both modes use Less Restrictive loading and orphan pruning, with bypass as a fallback after native rejection. Quick Launch opens your selected character/world; hold Shift to skip automatic launch. Applies next launch. Missing content may be lost when the game saves.");
        const auto choose=[&](const char* label,const char* key,char* text){
            ImGui::PushID(key);ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(250));
            if(ImGui::BeginCombo(label,*text?text:"Choose...")){
                for(const auto& row:quick.value(key,json::array())){const auto name=row.value("Name",std::string{});
                    if(ImGui::Selectable(name.c_str(),name==text)){std::snprintf(text,257,"%s",name.c_str());dirty=true;saved=false;}}
                if(quick.value(key,json::array()).empty())ImGui::TextDisabled("No choices cached. Refresh in the main menu.");ImGui::EndCombo();
            }
            hint("Saved for future launches. You can change this while playing without leaving the current world. Native choices refresh in the main menu; refresh and manual entry are under Advanced.");ImGui::PopID();
        };
        if(mode==2){choose("Character","Characters",character);choose("World","Worlds",world);}
        ImGui::BeginDisabled(!dirty||quick.empty()||unconfirmed||!Patch().empty());
        if(HelpyImGuiTheme::SoftButton("Save launch options",{ImGui::GetContentRegionAvail().x,HelpyImGuiTheme::Current.Y(28)})){
            json result;if(Remote&&Remote({{"Action","QuickLoadSave"},{"ExpectedContents",quick.at("ExpectedContents")},{"Document",{{"Mode",mode==2?"quickLaunch":mode==1?"lessRestrictive":"vanilla"},{"CharacterName",character},{"WorldName",world}}}},result)&&result.is_object()&&result.value("Ok",false)){quick=result;dirty=false;saved=true;Load();}else unconfirmed=true;
            message=result.is_object()?result.value("Status",std::string("Quick Launch save unconfirmed; reload before retrying.")):"Quick Launch save unconfirmed; reload before retrying.";
        }ImGui::EndDisabled();
        hint("Saves only launch options in RuneSchema/settings/settings.ini, with a backup. Applies on the next game launch; Save Bypass is unchanged. Finish any open Settings draft first.");
        }
        ImGui::EndDisabled();
        if(unconfirmed||quick.empty()){ImGui::TextColored(HelpyImGuiTheme::Current.error,"Launch settings need attention. Hover for details.");hint(message);}
        else if(saved){ImGui::TextDisabled("Saved for next launch");hint(message);}
        ImGui::Separator();
        if(Loaded){
            QuickToggle("Blanket save cleanup (fallback)","/persistence/blanketCleanup");
            hint("Enabled: use the original broad cleanup across supported local saves at a safe entry boundary. Disabled: limit cleanup to the character/world being loaded. Preview/selection alone does not authorize cleanup. This choice stays saved until changed; backups remain mandatory.");
        const bool savedBlanket=Snapshot.at("Document").at("persistence").value("blanketCleanup",false);
            const bool atEntry=Snapshot.at("Document").at("persistence").value("cleanupOnWorldEntry",false);
            if(Snapshot.at("Editable").contains("/persistence/cleanupOnWorldEntry")){
                int apply=atEntry?0:1;
                ImGui::TextUnformatted("Apply cleanup changes");
                ImGui::BeginDisabled(!Patch().empty());
                bool changed=ImGui::RadioButton("Next character/world entry",&apply,0);
                changed|=ImGui::RadioButton("Next game launch",&apply,1);
                if(changed)SaveQuick("/persistence/cleanupOnWorldEntry",apply==0);
                ImGui::EndDisabled();
                hint("Next entry reads the saved policy at a native load boundary. Next game launch retains the current session's applied policy until restart. Changing this control never starts a background sweep.");
            }
            ImGui::TextDisabled("%s",savedBlanket
                ?"Saved scope: blanket fallback":"Saved scope: selected character/world");
        }
        if(Loaded){QuickToggle("Save Bypass","/persistence/allowCorruptCharacterEntry",true);
            hint("Checked: use bypass by default. Unchecked: Normal, Quick Launch and dedicated servers use relaxed native loading first, then fallback only after native rejection. Native loading runs once. Does not repair unreadable data; keep backups and disable any standalone bypass. Next launch only.\n\nCurrent session: "+Snapshot.value("BypassStatus",std::string("Not reported")));}
        ImGui::TextDisabled("Backup-first save pruning");
        hint("RuneSchema verifies a whole-save backup before removing orphaned mod data. Active quests from disabled mods are removed when their IDs no longer resolve in the certified quest registry. Backups are restored manually; old Recovery archives are not loaded or deleted.");
        if(QuickToggleError||!Loaded){ImGui::TextColored(HelpyImGuiTheme::Current.error,"RuneSchema settings need attention. Hover for details.");hint(Status);}
        if(ImGui::TreeNode("Advanced")){
            ImGui::BeginDisabled(quick.empty()||quick.value("DedicatedServer",false));
            if(!quick.value("DedicatedServer",false)){
            bool vanilla=mode==0;if(ImGui::Checkbox("Vanilla Load",&vanilla)){mode=vanilla?0:1;dirty=true;saved=false;}
            hint("Native strictness; automatic fallback is off. Orphan pruning remains active. The Save Bypass checkbox can still force bypass by default. Applies next launch.");
            const auto choices=[&](const char* label,const char* key,char* text){ImGui::PushID(key);ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(250));dirty|=ImGui::InputText(label,text,257);ImGui::PopID();};
            choices("Character name","Characters",character);hint("Manual native character display name. Ambiguous names are refused at launch.");
            choices("World slot name","Worlds",world);hint("Manual native world slot name, not a save-file path.");
            if(HelpyImGuiTheme::SoftButton("Refresh choices")){load(false);choicePolls=3;nextChoicePoll=ImGui::GetTime()+.5;}hint("Refresh native character/world choices in the main menu without discarding your selection.");ImGui::SameLine();
            if(HelpyImGuiTheme::SoftButton("Reload launch options"))load(true);hint("Discard this launch draft and reread settings.ini. Required after an unconfirmed save.");
            }
            ImGui::EndDisabled();
            if(Loaded){QuickToggle("Automatic reflection mapping updates","/autoUpdateMappings");hint("Refreshes stale .usmap reflection mappings at startup. Does not reset content caches.");
                QuickToggle("Detailed RuneSchema logging","/advancedLogging");hint("Adds detailed RuneSchema diagnostics on the next launch.");
                QuickToggle("Archive RuneSchema logs","/archiveLogs");hint("Preserves previous sessions in logs/. Archives are not automatically deleted.");}
            if(HelpyImGuiTheme::SoftButton("Reload RuneSchema settings")){Load();QuickToggleError=false;}hint(Status);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
}
inline void Field(const std::string& key,json& value,const std::string& path){
    ImGui::PushID(path.c_str());const auto label=path=="/persistence/cleanupOnWorldEntry"?std::string("Apply cleanup changes at next world entry"):Label(key);bool changed=false;
    if(value.is_boolean()){bool v=value.get<bool>();if(ImGui::Checkbox(label.c_str(),&v)){value=v;changed=true;}}
    else if(value.is_number_integer()||value.is_number_unsigned()){
        int64_t v=value.get<int64_t>();ImGui::SetNextItemWidth(180);if(ImGui::InputScalar(label.c_str(),ImGuiDataType_S64,&v)){value=v;changed=true;}
    }else if(value.is_number()){double v=value.get<double>();ImGui::SetNextItemWidth(180);if(ImGui::InputDouble(label.c_str(),&v)){value=v;changed=true;}}
    else if(value.is_string()){
        const auto current=value.get<std::string>();if(current.size()>8192){ImGui::TextWrapped("%s: value is too large for this field; edit the INI externally.",label.c_str());ImGui::PopID();return;}
        std::vector<char> text(8193);std::snprintf(text.data(),text.size(),"%s",current.c_str());ImGui::SetNextItemWidth(std::max(60.f,std::min(300.f,ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(label.c_str()).x-ImGui::GetStyle().ItemInnerSpacing.x)));
        if(ImGui::InputText(label.c_str(),text.data(),text.size(),path=="/helpyAuthority/accessPin"?ImGuiInputTextFlags_Password:0)){value=std::string(text.data());changed=true;}
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
        ImGui::TextWrapped("Use Save Bypass by default. When off, Normal and Quick Launch use failure-only fallback after native rejection. Native loading runs once; no automatic reload. Restart required. Disable the standalone bypass first.");
        ImGui::TextWrapped("Dedicated servers use Less Restrictive loading and failure-only fallback by default; native world startup is unchanged.");
        ImGui::TextWrapped("This does not repair unreadable data. Back up the character; later game saves can persist incomplete progress.");
        ImGui::TextWrapped("Current session: %s",Snapshot.value("BypassStatus",std::string("Not reported")).c_str());
    }
    if(path=="/defaultRecovery/enabled")ImGui::TextWrapped("Default recovery is separate from the validation bypass. Enabling bypass does not enable progress restoration.");
    if(path=="/persistence/blanketCleanup")ImGui::TextWrapped("Fallback across supported local saves. Off selects only the character/world being loaded. Neither choice authorizes preview cleanup. The setting persists until changed; verified backups remain mandatory.");
    if(path=="/persistence/cleanupOnWorldEntry")ImGui::TextWrapped("On: apply the saved cleanup policy at the next character/world entry. Off: retain the current session policy until the next game launch. This does not start a background scan.");
    if(path=="/archiveLogs")ImGui::TextWrapped("Starts a fresh continuous RuneSchema.log at launch. Keep the previous run in logs/ with a UTC date-time filename. Restart required. Archives are not automatically deleted; UE4SS.log is unchanged.");
    ImGui::PopID();
}
inline void Render(){
    if(!Loaded&&Status.empty())Load();
    if(Loaded)Dirty=!Patch().empty();
    ImGui::TextUnformatted("RuneSchema configuration");
    ImGui::TextWrapped("Edit this installation's settings.ini. Saves create a backup. The Helpy shortcut applies immediately; cleanup follows its activation choice. Other startup changes require a restart. These settings do not change a connected remote server.");
    if(HelpyImGuiTheme::SoftButton(Dirty?"Discard / reload":"Reload from disk"))Load();
    bool invalid=false;for(const auto& [path,error]:ArrayErrors)invalid|=error;
    ImGui::SameLine();ImGui::BeginDisabled(!Loaded||!Dirty||invalid);
    if(HelpyImGuiTheme::SoftButton("Save settings.ini")){
        const auto pointer=json::json_pointer("/persistence/allowCorruptCharacterEntry");
        if(Draft.at(pointer).get<bool>()&&!Snapshot.at("Document").at(pointer).get<bool>())ImGui::OpenPopup("Enable character validation bypass?");else Save();
    }ImGui::EndDisabled();
    if(HelpyImGuiTheme::BeginCenteredModal("Enable character validation bypass?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Use Save Bypass by default next launch? Keep an original character backup and disable CorruptCharacterBypass. This does not guarantee complete loading or progress retention.");
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
            if(HelpyEnvironment::Server()&&path=="/quickLaunch")continue;
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
