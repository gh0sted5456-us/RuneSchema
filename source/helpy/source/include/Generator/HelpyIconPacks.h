#pragma once
#include "Core/ProcessExitFuture.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyIconPack.h"
#include "Core/HelpyCacheOwnership.h"
#include "Core/HelpyModIconPacks.h"
#include "Core/HelpyTaskbar.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/HelpyBackgrounds.h"
#include "Generator/SourceBanners.h"
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
inline PS::ProcessExitFuture<HelpyIconPack::Result> Worker;
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
inline uint64_t ReferencesRevision=0;
inline std::string PackagedKey;
inline std::chrono::steady_clock::time_point NextPackaged{};
inline std::function<bool(const nlohmann::json&,nlohmann::json&)> RemoteBuild;
inline nlohmann::json BuildState=nlohmann::json::object();
inline std::string SelectedSource;
inline std::filesystem::path PendingBuildExport;
inline std::filesystem::path PendingDownload;
inline std::string PendingBuildSource;
inline std::chrono::steady_clock::time_point NextBuildRead{};
inline bool Building(){return BuildState.value("Active",false);}
inline void PollBuild(){
    if(!RemoteBuild||std::chrono::steady_clock::now()<NextBuildRead)return;
    NextBuildRead=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    nlohmann::json reply;if(RemoteBuild({{"Action","IconCacheBuildRead"}},reply)&&reply.is_object())BuildState=std::move(reply);
}
inline bool Busy(){return Worker.valid();}
inline bool Start(const std::filesystem::path& file,bool importing,const nlohmann::json& coverage=nlohmann::json::object(),bool nativeArtworkFallback=false){
    if(Busy()||Building()||SourceBanners::Busy()){Status="Wait for the active cache/artwork operation to finish.";return false;}
    try{
        const auto extension=HelpyCacheSources::Lower(file.extension().string());
        if(extension!=".rsmk-icons"&&extension!=".rsmk-cache")throw std::runtime_error("[RS-CACHE-002] Choose a .rsmk-cache bundle or legacy .rsmk-icons pack.");
        const auto limit=HelpyRuntimePreferences::IconCacheLimitMB.load();if(limit<=0)throw std::runtime_error("Enable Keep icons between launches before using shared packs.");
        const auto snapshot=HelpySourceCache::Current;const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);
        Progress.Cancel=false;Progress.Begin(HelpyIconPack::Phase::Discover);Importing=importing;Packaged=false;CompletionNotify=false;ActiveFile=file;LastOperationOK=false;
        const auto source=SelectedSource;
        const auto policy=HelpyIconPack::ImportPolicy::Presentation;
        const auto journal=HostServices::CacheDirectory()/"journal";
        Worker=std::async(std::launch::async,[file,root,cache,snapshot,limit,importing,source,policy,coverage,journal]{
            const auto& verified=snapshot;
            return importing?HelpyIconPack::Import(file,root,cache,verified,uint64_t(limit)*1024*1024,Progress,policy,source):HelpyIconPack::Export(file,root,cache,verified,Progress,source,coverage,CacheBundleStore::Collect(cache,root,journal,verified,true),true);
        });Status=importing?"Processing cache and assigning item/artwork relationships...":"Exporting the current cache snapshot...";return true;
    }catch(const std::exception& error){Status=error.what();return false;}
}
inline void Poll(){PollBuild();
    if(PendingDownload.empty())PendingDownload=HelpyDownloads::TakeBaseCache();
    if(!PendingDownload.empty()&&!Busy()&&!Building()&&!SourceBanners::Busy()&&HelpyRuntimePreferences::IconCacheLimitMB.load()>0){
        {const auto savedSource=SelectedSource;SelectedSource.clear();const bool started=Start(PendingDownload,true,nlohmann::json::object(),true);SelectedSource=savedSource;if(started)PendingDownload.clear();}
    }
    if(!PendingBuildExport.empty()&&!Building()){
        const auto file=PendingBuildExport;
        const auto stage=BuildState.value("Stage",std::string{});
        if(stage=="Complete"||stage=="Completed with failures"){
            nlohmann::json coverage;
            if(RemoteBuild&&RemoteBuild({{"Action","IconCacheBuildRead"},{"IncludeManifest",true}},coverage)
                &&coverage.value("Sequence",uint64_t{})==BuildState.value("Sequence",uint64_t{})){
                const auto selected=SelectedSource;SelectedSource=PendingBuildSource;const bool started=Start(file,false,coverage);SelectedSource=selected;if(started){PendingBuildExport.clear();PendingBuildSource.clear();}
            }else{PendingBuildExport.clear();Status="Build coverage could not be retrieved. Retry Export; cached icons are retained.";}
        }
        else {PendingBuildExport.clear();Status="Build/export stopped; completed thumbnails remain cached. No incomplete export was started.";}
    }
    if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try{const auto result=Worker.get();Status=result.Message;CompletionNotify=!Packaged||HelpyModIconPacks::NotifyAutomatic(result);if(!Packaged){LastFile=ActiveFile;LastOperationOK=true;}if(Importing&&result.Imported){ItemIconCache::InvalidateIcons(result.ChangedIcons);if(ItemIconCache::RemoteResetAccounting)ItemIconCache::RemoteResetAccounting();}
        if(Importing&&(result.RecordsImported||result.MapImported)){if(RemoteBuild){nlohmann::json ignored;RemoteBuild({{"Action","CacheBundleImported"}},ignored);}++ReferencesRevision;}
        if(Importing&&!Packaged){if(result.Imported||result.Unchanged||result.RecordsImported||result.MapImported)HelpyDownloads::ReleaseImportedPack(ActiveFile);HelpyDownloads::CacheApplied(Status);}}
    catch(const HelpyCacheOwnership::Unavailable& error){
        Status=error.what();CompletionNotify=!Packaged;
        if(Packaged){PackagedKey.clear();NextPackaged=std::chrono::steady_clock::now()+std::chrono::seconds(2);}
    }
    catch(const std::exception& error){Status=error.what();CompletionNotify=true;} // Last-good images and records remain usable on failed validation.
}
inline void PollPackaged(){
    if(Busy()||Building()||!PendingBuildExport.empty()||std::chrono::steady_clock::now()<NextPackaged)return;
    const auto limit=HelpyRuntimePreferences::IconCacheLimitMB.load();if(limit<=0)return;
    const auto snapshot=HelpySourceCache::Current;NextPackaged=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false),shared=RuneSchemaData::Directory()/"core"/"cache";
    Progress.Cancel=false;Progress.Begin(HelpyIconPack::Phase::Discover);Importing=true;Packaged=true;CompletionNotify=false;
    const auto ownership=RemoteBuild;
    Worker=std::async(std::launch::async,[root,cache,shared,snapshot,limit,ownership]{
        return HelpyModIconPacks::ImportAll(root,cache,snapshot,uint64_t(limit)*1024*1024,Progress,shared,true);
    });
}
inline void Shutdown() noexcept {PendingBuildExport.clear();Progress.Cancel=true;if(RemoteBuild)try{nlohmann::json ignored;RemoteBuild({{"Action","IconCacheBuildCancel"}},ignored);}catch(...){}if(Worker.valid())try{(void)Worker.get();}catch(...){};}
inline void Choose(bool importing,bool buildFirst=false){
    wchar_t file[32768]{};if(!importing){const auto name=SelectedSource.empty()?std::wstring(L"Helpy content cache.rsmk-cache"):std::filesystem::path(SelectedSource.substr(SelectedSource.find_last_of('/')+1)+".rsmk-cache").wstring();wcscpy_s(file,name.c_str());}
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=importing?L"Helpy cache or legacy icons\0*.rsmk-cache;*.rsmk-icons\0\0":L"Helpy cache bundle (*.rsmk-cache)\0*.rsmk-cache\0\0";dialog.lpstrDefExt=L"rsmk-cache";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(importing?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog)){
        if(buildFirst){
            if(Busy()||Building()||!RemoteBuild){Status="Wait for the active cache operation to finish.";return;}
            nlohmann::json reply;
            if(RemoteBuild({{"Action","IconCacheBuildStart"},{"Source",SelectedSource}},reply)&&reply.value("Accepted",false)){
                PendingBuildExport=file;PendingBuildSource=SelectedSource;BuildState={{"Active",true},{"Stage","Queued"}};NextBuildRead={};
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
    if(ImGui::BeginCombo("##Cache source",SelectedSource.empty()?"All sources":SelectedSource.c_str())){
        if(ImGui::Selectable("All sources",SelectedSource.empty()))SelectedSource.clear();for(const auto& source:choices)if(ImGui::Selectable(source.c_str(),SelectedSource==source))SelectedSource=source;ImGui::EndCombo();}
    ImGui::EndDisabled();
    if(!HelpySourceCache::Ready())ImGui::TextDisabled("Cached content is available while the game checks mounted sources.");
    ImGui::SeparatorText("1. Add cached content");
    ImGui::BeginDisabled(Busy()||Building()||HelpyDownloads::Busy()||HelpyRuntimePreferences::IconCacheLimitMB.load()<=0);
    if(ImGui::Button("Download base-game cache"))HelpyDownloads::StartBaseCache(HelpySourceCache::Current.SourceKeys,HelpySourceCache::Current.NativePlugins,HelpyEnvironment::Label(),HelpyEnvironment::GameVersion);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))PS::HelpyImGuiTheme::Tooltip("Downloads items and artwork into this platform's AppData cache. Cached content appears immediately; the game checks current availability separately and updates it passively.");
    ImGui::BeginDisabled(Busy()||Building());
    if(ImGui::Button("Import cache..."))Choose(true);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheImport=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    ImGui::SeparatorText("2. Fill missing content (optional)");
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy||!RemoteBuild||!BuildState.value("Available",false)||!HelpySourceCache::Ready()||HelpyEnvironment::Server());
    if(ImGui::Button("Build / update")){
        nlohmann::json result;if(RemoteBuild({{"Action","IconCacheBuildStart"},{"Source",SelectedSource}},result)&&result.value("Accepted",false)){BuildState={{"Active",true},{"Stage","Queued"}};NextBuildRead={};}
        else Status="Cache builder is unavailable or already running.";
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheBuild=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    ImGui::TextWrapped("Build / update asks the game for missing items and pictures. Export can save the cache you already have.");
    ImGui::SeparatorText("3. Save current cache");
    ImGui::BeginDisabled(Busy()||Building()||SourceBanners::Busy());
    if(ImGui::Button("Export current cache..."))Choose(false);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheExport=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    ImGui::BeginDisabled(Busy()||Building()||SourceBanners::Busy()||!SelectedSource.starts_with("mods/"));
    if(ImGui::Button("Share selected mod cache in core")){
        try{Start(HelpyModIconPacks::SharedFile(RuneSchemaData::Directory()/"core"/"cache",SelectedSource),false);}
        catch(const std::exception& error){Status=error.what();}
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))PS::HelpyImGuiTheme::Tooltip("Saves this mod's current bundle under core/cache/mods. Other runtimes import it at their next source scan only when live ownership and content hashes match. Sharing does not load mods or grant permissions.");
    ImGui::TextWrapped("Exports contain cached paths, known IDs, pictures and the map when available. Coverage can be partial; live availability is checked separately when used.");
    if(HelpyDownloads::Busy()){const auto total=HelpyDownloads::BytesTotal(),done=HelpyDownloads::BytesCompleted();ImGui::ProgressBar(total?std::min(1.0f,float(done)/float(total)):-float(ImGui::GetTime()),{-1,0},"Downloading cache / assets");}
    if(!Busy()&&!HelpyDownloads::Current().Status.empty())ImGui::TextWrapped("%s",HelpyDownloads::Current().Status.c_str());
    if(ImGui::TextLink("Open Helpy cache folder")){
        const auto folder=HostServices::CacheDirectory();std::error_code error;std::filesystem::create_directories(folder,error);
        if(!error)ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    }
    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s",HostServices::CacheDirectory().string().c_str());
    ImGui::Checkbox("Advanced",&Advanced);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CacheAdvanced=HelpyPreviewHooks::LastRect();
