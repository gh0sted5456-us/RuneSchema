#pragma once
#include "Core/HelpyIconPack.h"
#include "Generator/ItemIconCache.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyEnvironment.h"
#include "Core/IconCacheBuild.h"
#include <commdlg.h>
#pragma comment(lib,"comdlg32.lib")

namespace PS::HelpyIconPacks {
inline std::future<HelpyIconPack::Result> Worker;
inline HelpyIconPack::Progress Progress;
inline std::string Status;
inline bool Importing=false;
inline std::function<bool(const nlohmann::json&,nlohmann::json&)> RemoteBuild;
inline nlohmann::json BuildState=nlohmann::json::object();
inline std::string SelectedSource;
inline std::chrono::steady_clock::time_point NextBuildRead{};
inline bool Building(){return BuildState.value("Active",false);}
inline void PollBuild(){
    if(!RemoteBuild||std::chrono::steady_clock::now()<NextBuildRead)return;
    NextBuildRead=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    nlohmann::json reply;if(RemoteBuild({{"Action","IconCacheBuildRead"}},reply)&&reply.is_object())BuildState=std::move(reply);
}
inline bool Busy(){return Worker.valid();}
inline void Start(const std::filesystem::path& file,bool importing){
    if(Busy()||Building()){Status="Wait for the active cache build or pack operation to finish.";return;}
    try{
        if(HelpyCacheSources::Lower(file.extension().string())!=".rsmk-icons")throw std::runtime_error("Choose a .rsmk-icons file. ZIP downloads must be unpacked first.");
        if(!HelpySourceCache::Ready()||!HelpySourceCache::Current.Authoritative)throw std::runtime_error("Wait for registered content sources to be ready, then retry.");
        const auto limit=HelpyRuntimePreferences::IconCacheLimitMB.load();if(limit<=0)throw std::runtime_error("Enable Keep icons between launches before using shared packs.");
        const auto snapshot=HelpySourceCache::Current;const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);
        Progress.Cancel=false;Progress.Begin(HelpyIconPack::Phase::Discover);Importing=importing;
        const auto source=SelectedSource;
        Worker=std::async(std::launch::async,[file,root,cache,snapshot,limit,importing,source]{
            return importing?HelpyIconPack::Import(file,root,cache,snapshot,uint64_t(limit)*1024*1024,Progress):HelpyIconPack::Export(file,root,cache,snapshot,Progress,source);
        });Status="Verifying content sources in the background...";
    }catch(const std::exception& error){Status=error.what();}
}
inline void Poll(){PollBuild();if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try{const auto result=Worker.get();Status=result.Message;if(Importing&&result.Imported){ItemIconCache::InvalidateSource();if(ItemIconCache::RemoteResetAccounting)ItemIconCache::RemoteResetAccounting();}}
    catch(const std::exception& error){Status=error.what();if(Importing)ItemIconCache::InvalidateSource();}
}
inline void Shutdown() noexcept {Progress.Cancel=true;if(RemoteBuild)try{nlohmann::json ignored;RemoteBuild({{"Action","IconCacheBuildCancel"}},ignored);}catch(...){}if(Worker.valid())try{(void)Worker.get();}catch(...){};}
inline void Choose(bool importing){
    wchar_t file[32768]{};if(!importing){const auto name=SelectedSource.empty()?std::wstring(L"Helpy icon cache.rsmk-icons"):std::filesystem::path(SelectedSource.substr(SelectedSource.find_last_of('/')+1)+".rsmk-icons").wstring();wcscpy_s(file,name.c_str());}
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Helpy icon cache (*.rsmk-icons)\0*.rsmk-icons\0\0";dialog.lpstrDefExt=L"rsmk-icons";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(importing?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog))Start(file,importing);
}
inline void Render(bool catalogueBusy=false){
    PollBuild();HelpyCacheSources::Plan plan;plan.Registered.clear();plan.Owners=HelpySourceCache::Current.Owners;
    for(const auto& [source,key]:HelpySourceCache::Current.SourceKeys)plan.Registered.insert(source);
    auto choices=ContentSource::Choices(plan);if(BuildState.contains("Sources")&&BuildState["Sources"].is_array())for(const auto& value:BuildState["Sources"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))choices.insert(value.get<std::string>());
    ImGui::BeginDisabled(Busy()||Building());
    ImGui::SetNextItemWidth(std::min(360.0f,ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo("Cache source",SelectedSource.empty()?"All registered sources":SelectedSource.c_str())){
        if(ImGui::Selectable("All registered sources",SelectedSource.empty()))SelectedSource.clear();for(const auto& source:choices)if(ImGui::Selectable(source.c_str(),SelectedSource==source))SelectedSource=source;ImGui::EndCombo();}
    ImGui::EndDisabled();
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy||!RemoteBuild||!BuildState.value("Available",false)||!HelpySourceCache::Ready()||HelpyEnvironment::Server());
    if(ImGui::Button("Build registered icon cache (menu or world)")){
        nlohmann::json result;if(RemoteBuild({{"Action","IconCacheBuildStart"},{"Source",SelectedSource}},result)&&result.value("Accepted",false)){BuildState={{"Active",true},{"Stage","Queued"}};NextBuildRead={};}
        else Status="Cache builder is unavailable or already running.";
    }ImGui::EndDisabled();
    ImGui::TextWrapped("No character/world login is required: the existing game menu renderer captures registered icons. No save, inventory or gameplay world is created. Core, plugin mounts and mod-folder groups can be built/exported separately.");
    if(Building()){
        const auto stage=BuildState.value("Stage",std::string("Queued"));const bool capture=stage=="Caching registered icons";
        const auto done=BuildState.value(capture?"Done":"ScanDone",size_t{}),total=BuildState.value(capture?"Total":"ScanTotal",size_t{});
        const bool measurable=capture||stage=="Discovering registered icons";
        const float fraction=total&&measurable?std::min(1.0f,float(done)/float(total)):-static_cast<float>(ImGui::GetTime());ImGui::ProgressBar(fraction,{-1,0},stage.c_str());
        if(total)ImGui::Text("%zu / %zu %s",done,total,capture?"icons":"records");
        ImGui::Text("Cached %zu · captured %zu · failed %zu",BuildState.value("CacheHits",size_t{}),BuildState.value("Captured",size_t{}),BuildState.value("Failed",size_t{}));
        if(const auto failed=BuildState.value("DiscoveryFailed",size_t{}))ImGui::Text("%zu item/building icon owners could not be inspected",failed);
        if(ImGui::SmallButton("Cancel cache build")){nlohmann::json ignored;RemoteBuild({{"Action","IconCacheBuildCancel"}},ignored);}
    }else if(BuildState.contains("Total"))ImGui::Text("Cache build: %s · %zu / %zu icons · %zu failed",BuildState.value("Stage",std::string{}).c_str(),BuildState.value("Done",size_t{}),BuildState.value("Total",size_t{}),BuildState.value("Failed",size_t{}));
    if(!Building()&&BuildState.value("DiscoveryFailed",size_t{}))ImGui::Text("Icon-owner discovery failures: %zu",BuildState.value("DiscoveryFailed",size_t{}));
    if(!BuildState.value("Error",std::string{}).empty())ImGui::TextWrapped("%s",BuildState["Error"].get_ref<const std::string&>().c_str());
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy);if(ImGui::Button("Import icon cache"))Choose(true);ImGui::SameLine();if(ImGui::Button("Dump / export cached icons"))Choose(false);ImGui::EndDisabled();
    ImGui::TextWrapped("Build first, then export the selected source as an optional .rsmk-icons attachment. Shared thumbnails are deduplicated. Matching cached icons load first; missing icons are captured and saved as needed.");
    ImGui::TextWrapped("Drop a .rsmk-icons file onto this window to import. Only matching installed content is used. Packs contain pictures, not items or mods.");
    if(Busy()){
        const auto phase=Progress.Stage.load();const auto total=Progress.Total.load(),done=Progress.Done.load();const auto bytes=Progress.BytesDone.load(),byteTotal=Progress.BytesTotal.load();
        const char* label=phase==HelpyIconPack::Phase::Discover?"Finding cached icons":phase==HelpyIconPack::Phase::Verify?"Checking content sources":phase==HelpyIconPack::Phase::Validate?"Validating icons":phase==HelpyIconPack::Phase::Write?"Writing icon cache":phase==HelpyIconPack::Phase::Commit?"Saving file":"Complete";
        const bool measurable=phase!=HelpyIconPack::Phase::Commit&&(byteTotal||total);
        const float fraction=measurable?std::min(1.0f,byteTotal?float(bytes)/float(byteTotal):float(done)/float(total)):-static_cast<float>(ImGui::GetTime());
        ImGui::ProgressBar(fraction,{-1,0},Progress.Cancel?"Cancelling safely...":label);
        if(total)ImGui::Text("%zu / %zu %s",std::min(done,total),total,phase==HelpyIconPack::Phase::Verify?"source files":"icons");else ImGui::Text("%zu files inspected",done);
        if(byteTotal)ImGui::Text("%.1f / %.1f MB",double(bytes)/(1024*1024),double(byteTotal)/(1024*1024));
        ImGui::BeginDisabled(Progress.Cancel.load());if(ImGui::SmallButton("Cancel icon operation"))Progress.Cancel=true;ImGui::EndDisabled();
    }
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
}
}
