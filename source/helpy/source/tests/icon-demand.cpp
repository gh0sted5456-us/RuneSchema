#include "Generator/ItemIconCache.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/RuntimeConnectionPanel.h"
#include "Generator/DragFeedback.h"
#include "Generator/HelpyLoadingIndicator.h"
#include <cassert>
#include <deque>
#include <fstream>
#include <thread>

int main() {
    using namespace PS;
    {namespace quick=RuneSchemaSettingsPanel;unsigned writes=0;
        const nlohmann::json settings={{"advancedLogging",false},{"autoUpdateMappings",true},{"persistence",{{"allowCorruptCharacterEntry",false}}}};
        quick::Accept({{"Document",settings},{"Editable",{{"/advancedLogging","boolean"},{"/autoUpdateMappings","boolean"},{"/persistence/allowCorruptCharacterEntry","boolean"}}},{"ExpectedContents","original fixture"}});
        quick::Remote=[&](const nlohmann::json& request,nlohmann::json& reply){++writes;assert(request.at("Action")=="SettingsSave"&&request.at("ExpectedContents")=="original fixture");
            assert(request.at("Patch").size()==1&&request.at("Patch").at("/advancedLogging")==true);auto saved=quick::Snapshot;saved["Document"]["advancedLogging"]=true;saved["Ok"]=true;reply=saved;return true;};
        quick::SaveQuick("/advancedLogging",true);assert(writes==1&&quick::Snapshot["Document"]["advancedLogging"]==true&&quick::Snapshot["Document"]["persistence"]["allowCorruptCharacterEntry"]==false);
        quick::Draft["autoUpdateMappings"]=false;quick::SaveQuick("/advancedLogging",false);assert(writes==1); // Never save an unrelated pending Settings draft.
        quick::Loaded=false;quick::Snapshot=quick::Draft=nlohmann::json::object();quick::Status.clear();quick::Remote={};}
    namespace icons=ItemIconCache;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    {auto theme=HelpyImGuiTheme::Current;theme.workspaces={true,false,true,true,true};theme.itemShowPlaceholders=true;
        const auto document=HelpyImGuiTheme::DocumentFrom(theme,265);
        assert(document["workspaces"]["authoring"]==true&&document["workspaces"]["tools"]==false&&document["workspaces"]["liveLog"]==true&&document["workspaces"]["saveEditor"]==true&&document["workspaces"]["serverTools"]==true&&document["views"]["itemShowPlaceholders"]==true);}
    {auto theme=HelpyImGuiTheme::Current;theme.accent={.15f,.5f,.95f,1};theme.accentHover={.3f,.7f,1,1};
        HelpyImGuiTheme::Scope skin(theme);const auto& colors=ImGui::GetStyle().Colors;
        assert(colors[ImGuiCol_PlotHistogram].z==theme.accent.z&&colors[ImGuiCol_TextLink].y==theme.accentHover.y&&colors[ImGuiCol_ResizeGripActive].y==theme.accentHover.y);}
    HelpyImGuiTheme::PictureBackingActive=true;
    {HelpyImGuiTheme::Scope skin(HelpyImGuiTheme::Current);assert(ImGui::GetStyle().Colors[ImGuiCol_ChildBg].w==0&&ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w==HelpyImGuiTheme::Current.panelAlt.w);}
    HelpyImGuiTheme::PictureBackingActive=false;
    {HelpyImGuiTheme::Scope skin(HelpyImGuiTheme::Current);assert(ImGui::GetStyle().Colors[ImGuiCol_ChildBg].w==HelpyImGuiTheme::Current.panel.w);}
    io.DisplaySize={640,480};io.DeltaTime=1.0f/60.0f;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    SpawnToolRequests::Generation=41;
    {namespace connection=RuntimeConnectionPanel;const auto now=std::chrono::steady_clock::now();
        connection::Generation=41;connection::Received=now;
        connection::Snapshot={{"Available",true},{"Fresh",true},{"Role","listen-server"},{"AgeMs",0},{"KnownPlayers",2}};
        assert(connection::Read(now).Host&&connection::Read(now).Players==2);
        assert(!connection::Read(now+std::chrono::seconds(13)).Host);
        SpawnToolRequests::Generation=42;assert(!connection::Read(now).Host);SpawnToolRequests::Generation=41;
        connection::Snapshot["Role"]="client";assert(!connection::Read(now).Host&&connection::Read(now).Session=="Multiplayer client");
        connection::Snapshot=nlohmann::json::object();connection::Generation=0;}
    auto& state=icons::detail::State();state.Generation=41;
    unsigned submissions=0;std::deque<nlohmann::json> replies;
    icons::ConfigureRemote([&](const std::string&){++submissions;return true;},[&](nlohmann::json& result){if(replies.empty())return false;result=std::move(replies.front());replies.pop_front();return true;});
    const auto begin=[] {ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({640,480});ImGui::Begin("test");};
    const auto end=[] {ImGui::End();ImGui::EndFrame();assert(GImGui->ErrorCountCurrentFrame==0);};
    begin();ImGui::Dummy({50,40});const auto sourceRect=ImGui::GetItemRectMin();const auto vertices=ImGui::GetForegroundDrawList()->VtxBuffer.Size;
    ImGui::GetIO().MousePos={639,479};DragFeedback::Held("Item","A very long held item name that wraps without changing its source layout");
    assert(ImGui::GetForegroundDrawList()->VtxBuffer.Size>vertices&&ImGui::GetMouseCursor()==ImGuiMouseCursor_Hand);
    assert(ImGui::GetItemRectMin().x==sourceRect.x&&ImGui::GetItemRectMin().y==sourceRect.y&&state.Queued.empty()&&submissions==0);end();
    const auto until=[&](auto ready){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!ready()){assert(std::chrono::steady_clock::now()<deadline);const auto before=state.Textures.size();icons::Update();assert(state.Textures.size()<=before+1);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
    const auto cached=[&](const std::string& icon){icons::Queue(icon,true);until([&]{return state.Textures.contains(icon);});};
    // Reproduce a tooltip opened from a tiny Save Editor table cell for many
    // frames. Width must be viewport-bounded and invariant, not self-shrinking.
    for(const ImVec2 viewport:std::array{ImVec2{640,480},ImVec2{760,560},ImVec2{1280,800}}){
        io.DisplaySize=viewport;float previous=0;
        for(int frame=0;frame<160;++frame){begin();ImGui::BeginChild("tiny-cell",{18,90});
            HelpyImGuiTheme::BeginDetailsTooltip();
            const auto textLeft=ImGui::GetCursorScreenPos().x,textRight=textLeft+ImGui::GetContentRegionAvail().x;
            auto* draw=ImGui::GetWindowDrawList();const auto firstVertex=draw->VtxBuffer.Size;
            HelpyImGuiTheme::DetailsTextUnformatted(std::string(4096,'W').c_str());
            for(int i=firstVertex;i<draw->VtxBuffer.Size;++i)assert(draw->VtxBuffer[i].pos.x>=textLeft-2&&draw->VtxBuffer[i].pos.x<=textRight+2);
            const auto size=ImGui::GetWindowSize();if(frame>3){assert(size.x>=300&&size.x<=viewport.x&&size.y<=viewport.y);if(previous)assert(std::abs(size.x-previous)<1);previous=size.x;}
            HelpyImGuiTheme::EndDetailsTooltip();ImGui::EndChild();end();
        }
    }io.DisplaySize={640,480};
    for(const ImVec2 anchor:std::array{ImVec2{300,120},ImVec2{0,420},ImVec2{580,420}}){
        for(int frame=0;frame<6;++frame){begin();ImGui::SetCursorScreenPos(anchor);ImGui::Dummy({60,24});
            const auto minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
            HelpyImGuiTheme::BeginDetailsTooltip();
            const float textCenter=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x*.5f,startX=ImGui::GetCursorPosX();
            HelpyImGuiTheme::DetailsTextUnformatted("Common RuneSchema switches without leaving Home.\nHome.");
            const auto textMin=ImGui::GetItemRectMin(),textMax=ImGui::GetItemRectMax();
            assert(std::abs((textMin.x+textMax.x)*.5f-textCenter)<1&&ImGui::GetCursorPosX()==startX);
            HelpyImGuiTheme::DetailsTextDisabled("%s","Centered secondary details");
            HelpyImGuiTheme::DetailsTextColored({1,1,0,1},"%s","Centered warning details");
            const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
            if(frame>2){assert(position.x>=0&&position.x+size.x<=640&&position.y>=0&&position.y+size.y<=480);
                if(anchor.x==300)assert(std::abs(position.x+size.x*.5f-(minimum.x+maximum.x)*.5f)<1);
                if(anchor.y==420)assert(position.y+size.y<=minimum.y);}
            HelpyImGuiTheme::EndDetailsTooltip();end();
        }
    }
    // Draw-list controls must anchor to their own bounds, never the last widget.
    for(const bool covered:{false,true})for(int frame=0;frame<8;++frame){
        io.MousePos={420,180};begin();ImGui::SetCursorScreenPos({16,50});ImGui::Dummy({40,20});
        const auto previous=ImGui::GetItemRectMin();
        HelpyLoadingIndicator::Draw({420,180},12,HelpyImGuiTheme::Current,"Checking cache and mounted sources","Stage: checking-cache\nCatalogue records checked: 0 (unknown total)");
        assert(ImGui::GetItemRectMin().x==previous.x&&ImGui::GetItemRectMin().y==previous.y);
        ImGuiWindow* tooltip=nullptr;for(auto* window:GImGui->Windows)if(window->Active&&(window->Flags&ImGuiWindowFlags_Tooltip))tooltip=window;
        if(frame>3){if(covered)assert(!tooltip);else{assert(tooltip);assert(std::abs(tooltip->Pos.x+tooltip->Size.x*.5f-420)<1);assert(tooltip->Pos.y>192&&tooltip->Pos.x>=0&&tooltip->Pos.x+tooltip->Size.x<=640);}}
        ImGui::End();
        if(covered){ImGui::SetNextWindowPos({350,130});ImGui::SetNextWindowSize({140,100});ImGui::Begin("hover blocker",nullptr,ImGuiWindowFlags_NoSavedSettings);ImGui::TextUnformatted("Covers the loading ring");ImGui::End();}
        ImGui::EndFrame();assert(GImGui->ErrorCountCurrentFrame==0);
    }
    io.MousePos={-100,-100};
    begin();
    assert(!icons::Draw("clipped",{900,900},{950,950},true));
    assert(state.Queued.empty());
    icons::Queue("waiting",true);icons::Update();assert(submissions==0); // source identity unavailable
    end();
    HelpySourceCache::Current.Key="icon-demand-fixture";
    begin();icons::Update();assert(submissions==0&&state.Queued.empty()); // no longer visible
    for(int i=0;i<80;++i){const auto icon="visible-"+std::to_string(i);state.DiskChecked.insert(icon);icons::Queue(icon,true);}
    assert(state.Queued.size()==32);icons::Update();assert(submissions==1&&state.Pending.size()==1);
    state.NextSubmit={};icons::Update();assert(submissions==1); // single in-flight capture
    const auto active=*state.Pending.begin();end();
    begin();
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();state.NextSubmit={};icons::Update();
    assert(submissions==1&&state.Pending.empty()&&state.Queued.empty()); // retries off-screen are discarded
    // A visible icon may retry, but never loop indefinitely while idle.
    state.DiskChecked.insert(active);icons::Queue(active,true);state.NextSubmit={};icons::Update();assert(submissions==2);
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();state.NextSubmit={};icons::Update();assert(submissions==3);
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();assert(state.Failed.contains(active)&&state.RetryAt.empty());
    state.NextSubmit={};icons::Queue(active,true);icons::Update();assert(submissions==3);
    icons::RetryFailed();state.DiskChecked.insert(active);icons::Queue(active,true);icons::Update();assert(submissions==4);
    end();
    // A source-scoped disk hit must install pixels without asking the engine
    // to render again. A cached file cannot stand in for source registration.
    HelpyEnvironment::Configure({{"DedicatedServer",true},{"NativeIconCaptureAvailable",true}});
    assert(HelpyEnvironment::Server()&&!HelpyEnvironment::NativeIconCaptureAvailable);
    icons::InvalidateSource();HelpySourceCache::Current={};HelpySourceCache::Current.Authoritative=true;
    HelpySourceCache::Current.Key="0123456789abcdef";HelpySourceCache::Current.SourceKeys={{"game","abcdef0123456789"}};
    const std::string cachedIcon="/Game/Art/Test/T_Cached.T_Cached";
    const auto file=icons::detail::DiskFile(cachedIcon);std::filesystem::create_directories(file.parent_path());
    const uint32_t iconWidth=64,iconHeight=64,iconBytes=iconWidth*iconHeight*4;
    std::vector<unsigned char> iconPixels(iconBytes,255);
    {std::ofstream output(file,std::ios::binary);output.write("RSI3",4);output.write(reinterpret_cast<const char*>(&iconWidth),4);
        output.write(reinterpret_cast<const char*>(&iconHeight),4);output.write(reinterpret_cast<const char*>(&iconBytes),4);
        output.write(reinterpret_cast<const char*>(iconPixels.data()),iconPixels.size());}
    const auto requestsBeforeDiskHit=submissions;
    begin();icons::Queue(cachedIcon,true);icons::Update();
    until([&]{return state.Textures.contains(cachedIcon);});
    assert(submissions==requestsBeforeDiskHit&&state.Textures.contains(cachedIcon));
    auto* retained=state.Textures.at(cachedIcon);
    // A pack update affecting another path must not flash resident icons.
    icons::InvalidateIcons({"/Game/Art/Test/T_Other.T_Other"});
    assert(state.Textures.at(cachedIcon)==retained);
    HelpySourceCache::Current.SourceKeys["newsource"]="1234567890abcdef";
    icons::InvalidateSource(true);
    assert(state.Textures.at(cachedIcon)==retained); // no flash/re-render for an unrelated source addition
    HelpySourceCache::Current.SourceKeys["game"]="fedcba0987654321";
    icons::InvalidateSource(true);assert(!state.Textures.contains(cachedIcon));
    HelpySourceCache::Current.SourceKeys["game"]="abcdef0123456789";
    cached(cachedIcon);assert(state.Textures.contains(cachedIcon));
    icons::InvalidateIcons({cachedIcon});assert(!state.Textures.contains(cachedIcon));
    cached(cachedIcon);assert(state.Textures.contains(cachedIcon));
    assert(submissions==requestsBeforeDiskHit); // refreshed pixels still bypass engine captures
    // First-open client page: cached thumbnails load off-thread with bounded uploads,
    // without entering the single-flight engine capture queue, even though
    // native captures are available and the normal queue has a 32-entry cap.
    HelpyEnvironment::Configure({{"DedicatedServer",false},{"NativeIconCaptureAvailable",true}});
    assert(HelpyEnvironment::NativeIconCaptureAvailable);
    HelpySourceCache::Current.Provisional=true;
    for(int i=0;i<40;++i){const auto icon="/Game/Art/Test/T_FirstOpen_"+std::to_string(i)+".T_FirstOpen_"+std::to_string(i);
        const auto target=icons::detail::DiskFile(icon);std::filesystem::copy_file(file,target,std::filesystem::copy_options::overwrite_existing);
        cached(icon);assert(state.Textures.contains(icon));}
    icons::Update();assert(submissions==requestsBeforeDiskHit&&state.Pending.empty()&&state.Queued.empty());
    const std::string provisionalMissing="/Game/Art/Test/T_ProvisionalMiss.T_ProvisionalMiss";
    icons::Queue(provisionalMissing,true);state.NextSubmit={};icons::Update();
    until([&]{return state.DiskChecked.contains(provisionalMissing);});
    assert(submissions==requestsBeforeDiskHit&&state.Pending.empty()&&state.Queued.contains(provisionalMissing));
    // Leave the guarded request queued only until the next verified demand.
    state.High.clear();state.Normal.clear();state.Queued.clear();HelpySourceCache::Current.Provisional=false;
    // Imported pixels can arrive after a visible miss was queued. Dispatch
    // must recheck AppData instead of asking the game to capture them again.
    const std::string arrivedIcon="/Game/Art/Test/T_Arrived.T_Arrived";
    icons::Queue(arrivedIcon,true);assert(state.Queued.contains(arrivedIcon));
    std::filesystem::copy_file(file,icons::detail::DiskFile(arrivedIcon),std::filesystem::copy_options::overwrite_existing);
    state.NextSubmit={};icons::Update();until([&]{return state.Textures.contains(arrivedIcon);});assert(submissions==requestsBeforeDiskHit&&state.Pending.empty());
    // A newly discovered unrelated source must not cancel and resubmit work
    // already accepted for an unchanged source, or reset its retry history.
    const std::string pendingIcon="/Game/Art/Test/T_Pending.T_Pending";
    icons::Queue(pendingIcon,true);state.NextSubmit={};icons::Update();
    until([&]{return state.Pending.contains(pendingIcon);});
    assert(state.Pending.contains(pendingIcon)&&submissions==requestsBeforeDiskHit+1);
    const std::string queuedIcon="/Game/Art/Test/T_Queued.T_Queued";
    icons::Queue(queuedIcon,true);state.Attempts[queuedIcon]=1;
    HelpySourceCache::Current.SourceKeys["unrelated"]="aaaabbbbccccdddd";
    icons::InvalidateSource(true);assert(state.Pending.contains(pendingIcon)&&state.Queued.contains(queuedIcon)&&state.Attempts.at(queuedIcon)==1);
    state.NextSubmit={};icons::Update();assert(submissions==requestsBeforeDiskHit+1);
    HelpySourceCache::Current.SourceKeys["game"]="1111222233334444";
    icons::InvalidateSource(true);assert(!state.Pending.contains(pendingIcon)&&!state.Queued.contains(queuedIcon)&&!state.Attempts.contains(queuedIcon));
    HelpySourceCache::Current.SourceKeys["game"]="abcdef0123456789";
    const auto requestsAfterSourceChecks=submissions;
    HelpyEnvironment::Configure({{"DedicatedServer",true}});
    assert(icons::detail::DiskFile("/NotRegistered/Art/T_Stale.T_Stale").empty());
    assert(!icons::detail::Install("/Game/Art/Test/T_Wrong.T_Wrong",file));
    state.NextSubmit={};icons::Queue("/NotRegistered/Art/T_Stale.T_Stale",true);icons::Update();
    assert(submissions==requestsAfterSourceChecks&&state.Pending.empty()&&state.Failed.empty());
    icons::Queue("/Game/Art/Test/T_NotCached.T_NotCached",true);icons::Update();until([&]{return !state.DiskWorker.valid()&&state.DiskQueue.empty();});
    assert(submissions==requestsAfterSourceChecks&&state.Pending.empty()&&state.Queued.empty()&&state.Failed.empty());
    end();
    unsigned accountingResets=0;icons::RemoteResetAccounting=[&]{++accountingResets;};
    assert(!icons::RemoveDiskFolder(HostServices::CacheDirectory()));
    assert(!icons::RemoveDiskFolder(HostServices::CacheDirectory()/"unrelated"));
    assert(std::filesystem::exists(file));
    const auto diskUsage=icons::ScanDiskStats(icons::ActiveDiskFolder());assert(diskUsage.Files>=1&&diskUsage.Bytes>=std::filesystem::file_size(file));
    assert(icons::CleanDiskCache()&&accountingResets==1&&!std::filesystem::exists(file));
    icons::RemoteResetAccounting={};
    for(auto* texture:state.RetiredTextures)if(texture)texture->SetStatus(ImTextureStatus_Destroyed);
    icons::detail::ReapRetired(state);icons::ClearRemote();ImGui::DestroyContext();
    HelpyEnvironment::Configure(nlohmann::json::object());
}