#endif
    if(Advanced&&ImGui::CollapsingHeader("PAK / container discovery")){
        ImGui::TextWrapped("Directory indexes associate packages with their supplying containers. Indexed does not certify a successful mount or live item availability.");
        if(ImGui::BeginChild("##pak-discovery",ImVec2(0,180),ImGuiChildFlags_Borders)){
            for(const auto& [file,entry]:HelpySourceCache::Current.Containers.items()){
                const auto owner=entry.value("Owner",std::string{});
                if(!SelectedSource.empty()&&SelectedSource!=owner)continue;
                ImGui::TextWrapped("%s · %s · %zu packages",owner.c_str(),entry.value("Status",std::string{}).c_str(),entry.value("Packages",size_t{}));
                ImGui::TextWrapped("%s",file.c_str());
                const auto reason=entry.value("Reason",std::string{});if(!reason.empty())ImGui::TextWrapped("%s",reason.c_str());
                ImGui::Separator();
            }
        }ImGui::EndChild();
    }
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
    SourceBanners::ExportArt(SelectedSource,Busy()||Building());
    ImGui::TextWrapped("Writable cache: %s",HostServices::CacheDirectory().string().c_str());
    ImGui::TextWrapped("Mod authors can ship cache/*.rsmk-cache (or legacy *.rsmk-icons) inside their RuneSchema mod. Typed references are restricted to that active owner; supplied bundles are never edited.");
    ImGui::BeginDisabled(Busy()||Building()||catalogueBusy);
    if(HelpyEnvironment::Server()){
        ImGui::Checkbox("Use client packs as display-only thumbnails",&ServerDisplayOnly);
        ImGui::TextWrapped("Matches exact icon paths in active server sources, even when the server cook omits textures. These pictures may differ from the server version; they are not item definitions and are excluded from verified cache exports.");
    }
    if(ImGui::Button("Build / update, then export..."))Choose(false,true);ImGui::EndDisabled();
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
        const char* label=phase==HelpyIconPack::Phase::Discover?"Finding cached icons":phase==HelpyIconPack::Phase::Verify?"Checking content sources":phase==HelpyIconPack::Phase::Validate?"Validating icons":phase==HelpyIconPack::Phase::Write?"Assigning relationships and saving cache":phase==HelpyIconPack::Phase::Commit?"Saving file":"Complete";
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
