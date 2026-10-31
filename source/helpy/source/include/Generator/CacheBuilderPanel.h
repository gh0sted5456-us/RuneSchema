#pragma once
#include "Generator/HelpyMenu.h"
#include "Runtime/HelpyModCacheBuilder.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyPreviewHooks.h"
#include <commdlg.h>
#include <shlobj.h>
#include <array>
#include <map>

namespace PS::CacheBuilderPanel {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline bool Requested=false,SelectedTab=false,CreationVisible=false,SelectInstalled=false;
inline std::string Owner,Notice;
inline fs::path Root,Folder;
inline std::vector<fs::path> Inputs;
inline int Platform=0;
inline std::map<std::string,std::array<char,2049>> Fields;
inline constexpr std::array Keys{"ModID","Name","Author","Version","Nickname","Build","Website","NexusModID","NexusFileID","Description","Banner"};
#if defined(HELPY_PREVIEW)
inline int PreviewTab=-1;
#endif
inline void FocusInstalled(const Json& snapshot,const Json& row){Root=fs::u8path(snapshot.value("Root",std::string{}));Folder=fs::u8path(row.value("Folder",std::string{}));Owner=row.value("Name",std::string{});SelectedTab=false;SelectInstalled=true;Requested=true;Notice.clear();}
inline void AddInput(const fs::path& path){
    if(Inputs.size()>=64){Notice="Choose at most 64 inputs.";return;}
    std::error_code error;if(!fs::exists(path,error)){Notice="That source is unavailable.";return;}
    if(std::find(Inputs.begin(),Inputs.end(),path)==Inputs.end())Inputs.push_back(path);
    if(!Fields["ModID"][0]){auto name=(fs::is_directory(path,error)?path.filename():path.stem()).string();for(auto& c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_'&&c!='-'&&c!=' ')c='_';std::snprintf(Fields["ModID"].data(),129,"%s",name.c_str());}
}
inline bool DropTarget(){return CreationVisible;}
inline void ChooseFiles(){
    wchar_t buffer[32768]{};OPENFILENAMEW picker{sizeof(picker)};picker.hwndOwner=GetActiveWindow();picker.lpstrFile=buffer;picker.nMaxFile=32768;
    picker.lpstrFilter=L"Mod sources (*.zip;*.7z;*.pak;*.utoc;*.ucas;*.png)\0*.zip;*.7z;*.pak;*.utoc;*.ucas;*.png\0\0";
    picker.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
    if(GetOpenFileNameW(&picker)){const fs::path first(buffer);const auto* next=buffer+wcslen(buffer)+1;if(!*next)AddInput(first);else while(*next){AddInput(first/next);next+=wcslen(next)+1;}}
}
inline void ChooseFolder(){BROWSEINFOW picker{};picker.hwndOwner=GetActiveWindow();picker.lpszTitle=L"Choose a RuneSchema loader folder or UE4SS mod folder";picker.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    if(auto* item=SHBrowseForFolderW(&picker)){wchar_t path[32768]{};if(SHGetPathFromIDListW(item,path))AddInput(path);CoTaskMemFree(item);}}
inline void Dependencies(){
    if(ImGui::CollapsingHeader("Dependencies")){
        auto& setting=HelpyImGuiTheme::Current.autoInstallAuthoringDotNet;
        if(HelpyImGuiTheme::Toggle("Set up authoring .NET automatically when missing",&setting))HelpyImGuiTheme::SaveCurrent();
        ImGui::TextWrapped("Uses an existing .NET 10 SDK first. Otherwise, the builder downloads Microsoft's SDK, verifies its SHA-512 checksum, and installs a private copy under RuneSchema/core/tools in AppData. No system PATH changes. First use also restores the locked archive/asset-reader dependencies.");
        ImGui::TextWrapped("ZIP and 7z reading, PAK indexing and packaging run in the offline worker. Keep the installation used for reading game assets closed. No code from your input mod is run by this tool.");
    }
}
inline void Render(const Json& snapshot,const Json& rows){
    const auto& t=HelpyImGuiTheme::Current;CreationVisible=false;
    ImGui::TextWrapped("Build an installed mod's cache, or create a ready-to-distribute RuneSchema ZIP from archives and loose loader folders.");
    int wanted=SelectInstalled?0:-1;
#if defined(HELPY_PREVIEW)
    if(PreviewTab>=0)wanted=PreviewTab;
#endif
    int tab=wanted>=0?wanted:SelectedTab?1:0;constexpr std::array labels{"Installed","Creation"};
    HelpyMenu::Tabs("cache-builder",labels,tab);SelectedTab=tab==1;SelectInstalled=false;
    const auto available=ImGui::GetContentRegionAvail();const float left=std::max(1.f,(available.x-ImGui::GetStyle().ItemSpacing.x)*.52f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.panel);
    if(ImGui::BeginChild("##cache-builder-input",{left,0},true)){
        ImGui::BeginDisabled(HelpyModCacheBuilder::Current().Running);
        if(!SelectedTab){
            ImGui::SeparatorText("Installed mod");
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##cache-installed-mod",Owner.empty()?"Choose installed mod":Owner.c_str())){
                if(rows.is_array())for(const auto& row:rows)if(row.value("Present",false)&&ImGui::Selectable(row.value("Name",std::string{}).c_str(),Owner==row.value("Name",std::string{})))FocusInstalled(snapshot,row);
                ImGui::EndCombo();
            }
            if(!Folder.empty())ImGui::TextWrapped("%s",Folder.string().c_str());
            ImGui::TextWrapped("Reads the entire selected mod's cooked inputs, reuses unchanged cache entries, then publishes /cache. The previous cache is retained if anything fails.");
            ImGui::BeginDisabled(Folder.empty());
            if(HelpyImGuiTheme::SoftButton("Start cache build",{-1,0}))HelpyModCacheBuilder::StartInstalled(Root,Folder,Owner,t.autoInstallAuthoringDotNet);
            ImGui::EndDisabled();
        }else{
            CreationVisible=true;ImGui::SeparatorText("Creation & conversion");
            ImGui::TextWrapped("Drop a .zip/.7z, a RuneSchema mod or loader folder, a UE4SS mod folder, or loose PAK trio files into this page.");
            if(HelpyImGuiTheme::SoftButton("Add files..."))ChooseFiles();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Add folder..."))ChooseFolder();
            if(!Inputs.empty()&&HelpyImGuiTheme::SoftButton("Clear inputs"))Inputs.clear();
            for(size_t i=0;i<Inputs.size();++i){ImGui::PushID(static_cast<int>(i));ImGui::TextWrapped("%s",Inputs[i].string().c_str());if(ImGui::SmallButton("Remove")){Inputs.erase(Inputs.begin()+i);ImGui::PopID();break;}ImGui::PopID();}
            ImGui::SeparatorText("ID.txt");
            for(const auto* key:Keys){ImGui::TextUnformatted(key==std::string_view("ModID")?"Mod ID / folder name":key);ImGui::SetNextItemWidth(-1);ImGui::PushID(key);ImGui::InputText("##field",Fields[key].data(),Fields[key].size());ImGui::PopID();}
            if(!Fields["Author"][0])ImGui::TextDisabled("Mod ID and Author are required.");
            ImGui::TextUnformatted("Package for");ImGui::SetNextItemWidth(-1);ImGui::Combo("##package-platform",&Platform,"Steam / Epic / Server (Win64)\0Game Pass (WinGDK)\0");
            ImGui::TextWrapped("The new ZIP goes to Downloads with Binaries/ and Content/ roots, ID.txt, the rebuilt cache, and Content/Paks/~mods/runeschema.txt. Original inputs stay untouched. Native UE4SS DLL mods retain their normal ue4ss/Mods path and produce a warning.");
            ImGui::BeginDisabled(Inputs.empty()||!Fields["ModID"][0]||!Fields["Author"][0]);
            if(HelpyImGuiTheme::SoftButton("Start",{-1,0})){
                Json metadata=Json::object();for(const auto* key:Keys)metadata[key]=Fields[key].data();
                HelpyModCacheBuilder::StartCreation(Inputs,metadata,Platform?"WinGDK":"Win64",t.autoInstallAuthoringDotNet);
            }ImGui::EndDisabled();
        }
        ImGui::EndDisabled();Dependencies();if(!Notice.empty())ImGui::TextWrapped("%s",Notice.c_str());
    }ImGui::EndChild();ImGui::SameLine();
    if(ImGui::BeginChild("##cache-builder-progress",{0,0},true)){
        const auto& job=HelpyModCacheBuilder::Current();ImGui::SeparatorText("Progress & output");
        if(!job.Owner.empty())ImGui::TextWrapped("%s",job.Owner.c_str());
        ImGui::TextWrapped("%s",job.Stage.c_str());ImGui::ProgressBar(job.Percent/100.f,{-1,0});
        if(job.Running&&!job.Repair){
            const bool paused=job.State=="Paused"||job.State=="Pausing";
            if(HelpyImGuiTheme::SoftButton(paused?"Resume":"Pause"))HelpyModCacheBuilder::Control(paused?"run":"pause");ImGui::SameLine();
            ImGui::BeginDisabled(job.State=="Stopping");if(HelpyImGuiTheme::SoftButton("Stop"))HelpyModCacheBuilder::Control("stop");ImGui::EndDisabled();
            ImGui::TextWrapped("Pause and Stop wait for safe checkpoints. An SDK restore/build or a single asset decode can finish its current step first. Final publication completes as one operation.");
        }
        if(!job.Output.empty()){ImGui::TextUnformatted("Output");ImGui::TextWrapped("%s",job.Output.string().c_str());if(HelpyImGuiTheme::SoftButton("Open output folder"))ShellExecuteW(nullptr,L"open",job.Output.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        if(!job.Job.empty()&&HelpyImGuiTheme::SoftButton("Open job logs"))ShellExecuteW(nullptr,L"open",job.Job.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        for(const auto& warning:job.Warnings){ImGui::PushStyleColor(ImGuiCol_Text,t.warning);ImGui::TextWrapped("%s",warning.c_str());ImGui::PopStyleColor();}
    }ImGui::EndChild();ImGui::PopStyleColor();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheBuilderCreation=SelectedTab;
#endif
}
}
