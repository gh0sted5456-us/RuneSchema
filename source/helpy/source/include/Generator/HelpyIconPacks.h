#pragma once
#include "Core/HelpyIconPack.h"
#include "Core/HelpyModIconPacks.h"
#include "Core/HelpyTaskbar.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/HelpyBackgrounds.h"
#include "Generator/HelpyLoadingIndicator.h"
#include "Generator/ItemIconCache.h"
#include "Runtime/HostServices.h"
#include "Runtime/HelpyDownloads.h"
#include "Core/HelpyEnvironment.h"
#include "Core/IconCacheBuild.h"
#include "Core/HelpyPreviewHooks.h"
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib,"comdlg32.lib")

namespace PS::HelpyIconPacks {
inline std::future<HelpyIconPack::Result> Worker;
inline HelpyIconPack::Progress Progress;
inline std::string Status;
inline bool Importing=false;
inline bool ServerDisplayOnly=true;
inline bool Open=false,Detached=false,Advanced=false;
inline HelpyWindowChrome::State Chrome;
inline std::filesystem::path ActiveFile,LastFile;
inline bool LastOperationOK=false;
inline bool Packaged=false;
inline bool CompletionNotify=false;
inline std::string PackagedKey;
inline std::function<bool(const nlohmann::json&,nlohmann::json&)> RemoteBuild;
inline nlohmann::json BuildState=nlohmann::json::object();
inline std::string SelectedSource;
inline std::filesystem::path PendingBuildExport;
inline std::chrono::steady_clock::time_point NextBuildRead{};
inline bool Building(){return BuildState.value("Active",false);}
inline void PollBuild(){
    if(!RemoteBuild||std::chrono::steady_clock::now()<NextBuildRead)return;
    NextBuildRead=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    nlohmann::json reply;if(RemoteBuild({{"Action","IconCacheBuildRead"}},reply)&&reply.is_object())BuildState=std::move(reply);
}
inline bool Busy(){return Worker.valid();}
inline void Start(const std::filesystem::path& file,bool importing,const nlohmann::json& coverage=nlohmann::json::object()){
    if(Busy()||Building()){Status="Wait for the active cache build or pack operation to finish.";return;}
    try{
        if(HelpyCacheSources::Lower(file.extension().string())!=".rsmk-icons")throw std::runtime_error("Choose a .rsmk-icons file. ZIP downloads must be unpacked first.");
        if(!HelpySourceCache::Ready()||!HelpySourceCache::Current.Authoritative)throw std::runtime_error("Wait for registered content sources to be ready, then retry.");
        const auto limit=HelpyRuntimePreferences::IconCacheLimitMB.load();if(limit<=0)throw std::runtime_error("Enable Keep icons between launches before using shared packs.");
        const auto snapshot=HelpySourceCache::Current;const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);
        Progress.Cancel=false;Progress.Begin(HelpyIconPack::Phase::Discover);Importing=importing;Packaged=false;CompletionNotify=false;ActiveFile=file;LastOperationOK=false;
        const auto source=SelectedSource;
        const auto policy=HelpyEnvironment::Server()&&ServerDisplayOnly?HelpyIconPack::ImportPolicy::ServerDisplayOnly:HelpyIconPack::ImportPolicy::VerifiedContent;
        Worker=std::async(std::launch::async,[file,root,cache,snapshot,limit,importing,source,policy,coverage]{
            return importing?HelpyIconPack::Import(file,root,cache,snapshot,uint64_t(limit)*1024*1024,Progress,policy,source):HelpyIconPack::Export(file,root,cache,snapshot,Progress,source,coverage);
        });Status="Verifying content sources in the background...";
    }catch(const std::exception& error){Status=error.what();}
}
inline void Poll(){PollBuild();
    static std::filesystem::path downloaded;
    if(downloaded.empty())downloaded=HelpyDownloads::TakeBaseCache();
    if(!downloaded.empty()&&!Busy()&&!Building()){
        if(HelpySourceCache::Ready()&&HelpySourceCache::Current.Authoritative){const auto savedSource=SelectedSource;SelectedSource.clear();Start(downloaded,true);SelectedSource=savedSource;downloaded.clear();}
    }
    if(!PendingBuildExport.empty()&&!Building()){
        const auto file=PendingBuildExport;
        const auto stage=BuildState.value("Stage",std::string{});
        if(stage=="Complete"||stage=="Completed with failures"){
            nlohmann::json coverage;
            if(RemoteBuild&&RemoteBuild({{"Action","IconCacheBuildRead"},{"IncludeManifest",true}},coverage)
                &&coverage.value("Sequence",uint64_t{})==BuildState.value("Sequence",uint64_t{})){
                if(!HelpySourceCache::Ready()||coverage.value("SourceKeys",nlohmann::json::object())!=nlohmann::json(HelpySourceCache::Current.SourceKeys)){
                    Status="Finishing source verification before export...";return;
                }
                PendingBuildExport.clear();Start(file,false,coverage);
            }else{PendingBuildExport.clear();Status="Build coverage could not be retrieved. Retry Export; cached icons are retained.";}
        }
        else {PendingBuildExport.clear();Status="Build/export stopped; completed thumbnails remain cached. No incomplete export was started.";}
    }
    if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try{const auto result=Worker.get();Status=result.Message;CompletionNotify=!Packaged||HelpyModIconPacks::NotifyAutomatic(result);if(!Packaged){LastFile=ActiveFile;LastOperationOK=true;}if(Importing&&result.Imported){ItemIconCache::InvalidateIcons(result.ChangedIcons);if(ItemIconCache::RemoteResetAccounting)ItemIconCache::RemoteResetAccounting();}}
    catch(const std::exception& error){Status=error.what();CompletionNotify=true;if(Importing)ItemIconCache::InvalidateSource();}
}
inline void PollPackaged(){
    if(Busy()||Building()||!PendingBuildExport.empty()||!HelpySourceCache::Ready()||!HelpySourceCache::Current.Authoritative)return;
    const auto limit=HelpyRuntimePreferences::IconCacheLimitMB.load();if(limit<=0)return;
    const auto snapshot=HelpySourceCache::Current;if(PackagedKey==snapshot.Key)return;PackagedKey=snapshot.Key;
    const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);
    Progress.Cancel=false;Progress.Begin(HelpyIconPack::Phase::Discover);Importing=true;Packaged=true;CompletionNotify=false;
    Worker=std::async(std::launch::async,[root,cache,snapshot,limit]{return HelpyModIconPacks::ImportAll(root,cache,snapshot,uint64_t(limit)*1024*1024,Progress);});
}
inline void Shutdown() noexcept {PendingBuildExport.clear();Progress.Cancel=true;if(RemoteBuild)try{nlohmann::json ignored;RemoteBuild({{"Action","IconCacheBuildCancel"}},ignored);}catch(...){}if(Worker.valid())try{(void)Worker.get();}catch(...){};}
inline void Choose(bool importing,bool buildFirst=false){
    wchar_t file[32768]{};if(!importing){const auto name=SelectedSource.empty()?std::wstring(L"Helpy icon cache.rsmk-icons"):std::filesystem::path(SelectedSource.substr(SelectedSource.find_last_of('/')+1)+".rsmk-icons").wstring();wcscpy_s(file,name.c_str());}
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Helpy icon cache (*.rsmk-icons)\0*.rsmk-icons\0\0";dialog.lpstrDefExt=L"rsmk-icons";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(importing?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog)){
        if(buildFirst){
            if(Busy()||Building()||!RemoteBuild){Status="Wait for the active cache operation to finish.";return;}
            nlohmann::json reply;
            if(RemoteBuild({{"Action","IconCacheBuildStart"},{"Source",SelectedSource}},reply)&&reply.value("Accepted",false)){
                PendingBuildExport=file;BuildState={{"Active",true},{"Stage","Queued"}};NextBuildRead={};
                ActiveFile=file;LastOperationOK=false;Status="Building missing icons, then exporting. Any incomplete coverage will be reported as warnings.";
            }else Status="Registered icon capture could not start. No export was written.";
        }else Start(file,importing);
    }
}
inline void Render(bool catalogueBusy=false){
    PollBuild();HelpyCacheSources::Plan plan;plan.Registered.clear();plan.Owners=HelpySourceCache::Current.Owners;
    for(const auto& [source,key]:HelpySourceCache::Current.SourceKeys)plan.Registered.insert(source);
    auto choices=ContentSource::Choices(plan);if(BuildState.contains("Sources")&&BuildState["Sources"].is_array())for(const auto& value:BuildState["Sources"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))choices.insert(value.get<std::string>());
    ImGui::BeginDisabled(Busy()||Building());
    ImGui::TextDisabled("Source");ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##Cache source",SelectedSource.empty()?"All registered sources":SelectedSource.c_str())){
        if(ImGui::Selectable("All registered sources",SelectedSource.empty()))SelectedSource.clear();for(const auto& source:choices)if(ImGui::Selectable(source.c_str(),SelectedSource==source))SelectedSource=source;ImGui::EndCombo();}
    ImGui::EndDisabled();
    if(!HelpySourceCache::Ready())ImGui::TextDisabled("Waiting for the running game's mounted-content manifest...");
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy);
    if(ImGui::Button("Import..."))Choose(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy||!RemoteBuild||!BuildState.value("Available",false)||!HelpySourceCache::Ready()||HelpyEnvironment::Server());
    if(ImGui::Button("Export..."))Choose(false,true);
    ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy||!RemoteBuild||!BuildState.value("Available",false)||!HelpySourceCache::Ready()||HelpyEnvironment::Server());
    if(ImGui::Button("Build / update")){
        nlohmann::json result;if(RemoteBuild({{"Action","IconCacheBuildStart"},{"Source",SelectedSource}},result)&&result.value("Accepted",false)){BuildState={{"Active",true},{"Stage","Queued"}};NextBuildRead={};}
        else Status="Cache builder is unavailable or already running.";
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped(HelpyEnvironment::Server()?"Import client packs for server visuals. Native icon capture is unavailable on dedicated servers.":"Import a pack, or build from the game. Matching icons load from cache next launch; only differences are captured.");
    ImGui::BeginDisabled(Busy()||Building()||HelpyDownloads::Busy()||HelpyRuntimePreferences::IconCacheLimitMB.load()<=0||!HelpySourceCache::Ready()||!HelpySourceCache::Current.Authoritative);
    if(ImGui::Button("Download base-game cache"))HelpyDownloads::StartBaseCache(HelpySourceCache::Current.SourceKeys,HelpySourceCache::Current.NativePlugins,HelpyEnvironment::Label(),HelpyEnvironment::GameVersion);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("Looks for a published pack matching active native sources. SHA-256 and the existing import compatibility checks must pass. Mods keep their own caches; a missing pack never blocks local Build / update.");
    if(!HelpyDownloads::Current().Status.empty())ImGui::TextWrapped("%s",HelpyDownloads::Current().Status.c_str());
    if(ImGui::TextLink("Open Helpy cache folder")){
        const auto folder=HostServices::CacheDirectory();std::error_code error;std::filesystem::create_directories(folder,error);
        if(!error)ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    }
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",HostServices::CacheDirectory().string().c_str());
    ImGui::Checkbox("Advanced",&Advanced);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheAdvanced=HelpyPreviewHooks::LastRect();
#endif
    if(Building()){
        const auto stage=BuildState.value("Stage",std::string("Queued"));const bool verifying=stage=="Verifying complete cache";const bool capture=verifying||stage=="Caching registered icons"||stage=="Checking cached icons";
        const auto done=BuildState.value(verifying?"VerifyDone":capture?"Done":"ScanDone",size_t{}),total=BuildState.value(capture?"Total":"ScanTotal",size_t{});
        const bool measurable=capture||stage=="Discovering registered icons";
        const float fraction=total&&measurable?std::min(1.0f,float(done)/float(total)):-static_cast<float>(ImGui::GetTime());ImGui::ProgressBar(fraction,{-1,0},stage.c_str());
        HelpyLoadingIndicator::HoverDetails(stage,"Source: "+SelectedSource+"\nCompleted: "+std::to_string(done)+" / "+std::to_string(total)+"\n"+BuildState.value("Error",std::string{})+"\nMatching local icons are retained; missing/changed icons are requested from the runtime.");
        if(stage=="Discovering item icons")ImGui::Text("%zu items inspected",BuildState.value("ItemRows",size_t{}));
        else if(total)ImGui::Text("%zu / %zu %s",done,total,capture?"icons":"records");
        if(stage=="More cache space needed"){
            const auto required=BuildState.value("RequiredMB",size_t{});const auto recommended=std::clamp(int(required+std::max(size_t(16),required/10)),1,16384);
            ImGui::TextWrapped("This selection needs at least %zu MB. Increase the limit to keep the full batch instead of evicting its first icons.",required);
            if(ImGui::Button(("Use "+std::to_string(recommended)+" MB and continue").c_str())){
                HelpyRuntimePreferences::IconCacheLimitMB=recommended;HelpyImGuiTheme::SaveCurrent();
            }
        }
        if(Advanced)ImGui::Text("Cached %zu · captured %zu · failed %zu",BuildState.value("CacheHits",size_t{}),BuildState.value("Captured",size_t{}),BuildState.value("Failed",size_t{}));
        if(const auto failed=BuildState.value("DiscoveryFailed",size_t{}))ImGui::Text("%zu item/building icon owners could not be inspected",failed);
        if(ImGui::SmallButton("Cancel cache build")){PendingBuildExport.clear();nlohmann::json ignored;RemoteBuild({{"Action","IconCacheBuildCancel"}},ignored);}
    }else if(BuildState.contains("Total"))ImGui::TextWrapped("%s: %zu / %zu verified icons; %zu capture failures",BuildState.value("Stage",std::string{}).c_str(),BuildState.value("Verified",size_t{}),BuildState.value("Total",size_t{}),BuildState.value("Failed",size_t{}));
    if(!Building()&&BuildState.value("DiscoveryFailed",size_t{}))ImGui::Text("Icon-owner discovery failures: %zu",BuildState.value("DiscoveryFailed",size_t{}));
    if(!BuildState.value("Error",std::string{}).empty())ImGui::TextWrapped("Warning: %s",BuildState["Error"].get_ref<const std::string&>().c_str());
    if(Advanced){
    ImGui::TextWrapped("Writable cache: %s",HostServices::CacheDirectory().string().c_str());
    ImGui::TextWrapped("Mod authors can ship cache/*.rsmk-icons inside their RuneSchema mod. Matching active sources are imported into Helpy's AppData cache; mod-provided packs are never edited.");
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy);
    if(HelpyEnvironment::Server()){
        ImGui::Checkbox("Use client packs as display-only thumbnails",&ServerDisplayOnly);
        ImGui::TextWrapped("Matches exact icon paths in active server sources, even when the server cook omits textures. These pictures may differ from the server version; they are not item definitions and are excluded from verified cache exports.");
    }
    if(ImGui::Button("Export cached only (partial)..."))Choose(false);ImGui::EndDisabled();
    int megabytes=HelpyRuntimePreferences::IconCacheLimitMB.load();ImGui::BeginDisabled(Busy()||Building());
    if(ImGui::InputInt("Cache limit (MB)",&megabytes,64,256)){HelpyRuntimePreferences::IconCacheLimitMB.store(std::clamp(megabytes,1,16384));HelpyImGuiTheme::SaveCurrent();}ImGui::EndDisabled();
    ImGui::TextWrapped("Packs contain preview pixels, exact asset paths, source mappings and coverage. Imports merge matching sources automatically; missing mods are not registered. No world login is needed for menu capture.");
    if(BuildState.contains("Issues")&&ImGui::TreeNode("Build warnings")){
        for(const auto& issue:BuildState["Issues"])ImGui::TextWrapped("%s: %s",issue.value("Path",std::string{}).c_str(),issue.value("Reason",std::string{}).c_str());
        ImGui::TextDisabled("Full diagnostic report: cache/icon-build-report.json");ImGui::TreePop();
    }
    }
    if(Busy()){
        const auto phase=Progress.Stage.load();const auto total=Progress.Total.load(),done=Progress.Done.load();const auto bytes=Progress.BytesDone.load(),byteTotal=Progress.BytesTotal.load();
        const char* label=phase==HelpyIconPack::Phase::Discover?"Finding cached icons":phase==HelpyIconPack::Phase::Verify?"Checking content sources":phase==HelpyIconPack::Phase::Validate?"Validating icons":phase==HelpyIconPack::Phase::Write?"Writing icon cache":phase==HelpyIconPack::Phase::Commit?"Saving file":"Complete";
        const bool measurable=phase!=HelpyIconPack::Phase::Commit&&(byteTotal||total);
        const float fraction=measurable?std::min(1.0f,byteTotal?float(bytes)/float(byteTotal):float(done)/float(total)):-static_cast<float>(ImGui::GetTime());
        ImGui::ProgressBar(fraction,{-1,0},Progress.Cancel?"Cancelling safely...":label);
        HelpyLoadingIndicator::HoverDetails(label,"Source: "+SelectedSource+"\nCompleted: "+std::to_string(done)+" / "+std::to_string(total)+"\nBytes: "+std::to_string(bytes)+" / "+std::to_string(byteTotal)+"\n"+Status);
        if(total)ImGui::Text("%zu / %zu %s",std::min(done,total),total,phase==HelpyIconPack::Phase::Verify?"source files":"icons");else ImGui::Text("%zu files inspected",done);
        if(Advanced&&byteTotal)ImGui::Text("%.1f / %.1f MB",double(bytes)/(1024*1024),double(byteTotal)/(1024*1024));
        ImGui::BeginDisabled(Progress.Cancel.load());if(ImGui::SmallButton("Cancel icon operation"))Progress.Cancel=true;ImGui::EndDisabled();
    }
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(!ActiveFile.empty())ImGui::TextWrapped("%s: %s",LastOperationOK?"File":"Destination",ActiveFile.string().c_str());
    if(LastOperationOK&&!LastFile.empty()&&ImGui::TextLink("Show file in Explorer")){
        const auto arguments=L"/select,\""+LastFile.wstring()+L"\"";ShellExecuteW(nullptr,L"open",L"explorer.exe",arguments.c_str(),nullptr,SW_SHOWNORMAL);
    }
}
inline void RenderWindowContents(bool catalogueBusy=false){
    ImGui::TextUnformatted("Icon cache");ImGui::SameLine();
    if(ImGui::SmallButton(Detached?"Dock back":"Detach window"))Detached=!Detached;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheDetach=HelpyPreviewHooks::LastRect();
#endif
    ImGui::Separator();ImGui::BeginChild("##cache-body",{0,0});
    Render(catalogueBusy);ImGui::EndChild();
}
inline void RenderDockedWindow(bool catalogueBusy=false){
    if(!Open||Detached||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::IconCache))return;
    const auto& theme=HelpyImGuiTheme::Current;
    HelpyImGuiTheme::Scope skin(theme);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,theme.navigation);ImGui::PushStyleColor(ImGuiCol_TitleBgActive,theme.navigationActive);
    ImGui::SetNextWindowSize({std::min(theme.X(610),ImGui::GetIO().DisplaySize.x-theme.X(32)),std::min(theme.Y(480),ImGui::GetIO().DisplaySize.y-theme.Y(32))},ImGuiCond_Appearing);
    HelpyWindowChrome::Prepare(Chrome);
    if(ImGui::Begin("Icon cache##rsmk-cache",&Open,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse)){HelpyBackgrounds::DrawWindow();HelpyWindowChrome::Render(Chrome,HelpyTaskbar::Window::IconCache);RenderWindowContents(catalogueBusy);}ImGui::End();ImGui::PopStyleColor(2);
}
}
