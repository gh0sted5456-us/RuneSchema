#pragma once
#include "Core/DedicatedServerIni.h"
#include "Core/HelpyEnvironment.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HostServices.h"
#include <commdlg.h>
#include <optional>
#include <nlohmann/json.hpp>
#include "Runtime/HelpyPaths.h"

namespace PS::DedicatedServerPanel {
inline std::filesystem::path File;
inline std::string Original,Status;
inline std::optional<DedicatedServerIni::Document> Draft;
inline bool ShowSecrets=false,Initialized=false,ChooseAfterDiscard=false;
inline char Search[128]{};
inline bool Preview=false;
inline std::filesystem::path Staged;
inline void Stage(){
    if(!HelpyEnvironment::Server()||Preview||!Draft)return;
    Draft->ValidateChanges();(void)Draft->Serialize();
    if(DedicatedServerIni::Read(File)!=Original)throw std::runtime_error("INI changed on disk. Reload before staging; no active configuration was changed.");
    nlohmann::json changes=nlohmann::json::array();for(const auto& line:Draft->Lines)if(line.Editable&&line.Value!=line.Initial)changes.push_back({{"Section",line.Section},{"Key",line.Key},{"Expected",line.Initial},{"Value",line.Value}});
    if(changes.empty())return;
    auto path=File;path+=L".helpy-changes-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".json";
    const auto contents=nlohmann::json{{"Version",1},{"Target",std::filesystem::absolute(File).string()},{"Changes",changes}}.dump(2);
    DedicatedServerIni::Handle output{CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(output.Value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create an adjacent staged change file. Active INI unchanged.");
    DWORD written=0;if(!WriteFile(output.Value,contents.data(),static_cast<DWORD>(contents.size()),&written,nullptr)||written!=contents.size()||!FlushFileBuffers(output.Value))throw std::runtime_error("Staging failed. Active INI unchanged; remove an incomplete adjacent change file before retrying.");
    Staged=path;ShowSecrets=false;Status="Changes staged, not applied. Stop the server, then use the offline apply command. Keep the staged file private: it may contain passwords.";
}
inline void Load(){try{const auto contents=DedicatedServerIni::Read(File);DedicatedServerIni::Document next(contents);Original=contents;Draft=std::move(next);Staged.clear();Status="Loaded from disk. Stage changes here; stop the server before applying them.";ShowSecrets=false;}
    catch(const std::exception& error){Draft.reset();Original.clear();Status=error.what();}}
inline void Choose(){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"DedicatedServer.ini\0DedicatedServer.ini\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if(GetOpenFileNameW(&dialog)){File=file;Load();}}
inline void Render(){
    if(!HelpyEnvironment::Server()){ImGui::TextWrapped("Available only in Helpy running on the dedicated server itself. A connected client cannot edit server configuration.");return;}
    if(!Initialized){Initialized=true;File=HostServices::WorkingDirectory().parent_path().parent_path().parent_path()/"Saved"/"Config"/"WindowsServer"/"DedicatedServer.ini";if(!Preview)Load();}
    const bool dirty=Draft&&Draft->Dirty();
    ImGui::TextWrapped("Local server configuration only. Confirm this is the active profile, especially if you use -UserDir or another save directory. Unknown keys, repeated GUID entries, order and comments are retained.");
    ImGui::TextWrapped("File: %s",File.string().c_str());
    if(ImGui::Button("Select active DedicatedServer.ini")){if(dirty){ChooseAfterDiscard=true;ImGui::OpenPopup("Discard server configuration edits?");}else Choose();}
    ImGui::SameLine();if(ImGui::Button("Reload from disk")){if(dirty){ChooseAfterDiscard=false;ImGui::OpenPopup("Discard server configuration edits?");}else Load();}
    if(HelpyImGuiTheme::BeginCenteredModal("Discard server configuration edits?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Discard unsaved edits before selecting or reloading a file?");if(ImGui::Button("Discard edits")){if(ChooseAfterDiscard)Choose();else Load();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    ImGui::TextColored(HelpyImGuiTheme::Current.warning,"Stage while running; apply only after stopping the server. Live INI edits can be overwritten by the game.");
    ImGui::BeginDisabled(!dirty||Preview);if(ImGui::Button("Stage reviewed changes"))ImGui::OpenPopup("Stage server configuration changes?");ImGui::EndDisabled();
    if(HelpyImGuiTheme::BeginCenteredModal("Stage server configuration changes?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Write a private adjacent change file, leaving the active INI unchanged? Only documented scalar settings are staged. Runtime player/admin records are preserved by the offline apply helper. No live rights are granted.");
        if(ImGui::Button("Confirm stage")){try{Stage();}catch(const std::exception& error){Status=error.what();}ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    ImGui::Checkbox("Reveal sensitive values locally",&ShowSecrets);
    ImGui::TextWrapped("Passwords are masked by default; raw player/admin records stay redacted. Only OwnerId, ServerName, DefaultWorldName, AdminPassword and WorldPassword are editable. Other keys are preserved, not assumed supported.");
    if(!Staged.empty()){
        if(ImGui::Button("Copy offline apply command")){const auto script=HelpyPaths::Asset("Apply-DedicatedServerSettings.ps1");const auto command="powershell.exe -NoProfile -File \""+script.string()+"\" -Changes \""+Staged.string()+"\"";ImGui::SetClipboardText(command.c_str());}
        ImGui::TextWrapped("Staged file: %s",Staged.string().c_str());
    }
    if(ImGui::CollapsingHeader("Native permissions reference")){
        ImGui::TextWrapped("Verified mapping bits: Admin=1, Build=2, OpenChests=4, Chat=8, Owner=128. Mask 14 means Build + OpenChests + Chat, not Admin. OwnerId and known-player UserId are account identities, not character GUIDs.");
        ImGui::TextWrapped("Official guide: owners can ban/unban online or offline players; admins can ban online regular players, not unban. These game roles do not automatically authorize RuneSchema item grants, spawns or teleportation.");
    }
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##server-ini-filter","Filter sections or keys...",Search,sizeof(Search));const auto query=LiveLog::Lower(Search);
    if(Draft){ImGui::BeginChild("Server INI fields",{0,0},true);std::string section;
        for(size_t i=0;i<Draft->Lines.size();++i){auto& line=Draft->Lines[i];if(!line.Editable||(!query.empty()&&LiveLog::Lower(line.Section+" "+line.Key).find(query)==std::string::npos))continue;
            if(line.Section!=section){section=line.Section;ImGui::SeparatorText(section.empty()?"General":section.c_str());}
            ImGui::PushID(static_cast<int>(i));ImGui::TextUnformatted(line.Key.c_str());
            ImGui::TextWrapped("%s",DedicatedServerIni::Description(line.Key));
            if(DedicatedServerIni::Managed(line.Key))ImGui::TextDisabled("Runtime-managed value redacted and preserved.");
            else if(line.Value.size()>8192)ImGui::TextDisabled("Value exceeds editor limit; edit externally.");else{std::array<char,8193> value{};std::copy(line.Value.begin(),line.Value.end(),value.begin());ImGui::SetNextItemWidth(-1);
                const auto flags=!ShowSecrets&&DedicatedServerIni::Sensitive(line.Key)?ImGuiInputTextFlags_Password:ImGuiInputTextFlags_None;
                const bool writable=DedicatedServerIni::Writable(line.Section,line.Key);ImGui::BeginDisabled(!writable);
                if(ImGui::InputText("##value",value.data(),value.size(),flags))line.Value=value.data();ImGui::EndDisabled();}ImGui::PopID();}
        ImGui::EndChild();}
}
}
