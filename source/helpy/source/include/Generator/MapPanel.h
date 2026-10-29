#pragma once
#include "Core/ProcessExitFuture.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/MapProjection.h"
#include "Core/MapViewport.h"
#include "Core/MapSpawnRequest.h"
#include "Core/MapParty.h"
#include "Core/PartyStatWire.h"
#include "Core/MapTeleportRisk.h"
#include "Core/MapOceanMask.h"
#include "Core/SpawnCatalogSearch.h"
#include "Core/BuildingMenu.h"
#include "Runtime/HelpyMapCache.h"
#include "Core/MapPresentationCache.h"
#include "Generator/HelpyRuntimePreferences.h"
#include <future>
#include "Generator/HelpyLoadingIndicator.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/PartyMemberView.h"
#include "Generator/PlayerInventoryPanel.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/HelpyWindowChrome.h"
#include <fstream>
#include <chrono>
namespace PS::MapPanel {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct State {
    Json Snapshot=Json::object(),Definition=Json::object(),Building=Json::object(),Ground=Json::object(),Buildings=Json::array();
    MapProjection::Projection Projection;
    MapProjection::Viewport View;
    MapProjection::Point Center{0.5,0.5},Point{},ProbePoint{};
    float Zoom=1,Radius=300,Yaw=0;int Kind=2,Count=1,Operation=0,SpawnType=0;
    bool Permanent=false,Popup=false,ShowPlayers=true,HasPoint=false,Dragged=false,ProbeWanted=false,BuildingsWanted=false,BuildingsMore=false;
    bool NearMarker=false,TravelAction=false,CancelSent=false;
    bool BuildingBrowserOpen=false,BuildingAction=false,BuildingPick=false;
    Json Prefab=Json::object();std::string PrefabName;
    int BuildingCategory=-1;
    Json BuildingCategories=Json::array(),PlacementResult=Json::object();
    bool FitRequested=false,HoverActive=false,GroundReadSelected=false;
    Json HoverGround=Json::object();MapProjection::Point GroundReadPoint{},HoverPoint{};
    uint64_t GroundReadPending=0;Clock::time_point HoverStableAt{},NextGroundRead{},GroundReadAt{},HoverFreshUntil{};
    struct RiskSample {MapProjection::Point Point;std::string Player,Guid;Json Observation;};
    std::vector<RiskSample> RiskSamples;
    MapOceanMask::Mask Ocean;
    Json ResourceDetails=Json::object();bool ResourceDetailsWanted=false;
    bool LiveSearch=false;std::string TypedSearch;
    bool ShowLodestones=true,ShowPlaced=true,ShowResources=false,ShowAI=false,ShowChests=false;
    size_t BuildingsOffset=0,BuildingsNext=0,BuildingsSentOffset=0;
    std::string BuildingsSentQuery,SelectionName;
    Clock::time_point SearchDue{},BuildingSearchDue{};bool SearchWanted=false;
    char BuildingSearch[257]{};
    char Search[257]{};std::string Error,Status,ImageFile;
    char MarkerSearch[129]{};
    const MapFilters::Dataset* FilteredData=nullptr;uint64_t FilterRevision=0;
    std::string FilteredQuery;std::vector<size_t> FilteredMarkers;
    bool NativeFiltersRestored=false;
    ImVec2 MapPopupAnchor{},PartyContextAnchor{};
    std::string PartySelection,PartyTarget;bool PartyRecall=false,ShowParty=true,PartyDetached=false;
    HelpyWindowChrome::State PartyChrome;
    std::string PartyMoving,PartyDestination,PartyMovingGuid,PartyDestinationGuid,PartyPicking;
    MapProjection::Point PartyMapPoint{};bool PartyToMap=false,PartyConfirmWanted=false;
    uint64_t Generation=0,Pending=0,SpawnPending=0,ProbePending=0,BuildingsPending=0,Sequence=0x7300000000000000ull;
    Clock::time_point Next{},Requested{},ActionRequested{},ProbeRequested{},BuildingsRequested{};
    ImTextureData* Texture=nullptr;std::vector<ImTextureData*> Retired;
};
inline State Data;
inline void ImportPrefab(){
    wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"RSDW Builder export (*.json)\0*.json\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
    if(!GetOpenFileNameW(&dialog))return;
    try{const std::filesystem::path path(file);if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("Prefab export exceeds 1 MiB");
        std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read prefab export");auto document=Json::parse(input);
        MapSpawnRequest::Prefab(Data.Projection,Data.Point,document,"validation",Data.Yaw,Data.Generation,0);
        Data.Prefab=std::move(document);Data.PrefabName=path.filename().string();Data.Building=Json::object();Data.Error.clear();
    }catch(const std::exception& error){Data.Error=error.what();}
}
inline bool RiskOverlayActive(){return Data.Operation==2||!Data.PartyPicking.empty()||Data.PartyConfirmWanted;}
inline std::pair<std::string,std::string> RiskIdentity(){
    const auto path=!Data.PartyPicking.empty()?Data.PartyPicking:Data.PartyConfirmWanted?Data.PartyMoving:Data.Operation==2?SpawnToolsPanel::SelectedPlayer:std::string{};
    if(const auto* row=MapParty::Find(Data.Snapshot,path))return {path,ItemPresentation::Text(*row,"Guid")};return {path,{}};
}
inline std::vector<std::string> WikiNames(){std::vector<std::string> out;const auto& data=HelpyMapCache::Current().Data;if(data)for(const auto& category:data->Categories)out.push_back(category.Name);return out;}
inline void SaveNativeFilters(){auto& filters=HelpyMapCache::Current().Filters;filters.Players=Data.ShowPlayers;filters.Lodestones=Data.ShowLodestones;filters.Placements=Data.ShowPlaced;filters.Resources=Data.ShowResources;filters.AI=Data.ShowAI;filters.Chests=Data.ShowChests;HelpyMapCache::SaveFilters();Data.Next={};}
inline void Filters(){
    auto& cache=HelpyMapCache::Current();const auto& theme=HelpyImGuiTheme::Current;
    if(HelpyImGuiTheme::SoftButton("Filters..."))ImGui::OpenPopup("Map filters");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapFiltersButton=HelpyPreviewHooks::LastRect();
#endif
    ImGui::SetNextWindowSizeConstraints({theme.X(300),0},{std::max(theme.X(300),ImGui::GetIO().DisplaySize.x-theme.X(32)),ImGui::GetIO().DisplaySize.y-theme.Y(24)});
    if(!ImGui::BeginPopup("Map filters"))return;
    ImGui::BeginChild("##filter-body",{theme.X(330),std::max(theme.Y(120),ImGui::GetIO().DisplaySize.y-theme.Y(160))},false);
    bool changed=false;ImGui::SeparatorText("Cached map layers");
    changed|=ImGui::Checkbox("Show cached locations",&cache.Filters.ShowCached);
    if(cache.Data){
        if(ImGui::SmallButton("Show all")){cache.Filters.ShowCached=true;cache.Filters.Hidden.clear();changed=true;}ImGui::SameLine();
        if(ImGui::SmallButton("Hide all")){for(const auto& category:cache.Data->Categories)cache.Filters.Hidden.insert(category.Key);changed=true;}
        ImGui::BeginChild("##map-filter-list",{theme.X(320),std::min(theme.Y(190),ImGui::GetIO().DisplaySize.y*.22f)},true);
        size_t at=0;while(at<cache.Data->Categories.size()){
            const auto& first=cache.Data->Categories[at];size_t end=at;while(end<cache.Data->Categories.size()&&cache.Data->Categories[end].Group==first.Group)++end;
            bool all=true;size_t total=0;for(size_t i=at;i<end;++i){all=all&&!cache.Filters.Hidden.contains(cache.Data->Categories[i].Key);total+=cache.Data->Categories[i].Count;}
            ImGui::PushID(first.Group.c_str());
            if(ImGui::Checkbox("##all",&all)){for(size_t i=at;i<end;++i){const auto& key=cache.Data->Categories[i].Key;if(all)cache.Filters.Hidden.erase(key);else cache.Filters.Hidden.insert(key);}changed=true;}
            ImGui::SameLine();
            MapLayerIcons::Inline(first.Group);
#if defined(HELPY_PREVIEW)
            if(HelpyPreviewHooks::MapForceGroupsOpen)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
#endif
            const bool expanded=ImGui::TreeNode("##group","%s (%zu)",first.GroupLabel.c_str(),total);
            if(expanded){for(size_t i=at;i<end;++i){const auto& category=cache.Data->Categories[i];if(!MapFilters::Matches(MapFilters::Fold(category.Name+" "+category.GroupLabel),Data.MarkerSearch))continue;
                bool show=!cache.Filters.Hidden.contains(category.Key);const auto label=category.Name+" ("+std::to_string(category.Count)+")";
                ImGui::PushID(category.Key.c_str());if(ImGui::Checkbox("##visible",&show)){if(show)cache.Filters.Hidden.erase(category.Key);else cache.Filters.Hidden.insert(category.Key);changed=true;}
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::MapCategoryControls[category.Key]=HelpyPreviewHooks::LastRect();
#endif
                ImGui::SameLine();const auto side=theme.Y(20);const auto at=ImGui::GetCursorScreenPos();MapLayerIcons::Resource(HelpyGlyphs::Resource(category.Group,MapFilters::Fold(category.Name)),category.Name,{at.x+side*.5f,at.y+side*.5f},side);ImGui::Dummy({side,side});ImGui::SameLine();ImGui::TextUnformatted(label.c_str());ImGui::PopID();
            }ImGui::TreePop();}ImGui::PopID();at=end;
        }ImGui::EndChild();
        changed|=ImGui::Checkbox("Update after confirmed game patches",&cache.Filters.UpdateAfterGamePatch);
        if(ImGui::TreeNode("Cache details")){
            ImGui::TextWrapped("Reference data: %s. Cached for %s. No timed online polling.",cache.Data->Built.c_str(),cache.CachedBuild.empty()?"unconfirmed build":cache.CachedBuild.c_str());
            ImGui::TextWrapped("%s",MapFilters::Credit);ImGui::TextLinkOpenURL("Data source",MapFilters::SourceUrl);ImGui::SameLine();ImGui::TextLinkOpenURL("Licence",MapFilters::LicenseUrl);
            ImGui::TextWrapped("Reference locations, not live resource availability.");ImGui::TreePop();
        }
    }else ImGui::TextWrapped("Use Download Initial Assets on the splash or in Settings > Updates & runtime > Initial assets.");
    ImGui::BeginDisabled(HelpyWikiIcons::Busy()||!cache.Data);if(HelpyImGuiTheme::SoftButton("Download wiki pictures & journals"))HelpyWikiIcons::Start(WikiNames(),cache.CachedBuild);ImGui::EndDisabled();
    if(!HelpyWikiIcons::Status().empty())ImGui::TextWrapped("%s",HelpyWikiIcons::Status().c_str());
    ImGui::SeparatorText("Loaded game areas");
    bool nativeChanged=ImGui::Checkbox("Players",&Data.ShowPlayers);ImGui::SameLine();nativeChanged|=ImGui::Checkbox("Lodestones",&Data.ShowLodestones);
    nativeChanged|=ImGui::Checkbox("RuneSchema placements",&Data.ShowPlaced);
    nativeChanged|=ImGui::Checkbox("Placed chests / storage (loaded)",&Data.ShowChests);
    nativeChanged|=ImGui::Checkbox("Loaded resources",&Data.ShowResources);ImGui::SameLine();nativeChanged|=ImGui::Checkbox("AI spawn points",&Data.ShowAI);
    changed|=ImGui::Checkbox("Observe nodes near party members",&cache.Filters.ObserveNearbyResources);
    float resourceRadius=static_cast<float>(cache.Filters.ResourceRadiusMetres);ImGui::SetNextItemWidth(theme.X(260));
    if(ImGui::SliderFloat("Observation radius",&resourceRadius,25,2000,"%.0f m",ImGuiSliderFlags_Logarithmic)){cache.Filters.ResourceRadiusMetres=resourceRadius;changed=true;Data.Next={};}
    ImGui::TextWrapped("Loaded resources only, near any connected member. Native availability is measured; a countdown is shown only with a verified deadline. Beds, players or buildings may delay respawn.");
    if(nativeChanged)SaveNativeFilters();
    ImGui::TextWrapped("Loaded areas vs. cached world references. Search applies to both.");
    if(changed)HelpyMapCache::SaveFilters();
    ImGui::EndChild();
    if(HelpyImGuiTheme::SoftButton("Done"))ImGui::CloseCurrentPopup();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapFiltersClose=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndPopup();
}
inline void ReadGround(MapProjection::Point point,bool selected){
    const auto now=Clock::now();if(now<Data.NextGroundRead||SpawnToolRequests::IsWaiting()||!SpawnToolRequests::IsAvailable())return;
    const auto id=++Data.Sequence;Data.NextGroundRead=now+std::chrono::milliseconds(150);
    const auto [player,guid]=RiskIdentity();
    if(SpawnToolRequests::SubmitMapGround({{"RequestId",id},{"Generation",Data.Generation},{"RiskPlayer",player},{"RiskGuid",guid},{"TargetLocation",{{"X",point.X},{"Y",point.Y}}}})){
        Data.GroundReadPending=id;Data.GroundReadPoint=point;Data.GroundReadSelected=selected;Data.GroundReadAt=now;
    }
}
inline void Reap(){for(auto it=Data.Retired.begin();it!=Data.Retired.end();){auto* texture=*it;if(texture->Status!=ImTextureStatus_Destroyed){++it;continue;}ImGui::UnregisterUserTexture(texture);IM_DELETE(texture);it=Data.Retired.erase(it);}}
inline PS::ProcessExitFuture<void> MapCacheWriter;
inline void LoadImage(const Json& image,bool remember=true){
    const auto file=ItemPresentation::Text(image,"File");if(file.empty()||file==Data.ImageFile||ItemPresentation::Text(image,"Status")!="Ready")return;
    std::ifstream input(std::filesystem::path(file),std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Native map image is not accessible");
    const auto bytes=input.tellg();if(bytes<16||bytes>16+2048*2048*4)throw std::runtime_error("Native map image exceeds its bounds");
    input.seekg(0);char magic[4]{};uint32_t width=0,height=0,size=0;input.read(magic,4);input.read(reinterpret_cast<char*>(&width),4);input.read(reinterpret_cast<char*>(&height),4);input.read(reinterpret_cast<char*>(&size),4);
    if(!input||std::string(magic,4)!="RSI3"||width<1||height<1||width>2048||height>2048||size!=width*height*4||bytes!=static_cast<std::streamoff>(16+size))throw std::runtime_error("Native map image has an invalid format");
    std::vector<unsigned char> pixels(size);input.read(reinterpret_cast<char*>(pixels.data()),size);if(!input)throw std::runtime_error("Native map image is incomplete");
    Data.Ocean=MapOceanMask::Mask::Build(pixels,int(width),int(height));
    auto* texture=IM_NEW(ImTextureData)();texture->Create(ImTextureFormat_RGBA32,width,height);std::memcpy(texture->GetPixels(),pixels.data(),size);texture->UseColors=true;texture->RefCount=1;texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(texture);
    if(Data.Texture){Data.Texture->SetStatus(ImTextureStatus_WantDestroy);Data.Retired.push_back(Data.Texture);}Data.Texture=texture;Data.ImageFile=file;
    if(remember&&Data.Projection.Valid()&&!MapCacheWriter.valid()&&HelpyRuntimePreferences::IconCacheLimitMB.load()>0){
        const auto cache=HostServices::CacheDirectory()/"item-icons";const auto projection=Data.Projection.ToJson();
        MapCacheWriter=std::async(std::launch::async,[cache,projection,file]{MapPresentationCache::Capture(cache,projection,std::filesystem::path(file));});
    }
}
inline void Poll(){
    if(MapCacheWriter.valid()&&MapCacheWriter.wait_for(std::chrono::seconds(0))==std::future_status::ready)try{MapCacheWriter.get();}catch(const std::exception& error){Data.Error=error.what();}
    Reap();if(Data.Texture&&Data.Texture->Status==ImTextureStatus_OK&&Data.Texture->Pixels)Data.Texture->DestroyPixels();
    const auto generation=SpawnToolRequests::CurrentGeneration();const auto now=Clock::now();
    if(generation!=Data.Generation){Data.RiskSamples.clear();Data.GroundReadPending=0;Data.HoverActive=false;Data.HoverGround=Json::object();Data.NextGroundRead={};Data.View={};Data.FitRequested=false;Data.BuildingAction=Data.BuildingBrowserOpen=false;Data.PlacementResult=Json::object();Data.BuildingCategories=Json::array();Data.BuildingCategory=-1;}
    if(generation!=Data.Generation){Data.PartyMoving.clear();Data.PartyDestination.clear();Data.PartyPicking.clear();Data.PartyConfirmWanted=false;Data.PartySelection.clear();Data.PartyTarget.clear();Data.Generation=generation;Data.Pending=Data.SpawnPending=Data.ProbePending=Data.BuildingsPending=0;Data.Snapshot=Json::object();Data.Projection={};Data.Definition=Data.Building=Data.Ground=Json::object();Data.Buildings=Json::array();Data.ProbeWanted=Data.BuildingsWanted=Data.HasPoint=false;Data.Next={};Data.Popup=false;Data.Error.clear();Data.Status.clear();Data.Center={0.5,0.5};Data.Zoom=1;Data.TravelAction=Data.CancelSent=false;}
    static uint64_t mapRevision=UINT64_MAX;static Json cachedMap=Json::object();
    if(mapRevision!=MapPresentationCache::Revision.load()){
        mapRevision=MapPresentationCache::Revision.load();try{cachedMap=MapPresentationCache::Reference(HostServices::CacheDirectory()/"item-icons");}catch(const std::exception& error){cachedMap=Json::object();Data.Error=error.what();}
    }
    const auto useCachedMap=[&]{if(!cachedMap.empty()&&(!Data.Projection.Valid()||Data.Snapshot.value("CachedMapReference",false))){
        Data.Projection=MapProjection::Projection::From(cachedMap["Projection"]);Data.Snapshot["Projection"]=cachedMap["Projection"];Data.Snapshot["Image"]=cachedMap["Image"];Data.Snapshot["CachedMapReference"]=true;LoadImage(cachedMap["Image"],false);
    }};
    try{useCachedMap();}catch(const std::exception& error){Data.Error=error.what();}
    Json receipt;
    const auto& live=SpawnToolsPanel::UiSnapshot;
    if(Data.Snapshot.contains("Players")&&live.contains("Players"))PartyStatWire::Merge(Data.Snapshot["Players"],live["Players"],PartyMemberView::Now());
    if(Data.GroundReadPending&&live.contains("MapGround")&&live["MapGround"].is_object()){
        const auto& ground=live["MapGround"];
        if(ground.value("Generation",uint64_t{})==Data.Generation&&ground.value("RequestId",uint64_t{})==Data.GroundReadPending){
            if(ground.contains("LandingRisk")){Data.RiskSamples.push_back({Data.GroundReadPoint,ground.value("RiskPlayer",std::string{}),ground.value("RiskGuid",std::string{}),ground});if(Data.RiskSamples.size()>64)Data.RiskSamples.erase(Data.RiskSamples.begin());}
            if(Data.GroundReadSelected&&Data.Point.X==Data.GroundReadPoint.X&&Data.Point.Y==Data.GroundReadPoint.Y)Data.Ground=ground;
            else if(!Data.GroundReadSelected&&Data.HoverActive&&Data.HoverPoint.X==Data.GroundReadPoint.X&&Data.HoverPoint.Y==Data.GroundReadPoint.Y){Data.HoverGround=ground;Data.HoverFreshUntil=now+std::chrono::seconds(2);}
            Data.GroundReadPending=0;
        }
    }
    if(Data.GroundReadPending&&now-Data.GroundReadAt>std::chrono::seconds(4)){Data.GroundReadPending=0;Data.NextGroundRead=now+std::chrono::seconds(1);}
    if(Data.SpawnPending&&live.value("_RequestId",uint64_t{})==Data.SpawnPending&&live.contains("MapPoint"))Data.Ground=live["MapPoint"];
    if(Data.SpawnPending&&SpawnToolRequests::TakeCompleted(Data.SpawnPending,receipt)){Data.SpawnPending=0;Data.TravelAction=false;Data.Status=ItemPresentation::Text(receipt,"Status");if(receipt.contains("MapPoint"))Data.Ground=receipt["MapPoint"];if(Data.BuildingAction)Data.PlacementResult=receipt.value("SpawnResult",Json{{"Active",false},{"Error",Data.Status}});Data.Next={};}
    if(Data.ProbePending&&SpawnToolRequests::TakeCompleted(Data.ProbePending,receipt)){Data.ProbePending=0;if(Data.Point.X==Data.ProbePoint.X&&Data.Point.Y==Data.ProbePoint.Y)Data.Ground=receipt.value("MapPoint",Json{{"GroundAvailable",false},{"Error",ItemPresentation::Text(receipt,"Status")}});}
    if(Data.BuildingsPending&&SpawnToolRequests::TakeCompleted(Data.BuildingsPending,receipt)){
        Data.BuildingsPending=0;
        if(receipt.contains("MapBuildings")&&receipt["MapBuildings"].is_object()){
            const auto& page=receipt["MapBuildings"];if(!Data.BuildingsSentOffset)Data.Buildings=Json::array();
            if(page.contains("Rows")&&page["Rows"].is_array())for(const auto& row:page["Rows"])if(row.is_object()&&Data.Buildings.size()<4096)Data.Buildings.push_back(row);
            Data.BuildingsNext=page.value("NextOffset",size_t{});Data.BuildingsMore=page.value("HasMore",false);
            if(page.contains("Categories")&&page["Categories"].is_array())Data.BuildingCategories=page["Categories"];
            if(Data.BuildingsMore&&Data.BuildingBrowserOpen){Data.BuildingsOffset=Data.BuildingsNext;Data.BuildingsWanted=true;}
        }else Data.Error=ItemPresentation::Text(receipt,"Status");
    }
    if(Data.Pending&&SpawnToolRequests::TakeCompleted(Data.Pending,receipt)){
        Data.Pending=0;
        if(receipt.contains("MapSnapshot")&&receipt["MapSnapshot"].is_object()){
            Data.Snapshot=receipt["MapSnapshot"];Data.Projection=MapProjection::Projection::From(Data.Snapshot.value("Projection",Json::object()));
            try{const auto error=ItemPresentation::Text(Data.Snapshot,"Error");if(!error.empty())throw std::runtime_error(error);if(!Data.Projection.Valid())throw std::runtime_error("The game did not provide valid map alignment");LoadImage(Data.Snapshot.value("Image",Json::object()));Data.Error.clear();}
            catch(const std::exception& error){Data.Error=error.what();try{useCachedMap();}catch(...) {}}
        }else Data.Error="Map data is unavailable. Install the matching RuneSchema Map bridge and load a world.";
    }
    if(Data.Pending&&now-Data.Requested>std::chrono::seconds(15)){Data.Pending=0;Data.Error="Map refresh timed out. Use Refresh to try again.";}
    if(Data.ProbePending&&now-Data.ProbeRequested>std::chrono::seconds(15)){Data.ProbePending=0;Data.Error="Coordinate lookup timed out. Select the point again.";}
    if(Data.BuildingsPending&&now-Data.BuildingsRequested>std::chrono::seconds(15)){Data.BuildingsPending=0;Data.Error="Building menu lookup timed out. Search again to retry.";}
    if(Data.SpawnPending&&now-Data.ActionRequested>std::chrono::seconds(100)){Data.SpawnPending=0;Data.Status="The action has not confirmed completion. Check the game before retrying.";if(Data.BuildingAction)Data.PlacementResult={{"Active",false},{"Error",Data.Status}};}
    if(!SpawnToolRequests::IsAvailable()||SpawnToolRequests::IsWaiting()||Data.Pending||Data.SpawnPending||Data.ProbePending||Data.BuildingsPending)return;
    if(Data.Operation!=2&&Data.ProbeWanted){const auto id=++Data.Sequence;if(SpawnToolRequests::TrySubmit({{"Action","MapProbe"},{"NearMarker",Data.NearMarker},{"_Source","HelpyMap"},{"_RequestId",id},{"TargetLocation",{{"X",Data.Point.X},{"Y",Data.Point.Y}}}})){Data.ProbePoint=Data.Point;Data.ProbePending=id;Data.ProbeRequested=now;Data.ProbeWanted=false;}return;}
    if(Data.Operation==1&&Data.BuildingsWanted&&now>=Data.BuildingSearchDue){const auto id=++Data.Sequence;if(SpawnToolRequests::TrySubmit({{"Action","MapBuildings"},{"_Source","HelpyMap"},{"_RequestId",id},{"Query",""},{"Offset",Data.BuildingsOffset}})){Data.BuildingsPending=id;Data.BuildingsSentQuery="";Data.BuildingsSentOffset=Data.BuildingsOffset;Data.BuildingsRequested=now;Data.BuildingsWanted=false;}return;}
    if(!Data.Popup&&now>=Data.Next){
        const auto id=++Data.Sequence;Data.Next=now+std::chrono::milliseconds(750);
        const auto& options=HelpyMapCache::Current().Filters;
        if(SpawnToolRequests::TrySubmit({{"Action","MapSnapshot"},{"Lodestones",Data.ShowLodestones},{"Placements",Data.ShowPlaced},{"Resources",Data.ShowResources},{"AI",Data.ShowAI},{"Chests",Data.ShowChests},{"ObserveNearbyResources",options.ObserveNearbyResources},{"ResourceRadiusMetres",options.ResourceRadiusMetres},{"_Source","HelpyMap"},{"_RequestId",id}})){Data.Pending=id;Data.Requested=now;}
    }
}
inline void BuildingBrowser(){
    const auto& t=HelpyImGuiTheme::Current;const auto screen=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(t.X(960),screen.x-t.X(32)),std::min(t.Y(650),screen.y-t.Y(32))),ImGuiCond_Appearing);
    bool open=true;
    if(!HelpyImGuiTheme::BeginCenteredModal("Building menu",&open,ImGuiWindowFlags_NoSavedSettings)){Data.BuildingBrowserOpen=false;return;}
    Data.BuildingBrowserOpen=true;
    ImGui::TextDisabled("Game building menu");
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##building-menu-search","Search pieces, categories or groups...",Data.BuildingSearch,sizeof(Data.BuildingSearch));
    if(Data.BuildingsPending||Data.BuildingsWanted)ImGui::TextDisabled("Loading menu... %zu pieces received",Data.Buildings.size());
    else ImGui::TextDisabled("%zu pieces",Data.Buildings.size());
    const float body=std::max(t.Y(80),ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*2-t.Y(12));
    if(ImGui::BeginChild("##building-categories",ImVec2(t.X(150),body),true)){
        if(ImGui::Selectable("All categories",Data.BuildingCategory<0))Data.BuildingCategory=-1;
        for(const auto& category:Data.BuildingCategories){
            const auto id=category.value("Id",-1);const auto name=ItemPresentation::Text(category,"Name"),icon=ItemPresentation::Text(category,"Icon");ImGui::PushID(id);
            const auto a=ImGui::GetCursorScreenPos();const float iconSize=t.Y(18),labelWidth=std::max(1.0f,ImGui::GetContentRegionAvail().x-t.X(28));
            const float height=std::max(t.Y(28),ImGui::CalcTextSize(name.c_str(),nullptr,false,labelWidth).y+t.Y(6));
            if(ImGui::Selectable("##category",Data.BuildingCategory==id,ImGuiSelectableFlags_None,ImVec2(0,height)))Data.BuildingCategory=id;
            if(!icon.empty())ItemIconCache::Draw(icon,ImVec2(a.x,a.y+(height-iconSize)*.5f),ImVec2(a.x+iconSize,a.y+(height+iconSize)*.5f),true);
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(a.x+t.X(25),a.y+t.Y(3)),ImGui::GetColorU32(t.text),name.c_str(),nullptr,labelWidth);
            ImGui::PopID();
        }
    }ImGui::EndChild();ImGui::SameLine();
    if(ImGui::BeginChild("##building-grid",ImVec2(0,body),true)){
        const float width=ImGui::GetContentRegionAvail().x;const int columns=std::max(1,static_cast<int>((width+t.X(8))/(t.X(132)+t.X(8))));
        const float cardWidth=std::max(1.0f,(width-t.X(8)*(columns-1))/columns);size_t shown=0;
        for(const auto& row:Data.Buildings){if(!BuildingMenu::Matches(row,Data.BuildingSearch,Data.BuildingCategory))continue;
            if(shown%columns)ImGui::SameLine(0,t.X(8));
            const auto path=ItemPresentation::Text(row,"Path"),name=ItemPresentation::Text(row,"Name");ImGui::PushID(path.c_str());
            const auto start=ImGui::GetCursorScreenPos();const bool selected=path==ItemPresentation::Text(Data.Building,"Path");
            ImGui::PushStyleColor(ImGuiCol_Header,selected?t.selection:t.panelAlt);
            if(ImGui::Selectable("##piece",selected,ImGuiSelectableFlags_None,ImVec2(cardWidth,t.Y(116)))){Data.Building=row;Data.Prefab=Json::object();Data.PrefabName.clear();}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::BuildingCards[path]=HelpyPreviewHooks::LastRect();
#endif
            const bool hovered=ImGui::IsItemHovered();ImGui::PopStyleColor();
            const float icon=std::min(t.Y(56),cardWidth-t.X(16));const ImVec2 a(start.x+(cardWidth-icon)*.5f,start.y+t.Y(7)),b(a.x+icon,a.y+icon);
            if(!ItemIconCache::Draw(ItemPresentation::Text(row,"Icon"),a,b,true))HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Building,ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f),icon*.6f,t.textDisabled);
            ImGui::GetWindowDrawList()->AddRect(start,ImVec2(start.x+cardWidth,start.y+t.Y(116)),ImGui::GetColorU32(selected?t.accent:t.border),t.X(5));
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(start.x+t.X(7),start.y+t.Y(70)),ImGui::GetColorU32(t.text),name.c_str(),nullptr,cardWidth-t.X(14));
            if(hovered)PS::HelpyImGuiTheme::Tooltip("%s\n%s / %s",name.c_str(),ItemPresentation::Text(row,"Category").c_str(),ItemPresentation::Text(row,"Group").c_str());
            ImGui::PopID();++shown;
        }
        if(!shown)ImGui::TextWrapped("%s",Data.BuildingsPending||Data.BuildingsWanted?"Looking through the game's menu...":"No pieces match this search/category.");
    }ImGui::EndChild();
    ImGui::TextWrapped("Selected: %s",Data.Building.empty()?"None":ItemPresentation::Text(Data.Building,"Name").c_str());
    ImGui::BeginDisabled(Data.Building.empty());if(HelpyImGuiTheme::SoftButton("Use selected piece")){Data.BuildingBrowserOpen=false;ImGui::CloseCurrentPopup();}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BuildingUse=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Close menu")){Data.BuildingBrowserOpen=false;ImGui::CloseCurrentPopup();}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BuildingClose=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndPopup();
}
inline void PlacePopup(){
    const auto& theme=HelpyImGuiTheme::Current;
    if(!ImGui::IsPopupOpen("##map-place")){Data.Popup=false;return;}
    const auto* viewport=ImGui::GetMainViewport();const auto display=viewport->WorkSize;
    const ImVec2 size{std::min(theme.X(400),display.x-theme.X(24)),std::min(theme.Y(Data.Operation==2?400:650),display.y-theme.Y(24))};
    // Fill-width children cannot be measured by an auto-fit parent. Pin both
    // axes for the lifetime of this popup instead of shrinking one frame at a time.
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::SetNextWindowPos(viewport->GetWorkCenter(),ImGuiCond_Always,{.5f,.5f});
    if(!ImGui::BeginPopup("##map-place")){Data.Popup=false;return;}Data.Popup=true;const auto& t=HelpyImGuiTheme::Current;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapPopupWindow={ImGui::GetWindowPos().x,ImGui::GetWindowPos().y,ImGui::GetWindowPos().x+size.x,ImGui::GetWindowPos().y+size.y};
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign,{.5f,.5f});
    ImGui::BeginChild("##map-options-body",{0,std::max(t.Y(60),ImGui::GetContentRegionAvail().y-t.Y(Data.Operation==2?80:110))},false);
#if defined(HELPY_PREVIEW)
    {const auto a=ImGui::GetCursorScreenPos();HelpyPreviewHooks::MapOptionsBody={a.x,a.y,a.x+ImGui::GetContentRegionAvail().x,a.y};}
#endif
    HelpyImGuiTheme::DetailsTextUnformatted(Data.SelectionName.empty()?"Selected map point":Data.SelectionName.c_str());
    HelpyImGuiTheme::DetailsText("X %.0f   Y %.0f",Data.Point.X,Data.Point.Y);
    const bool groundReady=Data.Ground.value("GroundAvailable",false)&&Data.Ground.contains("Location")&&Data.Ground["Location"].is_array()&&Data.Ground["Location"].size()==3;
    if(groundReady){HelpyImGuiTheme::DetailsText("Z %.0f (ground)",Data.Ground["Location"][2].get<double>());HelpyImGuiTheme::CenterRow(ImGui::CalcTextSize("Copy coordinates").x+ImGui::GetStyle().FramePadding.x*2);if(ImGui::SmallButton("Copy coordinates")){char value[128];std::snprintf(value,sizeof(value),"X=%.0f Y=%.0f Z=%.0f",Data.Point.X,Data.Point.Y,Data.Ground["Location"][2].get<double>());ImGui::SetClipboardText(value);}}
    else if(Data.Operation!=2)HelpyImGuiTheme::DetailsText("%s",Data.ProbePending||Data.ProbeWanted?"Reading ground height...":ItemPresentation::Text(Data.Ground,"Error").c_str());
    ImGui::Separator();
    const auto radioWidth=[&](const char* text){return ImGui::CalcTextSize(text).x+ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x;};
    HelpyImGuiTheme::CenterRow(radioWidth("Spawn")+radioWidth("Build")+radioWidth("Teleport")+ImGui::GetStyle().ItemSpacing.x*2);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapOperationRow={ImGui::GetCursorScreenPos().x,0,0,0};
#endif
    if(ImGui::RadioButton("Spawn",Data.Operation==0)){Data.Operation=0;if(!groundReady)Data.ProbeWanted=true;}ImGui::SameLine();
    if(ImGui::RadioButton("Build",Data.Operation==1)){Data.Operation=1;Data.Permanent=true;if(!groundReady)Data.ProbeWanted=true;if(Data.Buildings.empty()){Data.BuildingsOffset=0;Data.BuildingsWanted=true;}}ImGui::SameLine();
    if(ImGui::RadioButton("Teleport",Data.Operation==2)){Data.Operation=2;Data.ProbeWanted=false;}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapOperationRow.z=ImGui::GetItemRectMax().x;
#endif
    if(Data.Operation==2){
        if(Data.Ground.empty()&&!Data.GroundReadPending)ReadGround(Data.Point,true);
        if(!groundReady)ImGui::TextDisabled("%s",Data.GroundReadPending?"Reading native ground Z (optional)...":"Ground Z unavailable here; travel prepares the destination separately.");
    }
    if(Data.Operation==0){
    const char* types[]{"Resources", "Ore / mineral nodes", "Trees / wood", "Enemies / AI"};
    ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##spawn-type",&Data.SpawnType,types,4)){
        Data.Kind=Data.SpawnType==3?1:2;Data.Definition=Json::object();Data.SearchWanted=true;Data.SearchDue={};
    }
    SpawnToolsPanel::RefreshSnapshot(Data.Kind);auto& window=LazyCatalog::For(Data.Kind);
    if(!window.Started&&!window.Pending){Data.SearchWanted=true;Data.SearchDue={};}
    ImGui::SetNextItemWidth(-1);const bool enter=ImGui::InputTextWithHint("##map-spawn-search","Iron ore node... (Enter to search)",Data.Search,sizeof(Data.Search),ImGuiInputTextFlags_EnterReturnsTrue);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapSearchField=HelpyPreviewHooks::LastRect();
#endif
    if(Data.TypedSearch!=Data.Search){Data.TypedSearch=Data.Search;Data.Definition=Json::object();Data.SearchWanted=Data.LiveSearch;Data.SearchDue=Clock::now()+std::chrono::milliseconds(400);}
    HelpyImGuiTheme::CenterRow(ImGui::CalcTextSize("Search").x+ImGui::GetStyle().FramePadding.x*2+radioWidth("Search as I type")+ImGui::GetStyle().ItemSpacing.x);
    const bool search=ImGui::Button("Search");ImGui::SameLine();ImGui::Checkbox("Search as I type",&Data.LiveSearch);
    if(enter||search){Data.SearchWanted=true;Data.SearchDue={};}
    const auto query=std::string(Data.Search)+(Data.SpawnType==1?" ore node":Data.SpawnType==2?" tree":"");
    if(Data.SearchWanted&&Clock::now()>=Data.SearchDue){LazyCatalog::Search(Data.Kind,query);Data.SearchWanted=false;}
    ImGui::BeginChild("##spawn-results",ImVec2(0,t.Y(130)),true);
    size_t matches=0;
    if(!Data.SearchWanted&&window.Query==query)for(const auto& row:window.Rows){
        const auto type=ItemPresentation::Text(row,"Type");if(type!=(Data.Kind==2?"Resource":"AI")||!SpawnCatalogSearch::Matches(row,query))continue;
        ++matches;const auto key=ItemPresentation::Text(row,"Key");ImGui::PushID(key.c_str());
        if(ImGui::Selectable(ItemPresentation::Text(row,"Name").c_str(),ItemPresentation::Text(Data.Definition,"Key")==key))Data.Definition=row;ImGui::PopID();
    }
    if(!matches)ImGui::TextWrapped("%s",window.Pending||Data.SearchWanted?"Searching native actors...":"No matching actors in this slice.");
    if(window.HasMore&&!window.Pending&&!Data.SearchWanted&&ImGui::SmallButton("Load more results"))LazyCatalog::More(Data.Kind);
    ImGui::EndChild();
    if(!Data.Definition.empty())ImGui::TextWrapped("Selected: %s",ItemPresentation::Text(Data.Definition,"Name").c_str());
    if(!window.LastError.empty())ImGui::TextWrapped("%s",window.LastError.c_str());
    }else if(Data.Operation==1){
        if(HelpyImGuiTheme::SoftButton("Browse building menu",ImVec2(-1,0))){Data.BuildingBrowserOpen=true;if(Data.Buildings.empty()||Data.BuildingsMore){Data.BuildingsOffset=Data.Buildings.empty()?0:Data.BuildingsNext;Data.BuildingsWanted=true;}ImGui::OpenPopup("Building menu");}
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BuildingBrowse=HelpyPreviewHooks::LastRect();
#endif
        BuildingBrowser();
        if(HelpyImGuiTheme::SoftButton("Import prefab...",ImVec2(-1,0)))ImportPrefab();
        ImGui::TextWrapped("Selected: %s",!Data.Prefab.empty()?Data.PrefabName.c_str():Data.Building.empty()?"Choose a building piece":ItemPresentation::Text(Data.Building,"Name").c_str());
        if(!Data.Prefab.empty()){ImGui::Text("%zu pieces · centered here",Data.Prefab["pieces"].size());if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("The layout's bounds center goes at the selected ground point. Relative height, scale and rotation are preserved. Ghosted pieces are excluded.");ImGui::TextDisabled("Buildings only; stored items aren't copied.");}
        if(Data.BuildingsPending||Data.BuildingsWanted)ImGui::TextDisabled("Reading the game's building menu...");
        HelpyImGuiTheme::DetailsTextUnformatted("Rotation");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##map-rotation",&Data.Yaw,0,360,"%.0f degrees");
        if(Data.Prefab.empty())HelpyImGuiTheme::DetailsTextDisabled("Native building checks apply.");
    }
    if(Data.Operation!=2){
    ImGui::Separator();if(Data.Operation==1){Data.Permanent=true;}
    else{HelpyImGuiTheme::CenterRow(radioWidth("Temporary")+radioWidth("Permanent")+ImGui::GetStyle().ItemSpacing.x);if(ImGui::RadioButton("Temporary",!Data.Permanent))Data.Permanent=false;ImGui::SameLine();if(ImGui::RadioButton("Permanent",Data.Permanent))Data.Permanent=true;}
    HelpyImGuiTheme::DetailsTextDisabled("%s",Data.Permanent?"Saved as RuneSchema placements.":"Temporary placement for this session.");
    }
    if(Data.Operation==0){
    HelpyImGuiTheme::DetailsTextUnformatted("Count");ImGui::SetNextItemWidth(-1);ImGui::InputInt("##map-count",&Data.Count);Data.Count=std::clamp(Data.Count,1,20);
    if(Data.Count>1){HelpyImGuiTheme::DetailsTextUnformatted("Spread (cm)");ImGui::SetNextItemWidth(-1);ImGui::InputFloat("##map-spread",&Data.Radius);Data.Radius=std::clamp(Data.Radius,0.0f,10000.0f);}
    }
    HelpyImGuiTheme::DetailsTextUnformatted(Data.Operation==2?"Player to teleport":"Player context");SpawnToolsPanel::RenderCartRecipient(SpawnToolsPanel::UiSnapshot);
    if(Data.Operation==2)HelpyImGuiTheme::DetailsText("%s",Data.NearMarker?"Loads this destination, checks ground beside the stone (within 2 metres), then uses native travel.":"Loads the destination before departure, verifies a safe landing, then uses the game's native travel and loading screen. Unsafe or unavailable ground cancels departure.");
    ImGui::EndChild();
    const bool choice=Data.Operation==2||(Data.Operation==1?(!Data.Building.empty()||!Data.Prefab.empty()):!Data.Definition.empty());
    const bool authority=Data.Operation==0||Data.Snapshot.value("CanBuildAndTeleport",false);
    if(!authority)ImGui::TextWrapped("Use the host's RSMK for building placement and player teleportation.");
    const bool destinationReady=Data.Operation==2||groundReady;
    const bool previewReady=Data.Operation==2||(!Data.ProbePending&&!Data.ProbeWanted&&!Data.BuildingsPending);
    const bool ready=authority&&destinationReady&&Data.Projection.Valid()&&choice&&!SpawnToolsPanel::SelectedPlayer.empty()&&!SpawnToolRequests::IsWaiting()&&!Data.SpawnPending&&previewReady;
    if(Data.Operation==1&&!ready){const auto reason=BuildingMenu::BlockedReason(authority,groundReady,Data.Projection.Valid(),choice,!SpawnToolsPanel::SelectedPlayer.empty(),SpawnToolRequests::IsWaiting(),!previewReady||Data.SpawnPending);ImGui::PushStyleColor(ImGuiCol_Text,t.warning);ImGui::TextWrapped("%s",reason.c_str());ImGui::PopStyleColor();}
    if(Data.Operation==1&&!groundReady&&!Data.ProbePending&&!Data.ProbeWanted&&HelpyImGuiTheme::SoftButton("Retry ground")){Data.Ground=Json::object();Data.ProbeWanted=true;}
    if(SpawnToolRequests::IsWaiting())ImGui::TextDisabled("Finishing the current runtime request...");
    ImGui::BeginDisabled(!ready);
    const char* action=Data.Operation==2?"Teleport player here":Data.Operation==1?(!Data.Prefab.empty()?"Place & save prefab":Data.Permanent?"Place & save building":"Place building here"):(Data.Permanent?"Spawn & save placement":"Spawn here");
    if(HelpyImGuiTheme::SoftButton(action,ImVec2(-1,0))){try{
        const auto id=++Data.Sequence;auto request=Data.Operation==2?MapSpawnRequest::Target(Data.Projection,Data.Point,"MapTeleport",SpawnToolsPanel::SelectedPlayer,id):Data.Operation==1?(!Data.Prefab.empty()?MapSpawnRequest::Prefab(Data.Projection,Data.Point,Data.Prefab,SpawnToolsPanel::SelectedPlayer,Data.Yaw,Data.Generation,id):MapSpawnRequest::Building(Data.Projection,Data.Point,Data.Building,SpawnToolsPanel::SelectedPlayer,Data.Permanent,Data.Yaw,id)):MapSpawnRequest::Build(Data.Projection,Data.Point,Data.Definition,SpawnToolsPanel::SelectedPlayer,Data.Permanent,Data.Count,Data.Radius,id);
        request["NearMarker"]=Data.Operation==2&&Data.NearMarker;
        // Z is a measured hint for destination streaming. Runtime collision
        // is always re-traced before placement/departure; never trust stale UI Z.
        if(groundReady)request["TargetLocation"]["Z"]=Data.Ground["Location"][2];
        if(SpawnToolRequests::TrySubmit(std::move(request))){Data.SpawnPending=id;Data.ActionRequested=Clock::now();Data.TravelAction=Data.Operation==2;Data.BuildingAction=Data.Operation==1;Data.PlacementResult=Json::object();Data.CancelSent=false;Data.ProbeWanted=false;Data.Status=Data.TravelAction?"Preparing destination. You stay in place until a safe landing is verified.":"Applying action at the selected map position...";ImGui::CloseCurrentPopup();Data.Popup=false;}else Data.Error="The runtime is busy. Try again.";
    }catch(const std::exception& error){Data.Error=error.what();}}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapSpawn=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();ImGui::PopStyleVar();ImGui::EndPopup();
}
inline void SelectPartyMember(const Json& player){
    Data.PartySelection=ItemPresentation::Text(player,"Path");
    const auto at=player.find("Location");if(at==player.end()||!at->is_array()||at->size()!=3||!(*at)[0].is_number()||!(*at)[1].is_number())return;
    const MapProjection::Point point{(*at)[0].get<double>(),(*at)[1].get<double>()};if(!Data.Projection.Contains(point))return;
    Data.Point=point;Data.HasPoint=true;Data.SelectionName=RuneSchemaProfile::Name(player);Data.Center=Data.Projection.ToUV(point);Data.NearMarker=false;
}
inline void PlanPartyTravel(const std::string& moving,const std::string& destination,bool map=false,bool picking=false){
    Data.PartyMoving=moving;Data.PartyDestination=destination;Data.PartyToMap=map;Data.PartyMapPoint=Data.Point;
    const auto* from=MapParty::Find(Data.Snapshot,moving);const auto* to=MapParty::Find(Data.Snapshot,destination);
    Data.PartyMovingGuid=from?ItemPresentation::Text(*from,"Guid"):"";Data.PartyDestinationGuid=to?ItemPresentation::Text(*to,"Guid"):"";
    Data.PartyPicking=picking?moving:"";Data.PartyConfirmWanted=!picking;
}
inline Json PartyRequest(){
    const auto* from=MapParty::Find(Data.Snapshot,Data.PartyMoving);const auto* to=MapParty::Find(Data.Snapshot,Data.PartyDestination);
    if(!from||ItemPresentation::Text(*from,"Guid")!=Data.PartyMovingGuid||(!Data.PartyToMap&&(!to||ItemPresentation::Text(*to,"Guid")!=Data.PartyDestinationGuid)))throw std::runtime_error("The selected party character changed; choose the member again.");
    return Data.PartyToMap?MapParty::ToPoint(Data.Snapshot,Data.PartyMoving,Data.Projection,Data.PartyMapPoint,Data.Generation,Data.Sequence+1):MapParty::Between(Data.Snapshot,Data.PartyMoving,Data.PartyDestination,Data.Generation,Data.Sequence+1);
}
inline void Party(ImVec2 mapMin,ImVec2 mapMax,bool detached=false){
    const auto& t=HelpyImGuiTheme::Current;
    if(!Data.ShowParty||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Party)||Data.PartyDetached!=detached)return;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::PartyGear.clear();HelpyPreviewHooks::PartyDurabilityMeters=0;HelpyPreviewHooks::PartyDurabilityDetails=false;
    HelpyPreviewHooks::PartyPower.clear();HelpyPreviewHooks::PartyPortraits.clear();HelpyPreviewHooks::PartyMeters.clear();
#endif
    bool expanded=false;
    if(detached){
        ImGui::SetNextWindowPos(mapMin);ImGui::SetNextWindowSize({mapMax.x-mapMin.x,mapMax.y-mapMin.y});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.025f,.028f,.025f,1));
        expanded=ImGui::Begin("Party##helpy-map-party",&Data.ShowParty,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        ImGui::PopStyleColor();
    }else{
        // A bounded in-flow section, never an overlay or auto-sized window.
        const float height=std::clamp(ImGui::GetContentRegionAvail().y,t.Y(100),t.Y(240));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.025f,.028f,.025f,1));
        expanded=ImGui::BeginChild("Party##helpy-map-party",{0,height},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleColor();
    }
    const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    if(expanded){
        ImGui::TextUnformatted("Party");ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton(detached?"Dock below map":"Detach Party"))Data.PartyDetached=!detached;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyDetach=HelpyPreviewHooks::LastRect();
#endif
        ImGui::SameLine();PartyMemberView::Advanced();ImGui::Separator();
        if(!PlayerInventoryPanel::RepairStatus.empty())ImGui::TextWrapped("%s",PlayerInventoryPanel::RepairStatus.c_str());
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapPartyWindow={ImGui::GetWindowPos().x,ImGui::GetWindowPos().y,ImGui::GetWindowPos().x+size.x,ImGui::GetWindowPos().y+size.y};
#endif
    const auto rows=Data.Snapshot.find("Players");
    const bool haveRows=rows!=Data.Snapshot.end()&&rows->is_array()&&!rows->empty();
    if(expanded&&!haveRows)ImGui::TextDisabled("No connected players in the current map snapshot.");
    if(expanded&&haveRows){
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
    ImGui::BeginChild("##map-party",{0,0},false);
    ImGui::PopStyleColor();
    const bool placards=PlayerPortraitPicker::Choices.CompactParty;const float cardWidth=placards?std::min(t.X(310),ImGui::GetContentRegionAvail().x):ImGui::GetContentRegionAvail().x;
    const auto cardColumns=std::max(1,int((ImGui::GetContentRegionAvail().x+ImGui::GetStyle().ItemSpacing.x)/(cardWidth+ImGui::GetStyle().ItemSpacing.x)));size_t cardIndex=0;
    for(const auto& row:*rows){if(!row.is_object())continue;const auto path=ItemPresentation::Text(row,"Path");if(path.empty())continue;
        if(placards&&cardIndex++%cardColumns)ImGui::SameLine();
        ImGui::PushID(path.c_str());const auto name=RuneSchemaProfile::Name(row)+(row.value("IsLocal",false)?" (you)":"");
        ImGui::PushStyleColor(ImGuiCol_Border,PartyMemberView::Border(row));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(t.panelAlt.x,t.panelAlt.y,t.panelAlt.z,.96f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{t.X(6),t.Y(6)});
        const float cardHeight=PartyMemberView::CardHeight(row,cardWidth-t.X(12));
        const bool card=ImGui::BeginChild("##party-member-card",{cardWidth,cardHeight},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();ImGui::PopStyleColor(2);
        if(card){
        {const auto a=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();SourceBanners::DrawCard(PlayerPortraitPicker::Choices.GetBackground(ItemPresentation::Text(row,"Guid")),a,{a.x+size.x,a.y+size.y});}
        PartyMemberView::RestoreGear=PlayerInventoryPanel::RestoreGear;PartyMemberView::ChangeGear=PlayerInventoryPanel::ChangeGear;
        if(PartyMemberView::Row(row,Data.PartySelection==path,false,true))SelectPartyMember(row);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapPartyRows[path]=HelpyPreviewHooks::LastRect();
#endif
        if(!PartyMemberView::GearContext&&ImGui::IsItemHovered()&&ImGui::IsMouseReleased(ImGuiMouseButton_Right)){Data.PartyContextAnchor=ImGui::GetMousePos();ImGui::OpenPopup("##party-options");}
        if(ImGui::IsPopupOpen("##party-options")){
            const auto area=ImGui::GetMainViewport()->WorkSize,origin=ImGui::GetMainViewport()->WorkPos;const float width=std::min(t.X(320),area.x-t.X(24));
            ImGui::SetNextWindowPos({std::clamp(Data.PartyContextAnchor.x,origin.x+t.X(8),std::max(origin.x+t.X(8),origin.x+area.x-width-t.X(8))),std::clamp(Data.PartyContextAnchor.y,origin.y+t.Y(8),std::max(origin.y+t.Y(8),origin.y+area.y-t.Y(220)))},ImGuiCond_Appearing);
            ImGui::SetNextWindowSize({width,0},ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(1.0f,area.y-t.Y(24))});
        }
        if(ImGui::BeginPopup("##party-options")){
#if defined(HELPY_PREVIEW)
            const auto a=ImGui::GetWindowPos(),b=ImGui::GetWindowSize();HelpyPreviewHooks::MapPartyContext={a.x,a.y,a.x+b.x,a.y+b.y};
#endif
            ImGui::TextUnformatted(name.c_str());ImGui::Separator();
            if(ImGui::MenuItem("View profile..."))PartyMemberView::ViewProfile(row);
            if(ImGui::MenuItem("Change portrait...",nullptr,false,!PlayerPortraits::Identity(ItemPresentation::Text(row,"Guid")).empty()))PlayerPortraitPicker::Request(row);
            PartyMemberView::BackgroundMenu(row);
            if(ImGui::MenuItem("View inventory...",nullptr,false,!ItemPresentation::Text(row,"Guid").empty()))PlayerInventoryPanel::Request(row);
            if(ImGui::MenuItem("Locate on map"))SelectPartyMember(row);
            std::string blocked;try{MapParty::Travel(Data.Snapshot,path,false,Data.Generation,1);}catch(const std::exception& error){blocked=error.what();}
            const bool runtimeReady=Data.Snapshot.value("CanBuildAndTeleport",false)&&Data.Snapshot.value("PartyTravelVersion",0)>=2&&!Data.SpawnPending&&!SpawnToolRequests::IsWaiting()&&SpawnToolRequests::IsAvailable();
            const bool ready=blocked.empty()&&runtimeReady;const auto* local=MapParty::Local(Data.Snapshot);
            if(ImGui::MenuItem("Teleport to",nullptr,false,ready)){Data.PartyTarget=path;Data.PartyRecall=false;PlanPartyTravel(ItemPresentation::Text(*local,"Path"),path);}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::MapPartyTeleport=HelpyPreviewHooks::LastRect();
#endif
            if(ImGui::MenuItem("Recall to me",nullptr,false,ready)){Data.PartyTarget=path;Data.PartyRecall=true;PlanPartyTravel(path,ItemPresentation::Text(*local,"Path"));}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::MapPartyRecall=HelpyPreviewHooks::LastRect();
#endif
            if(ImGui::BeginMenu("Teleport member to...",runtimeReady)){
                for(const auto& destination:*rows){const auto target=ItemPresentation::Text(destination,"Path");if(target.empty()||target==path)continue;ImGui::PushID(target.c_str());
                    const auto label=RuneSchemaProfile::Name(destination)+(destination.value("IsLocal",false)?" (me)":"");
                    if(ImGui::MenuItem(label.c_str()))PlanPartyTravel(path,target);ImGui::PopID();
                }
                ImGui::Separator();if(ImGui::MenuItem("Selected map point",nullptr,false,Data.HasPoint&&Data.Projection.Contains(Data.Point)))PlanPartyTravel(path,"",true);
                if(ImGui::MenuItem("Choose point on map..."))PlanPartyTravel(path,"",true,true);ImGui::EndMenu();
            }
            if(!runtimeReady)ImGui::TextWrapped("Requires the current host/server party runtime and an idle action queue.");
            ImGui::EndPopup();
        }
        if(!PlayerPortraitPicker::Choices.CompactParty&&PlayerPortraitPicker::Choices.Skills&&PartyMemberView::SkillsFresh(row,PartyMemberView::Now()))PartyMemberView::Skills(row,PartyMemberView::Now());
        }ImGui::EndChild();
        ImGui::PopID();
    }ImGui::EndChild();}
    if(Data.PartyConfirmWanted){Data.PartyConfirmWanted=false;ImGui::OpenPopup("Confirm party travel");}
    if(ImGui::IsPopupOpen("Confirm party travel")){
        ImGui::SetNextWindowSize({std::min(t.X(420),ImGui::GetIO().DisplaySize.x-t.X(32)),0},ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
    }
    if(HelpyImGuiTheme::BeginCenteredModal("Confirm party travel",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        const auto* moving=MapParty::Find(Data.Snapshot,Data.PartyMoving);const auto* target=MapParty::Find(Data.Snapshot,Data.PartyDestination);
        const auto name=moving?RuneSchemaProfile::Name(*moving):std::string("Disconnected player");
        if(Data.PartyToMap)ImGui::TextWrapped("Teleport %s to map point X %.0f, Y %.0f?",name.c_str(),Data.PartyMapPoint.X,Data.PartyMapPoint.Y);
        else ImGui::TextWrapped("Teleport %s to %s?",name.c_str(),target?ItemPresentation::Text(*target,"Name").c_str():"Disconnected player");
        ImGui::TextWrapped("The host checks current identities and a safe landing before departure.");
        std::string blocked;Json request;try{request=PartyRequest();}catch(const std::exception& error){blocked=error.what();}
        if(!blocked.empty())ImGui::TextWrapped("%s",blocked.c_str());
        ImGui::BeginDisabled(!blocked.empty()||Data.SpawnPending||SpawnToolRequests::IsWaiting()||!SpawnToolRequests::IsAvailable());
        if(HelpyImGuiTheme::SoftButton("Confirm")){
            if(SpawnToolRequests::TrySubmit(std::move(request))){Data.SpawnPending=++Data.Sequence;Data.ActionRequested=Clock::now();Data.TravelAction=true;Data.BuildingAction=false;Data.CancelSent=false;Data.ProbeWanted=false;Data.Status="Preparing safe party travel...";ImGui::CloseCurrentPopup();}
            else Data.Error="Runtime busy; no party travel submitted.";
        }
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapPartyConfirm=HelpyPreviewHooks::LastRect();
#endif
        ImGui::EndDisabled();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapPartyCancel=HelpyPreviewHooks::LastRect();
#endif
        ImGui::EndPopup();
    }
    if(detached)ImGui::End();else ImGui::EndChild();
}
inline std::string WikiUrl(const std::string& name){
    constexpr char hex[]="0123456789ABCDEF";std::string url="https://dragonwilds.runescape.wiki/?search=";
    for(unsigned char c:name){if(std::isalnum(c)||c=='-'||c=='_')url+=c;else{url+='%';url+=hex[c>>4];url+=hex[c&15];}}return url;
}
inline void ResourceDetails(){
    const auto& t=HelpyImGuiTheme::Current;if(Data.ResourceDetailsWanted){ImGui::OpenPopup("Map resource details");Data.ResourceDetailsWanted=false;}
    const auto area=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(t.X(510),area.x-t.X(24)),std::min(t.Y(390),area.y-t.Y(24))},ImGuiCond_Appearing);
    bool open=true;if(!HelpyImGuiTheme::BeginCenteredModal("Map resource details",&open,ImGuiWindowFlags_NoSavedSettings))return;
    auto& item=Data.ResourceDetails;const auto name=ItemPresentation::Text(item,"Name");HelpyImGuiTheme::DetailsTextUnformatted(name.c_str());
    HelpyImGuiTheme::DetailsTextDisabled("X %.0f   Y %.0f",item.value("X",0.0),item.value("Y",0.0));ImGui::Separator();
    if(ImGui::BeginTabBar("##resource-details-tabs",ImGuiTabBarFlags_Reorderable)){
        if(ImGui::BeginTabItem("Live availability")){
            // Refresh the selected native node by exact path only. A wiki point
            // is not silently matched to the nearest node or claimed available.
            const auto path=item.value("Path",std::string{});const Json* observation=nullptr;
            if(!path.empty()&&Data.Snapshot.contains("Markers"))for(const auto& marker:Data.Snapshot["Markers"])if(ItemPresentation::Text(marker,"Path")==path&&marker.contains("ResourceObservation")){observation=&marker["ResourceObservation"];break;}
            const auto now=PartyMemberView::Now();const auto at=observation?observation->value("ObservedUnixMs",int64_t{}):0;
            if(observation&&at>0&&now>=at&&now-at<=10000){
                const auto state=ItemPresentation::Text(*observation,"State");ImGui::Text("State: %s",state.empty()?"Unknown":state.c_str());
                if(observation->contains("Quantity"))ImGui::Text("Available: %d / %d",observation->value("Quantity",0),observation->value("Maximum",0));
                ImGui::TextWrapped("%s",ItemPresentation::Text(*observation,"TimerReason").c_str());ImGui::TextDisabled("Observed %.1f seconds ago",double(now-at)/1000);
            }else ImGui::TextWrapped("Availability unknown. This is a cached reference or the native node has no fresh observation. Enable loaded resources and nearby-node observation to read visible nodes; a reference marker is not proof that a resource remains available.");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Wiki reference")){
            const auto wiki=item.value("WikiName",name);const auto a=ImGui::GetCursorScreenPos();if(HelpyWikiIcons::Draw(wiki,a,{a.x+t.X(64),a.y+t.Y(64)}))ImGui::Dummy({t.X(64),t.Y(64)});
            const auto journal=HelpyWikiIcons::Journal(wiki);const auto note=item.value("Note",std::string{});
            ImGui::TextWrapped("%s",!journal.empty()?journal.c_str():!note.empty()?note.c_str():"No offline journal/reference entry is cached yet. Download wiki pictures & journals in Map filters.");
            auto url=HelpyWikiIcons::Page(wiki);if(url.empty())url=WikiUrl(wiki);ImGui::TextLinkOpenURL("Open source on the Dragonwilds Wiki",url.c_str());
            ImGui::TextWrapped("Artwork / journal media: copyright Jagex Ltd, via the Dragonwilds Wiki. Wiki text: CC BY-NC-SA 3.0; not RuneSchema MIT.");
            ImGui::TextWrapped("%s",MapFilters::Credit);ImGui::TextLinkOpenURL("Map data source",MapFilters::SourceUrl);
            ImGui::TextWrapped("Wiki references are informational, not live node or respawn state.");ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    ImGui::EndPopup();
}
inline void Render(){
    if(!Data.NativeFiltersRestored&&HelpyMapCache::Current().Ready){const auto& filters=HelpyMapCache::Current().Filters;Data.ShowPlayers=filters.Players;Data.ShowLodestones=filters.Lodestones;Data.ShowPlaced=filters.Placements;Data.ShowResources=filters.Resources;Data.ShowAI=filters.AI;Data.ShowChests=filters.Chests;Data.NativeFiltersRestored=true;}
    Poll();SpawnToolsPanel::RefreshSnapshot(-1);const auto& t=HelpyImGuiTheme::Current;
    const auto inlineIfRoom=[](float needed){const auto right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
        if(right-ImGui::GetItemRectMax().x>needed+ImGui::GetStyle().ItemSpacing.x)ImGui::SameLine();};
    if(HelpyImGuiTheme::SoftButton("Fit world")){Data.FitRequested=true;Data.Center={0.5,0.5};}ImGui::SameLine();
    auto& mapCache=HelpyMapCache::Current();
    const auto refreshDetails=std::string("Refresh loaded world markers and player positions.\nNative markers: ")+(Data.Snapshot.value("MarkersScanning",false)?"checking loaded areas":"current snapshot")+"\n"+mapCache.Status+"\nInstalled game: "+HelpyEnvironment::GameVersion+"\nOnline filter data changes only after a confirmed installed game build change. Cached locations remain visible. No world or save edits.";
    if(HelpyLoadingIndicator::RefreshButton("Refresh",Data.Pending||Data.Snapshot.value("MarkersScanning",false)||HelpyMapCache::Busy(),refreshDetails)){Data.Next={};Data.Error.clear();if(HelpyMapCache::RefreshEligible())HelpyMapCache::Refresh();}
    inlineIfRoom(t.X(160));ImGui::SetNextItemWidth(t.X(120));ImGui::SliderFloat("Zoom",&Data.Zoom,static_cast<float>(Data.View.FitZoom()),32,"%.1fx",ImGuiSliderFlags_Logarithmic);
    inlineIfRoom(t.X(80));if(ImGui::Checkbox("Players",&Data.ShowPlayers))SaveNativeFilters();
    inlineIfRoom(t.X(70));ImGui::Checkbox("Party",&Data.ShowParty);
    inlineIfRoom(t.X(90));if(ImGui::Checkbox("Chests",&Data.ShowChests)){SaveNativeFilters();Data.Next={};}
    inlineIfRoom(t.X(70));if(HelpyImGuiTheme::SoftButton(Data.BuildingPick?"Cancel build":"Build...")){Data.BuildingPick=!Data.BuildingPick;if(Data.BuildingPick){Data.Operation=1;Data.Permanent=true;Data.Status="Click the map to choose a building or prefab position.";}}
    const char* hint="LMB select · drag to pan · RMB options · scroll to zoom";inlineIfRoom(ImGui::CalcTextSize(hint).x);ImGui::TextDisabled("%s",hint);
    Filters();ImGui::SameLine();ImGui::SetNextItemWidth(std::min(t.X(300),ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##map-marker-search","Find iron, oak, chest, lore...",Data.MarkerSearch,sizeof(Data.MarkerSearch));
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapMarkerSearch=HelpyPreviewHooks::LastRect();
#endif
    if(Data.Snapshot.value("MarkersScanning",false))ImGui::TextDisabled("Discovering loaded map markers...");
    if(Data.HasPoint)ImGui::TextDisabled("Selected: X %.0f   Y %.0f",Data.Point.X,Data.Point.Y);
    else ImGui::TextDisabled("Select a map location or party member.");
    if(!Data.PartyPicking.empty()){ImGui::TextWrapped("Select the member's destination with LMB. Dragging still pans; no teleport occurs before confirmation.");ImGui::SameLine();if(ImGui::SmallButton("Cancel destination"))Data.PartyPicking.clear();}
    if(!Data.Error.empty())ImGui::TextWrapped("%s",Data.Error.c_str());
    if(Data.SpawnPending&&Data.TravelAction){
        const auto& snapshot=SpawnToolsPanel::UiSnapshot;
        const bool ours=snapshot.value("_RequestId",uint64_t{})==Data.SpawnPending;
        const auto progress=ours?snapshot.value("MapTravel",Json::object()):Json::object();
        const auto phase=progress.value("Phase",std::string("Queued"));
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(Clock::now()-Data.ActionRequested).count();
        const auto label=phase+" · "+std::to_string(seconds)+"s";
        // Indeterminate: native streaming does not expose a truthful percentage.
        ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),ImVec2(-1,t.Y(8)),"");
        ImGui::TextUnformatted(label.c_str());
        if(!progress.value("Committed",false)){
            ImGui::SameLine();ImGui::BeginDisabled(Data.CancelSent);
            if(ImGui::SmallButton(Data.CancelSent?"Cancelling...":"Cancel travel")){SpawnToolRequests::Cancel();Data.CancelSent=true;}
            ImGui::EndDisabled();
        }
        if(ours)Data.Status=ItemPresentation::Text(snapshot,"Status");
    }
    if(Data.BuildingAction&&(Data.SpawnPending||!Data.PlacementResult.empty())){
        const bool active=Data.PlacementResult.value("Active",false),saved=Data.PlacementResult.value("Permanent",false)&&Data.PlacementResult.contains("File");
        ImGui::PushStyleColor(ImGuiCol_Text,Data.SpawnPending?t.info:active?t.success:saved?t.warning:t.error);
        ImGui::TextWrapped("%s",Data.SpawnPending?"Placing building... waiting for the runtime result.":active?"Building placement confirmed.":saved?"Placement saved, but not active yet. Do not place a duplicate.":"Building placement failed.");ImGui::PopStyleColor();
        if(Data.PlacementResult.contains("Error"))ImGui::TextWrapped("%s",ItemPresentation::Text(Data.PlacementResult,"Error").c_str());
        if(Data.PlacementResult.contains("Location"))ImGui::TextWrapped("Position: %s",Data.PlacementResult["Location"].dump().c_str());
        if(saved)ImGui::TextWrapped("Saved placement: %s",ItemPresentation::Text(Data.PlacementResult,"File").c_str());
    }
    if(!Data.Status.empty())ImGui::TextWrapped("%s",Data.Status.c_str());
    const auto available=ImGui::GetContentRegionAvail();
    const bool dockParty=Data.ShowParty&&!Data.PartyDetached&&!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Party);
    const float partyHeight=dockParty?std::clamp(available.y*.36f,t.Y(180),t.Y(240))+ImGui::GetStyle().ItemSpacing.y:0;
    const float width=std::max(t.X(160),available.x),height=std::max(t.Y(160),available.y-t.Y(24)-partyHeight);
    const auto min=ImGui::GetCursorScreenPos();const ImVec2 max(min.x+width,min.y+height);
    Data.View={width,height,Data.Projection.Valid()?Data.Projection.HalfWidth/Data.Projection.HalfHeight:1,Data.Zoom,Data.Center};
    if(Data.FitRequested){Data.View.Zoom=Data.View.FitZoom();Data.FitRequested=false;}Data.View.Clamp();
    ImGui::InvisibleButton("##world-map",ImVec2(width,height),ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
    const bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
    if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))Data.Dragged=false;
    if(hovered&&io.MouseWheel!=0)Data.View.ZoomAt(Data.View.Zoom*std::pow(1.2,io.MouseWheel),{io.MousePos.x-min.x,io.MousePos.y-min.y});
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){Data.Dragged=true;Data.View.Pan({io.MouseDelta.x,io.MouseDelta.y});}
    Data.Zoom=static_cast<float>(Data.View.Zoom);Data.Center=Data.View.Center;
    const auto span=Data.View.Span();const ImVec2 uvMin(static_cast<float>(Data.Center.X-span.X/2),static_cast<float>(Data.Center.Y-span.Y/2)),uvMax(static_cast<float>(Data.Center.X+span.X/2),static_cast<float>(Data.Center.Y+span.Y/2));
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(min,max,ImGui::GetColorU32(t.panelAlt));
    const bool imageReady=Data.Texture&&Data.Texture->Status==ImTextureStatus_OK&&Data.Projection.Valid();
    draw->PushClipRect(min,max,true);
    if(imageReady){
        const MapProjection::Point a{std::max(0.0f,uvMin.x),std::max(0.0f,uvMin.y)},b{std::min(1.0f,uvMax.x),std::min(1.0f,uvMax.y)};
        const auto start=Data.View.ToPixel(a),end=Data.View.ToPixel(b);
        draw->AddImage(Data.Texture->GetTexRef(),{min.x+static_cast<float>(start.X),min.y+static_cast<float>(start.Y)},
            {min.x+static_cast<float>(end.X),min.y+static_cast<float>(end.Y)}, {static_cast<float>(a.X),static_cast<float>(a.Y)},{static_cast<float>(b.X),static_cast<float>(b.Y)});
    }
    else draw->AddText(ImVec2(min.x+t.X(14),min.y+t.Y(14)),ImGui::GetColorU32(t.textDisabled),SpawnToolRequests::IsAvailable()?"Loading the native world map...":"Load a world to view the map.");
    if(RiskOverlayActive()&&Data.Projection.Valid()){
        // Unchecked space is yellow. Colored points represent recent measured
        // footprints, not a fabricated whole-map safety classification.
        draw->AddRectFilled(min,max,IM_COL32(235,185,45,24));const auto [player,guid]=RiskIdentity();const auto now=PartyMemberView::Now();
        if(imageReady)for(const auto& run:Data.Ocean.Runs){
            const auto a=Data.View.ToPixel({double(run.Left)/MapOceanMask::Mask::Side,double(run.Y)/MapOceanMask::Mask::Side});
            const auto b=Data.View.ToPixel({double(run.Right)/MapOceanMask::Mask::Side,double(run.Y+1)/MapOceanMask::Mask::Side});
            const ImVec2 lo{std::max(min.x,min.x+float(a.X)),std::max(min.y,min.y+float(a.Y))},hi{std::min(max.x,min.x+float(b.X)),std::min(max.y,min.y+float(b.Y))};
            if(hi.x>lo.x&&hi.y>lo.y)draw->AddRectFilled(lo,hi,IM_COL32(225,45,40,115));
        }
        for(const auto& sample:Data.RiskSamples){if(sample.Player!=player||sample.Guid!=guid||guid.empty())continue;const auto risk=MapTeleportRisk::Read(sample.Observation,now);if(risk==MapTeleportRisk::Level::Unknown)continue;
            const auto pixel=Data.View.ToPixel(Data.Projection.ToUV(sample.Point));const ImVec2 at{min.x+static_cast<float>(pixel.X),min.y+static_cast<float>(pixel.Y)};
            if(at.x<min.x||at.x>max.x||at.y<min.y||at.y>max.y)continue;const auto uv=Data.Projection.ToUV(sample.Point);const auto color=risk==MapTeleportRisk::Level::Lower&&!Data.Ocean.Contains(uv.X,uv.Y)?IM_COL32(60,210,100,155):IM_COL32(235,65,55,180);
            draw->AddCircleFilled(at,t.X(8),color);draw->AddCircle(at,t.X(9),color,16,t.X(1));
        }
        const ImVec2 label{min.x+t.X(12),min.y+t.Y(10)};const char* text="Teleport advice: red ocean / landing issue; yellow unknown; green checked. Attempts remain allowed.";
        draw->AddRectFilled({label.x-t.X(5),label.y-t.Y(4)},{std::min(max.x,label.x+ImGui::CalcTextSize(text).x+t.X(5)),label.y+ImGui::GetTextLineHeight()+t.Y(4)},IM_COL32(10,10,10,205),t.X(3));
        const ImVec4 clip{min.x,min.y,max.x,max.y};draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),label,ImGui::GetColorU32(t.text),text,nullptr,0,&clip);
    }
    const Json* hoveredPlayer=nullptr;float playerDistance=t.X(11)*t.X(11);
    if(Data.ShowPlayers&&Data.Projection.Valid()&&Data.Snapshot.contains("Players")&&Data.Snapshot["Players"].is_array())for(const auto& player:Data.Snapshot["Players"]){
        if(!player.is_object()||!player.contains("Location")||!player["Location"].is_array()||player["Location"].size()!=3)continue;
        const auto& location=player["Location"];if(!location[0].is_number()||!location[1].is_number())continue;
        const MapProjection::Point xy{location[0].get<double>(),location[1].get<double>()};if(!Data.Projection.Contains(xy))continue;const auto uv=Data.Projection.ToUV(xy);
        if(uv.X<uvMin.x||uv.X>uvMax.x||uv.Y<uvMin.y||uv.Y>uvMax.y)continue;
        const auto pixel=Data.View.ToPixel(uv);const ImVec2 point(min.x+static_cast<float>(pixel.X),min.y+static_cast<float>(pixel.Y));
        const auto color=PartyMemberView::NativeColor(player);const auto tint=color?ImGui::GetColorU32(*color):IM_COL32_WHITE;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapPlayerTints[ItemPresentation::Text(player,"Path")]=tint;
#endif
        const bool portrait=PlayerPortraitPicker::DrawCached(PlayerPortraitPicker::Icon(player),{point.x-t.X(12),point.y-t.Y(12)},{point.x+t.X(12),point.y+t.Y(12)},tint);
        if(!portrait)draw->AddCircleFilled(point,t.X(6),color?tint:ImGui::GetColorU32(t.accentHover));draw->AddCircle(point,t.X(portrait?13:7),tint);
        const float distance=(io.MousePos.x-point.x)*(io.MousePos.x-point.x)+(io.MousePos.y-point.y)*(io.MousePos.y-point.y);
        if(hovered&&distance<playerDistance){playerDistance=distance;hoveredPlayer=&player;}
    }
    const Json* hoveredMarker=nullptr;float nearest=t.X(11)*t.X(11);
    const auto screen=[&](MapProjection::Point xy){const auto pixel=Data.View.ToPixel(Data.Projection.ToUV(xy));return ImVec2(min.x+static_cast<float>(pixel.X),min.y+static_cast<float>(pixel.Y));};
    const auto query=MapFilters::Fold(Data.MarkerSearch);
    const MapFilters::Marker* hoveredReference=nullptr;size_t referenceCount=0;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapReferenceMatches=0;HelpyPreviewHooks::MapReferenceClusters=0;
#endif
    if(mapCache.Data&&mapCache.Filters.ShowCached&&Data.Projection.Valid()){
        if(Data.FilteredData!=mapCache.Data.get()||Data.FilterRevision!=mapCache.Revision||Data.FilteredQuery!=query){
            Data.FilteredData=mapCache.Data.get();Data.FilterRevision=mapCache.Revision;Data.FilteredQuery=query;Data.FilteredMarkers.clear();
            for(size_t i=0;i<mapCache.Data->Markers.size();++i){const auto& marker=mapCache.Data->Markers[i];if(mapCache.Filters.Shows(mapCache.Data->Categories[marker.Category])&&MapFilters::MatchesFolded(marker.Search,query))Data.FilteredMarkers.push_back(i);}
        }
        // Screen-space grouping bounds draw work while retaining every matching
        // location in the filter index. Zooming separates nearby references.
        struct Cluster{size_t Index=0,Count=0;ImVec2 At;bool Mixed=false;};std::map<std::pair<int,int>,Cluster> clusters;
        const float cell=std::max(t.X(18),std::sqrt(width*height/1600.f));
        for(const auto index:Data.FilteredMarkers){const auto& marker=mapCache.Data->Markers[index];if(!Data.Projection.Contains({marker.X,marker.Y}))continue;const auto at=screen({marker.X,marker.Y});if(at.x<min.x||at.x>max.x||at.y<min.y||at.y>max.y)continue;
            auto& group=clusters[{static_cast<int>((at.x-min.x)/cell),static_cast<int>((at.y-min.y)/cell)}];if(!group.Count){group.Index=index;group.At=at;}else if(mapCache.Data->Markers[group.Index].Category!=marker.Category)group.Mixed=true;++group.Count;
        }
        for(const auto& [key,group]:clusters){const auto& marker=mapCache.Data->Markers[group.Index];const auto& category=mapCache.Data->Categories[marker.Category];
            MapLayerIcons::Resource(HelpyGlyphs::Resource(category.Group,MapFilters::Fold(category.Name)),category.Name,group.At,t.X(18),group.Mixed);
            if(group.Count>1){const ImVec2 badge{group.At.x+t.X(6),group.At.y-t.Y(6)};draw->AddCircleFilled(badge,t.X(3),ImGui::GetColorU32(t.accent));}
            const float distance=(io.MousePos.x-group.At.x)*(io.MousePos.x-group.At.x)+(io.MousePos.y-group.At.y)*(io.MousePos.y-group.At.y);
            if(hovered&&distance<nearest){nearest=distance;hoveredReference=&marker;referenceCount=group.Count;}
        }
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapReferenceMatches=Data.FilteredMarkers.size();HelpyPreviewHooks::MapReferenceClusters=clusters.size();
#endif
    }
    if(Data.Projection.Valid()&&Data.Snapshot.contains("Markers"))for(const auto& marker:Data.Snapshot["Markers"]){
        const auto kind=ItemPresentation::Text(marker,"Kind");const bool placed=marker.value("Managed",false);
        if(!(kind=="Chest"?Data.ShowChests:kind=="Lodestone"?Data.ShowLodestones:placed?Data.ShowPlaced:kind=="Resource"?Data.ShowResources:Data.ShowAI))continue;
        if(!MapFilters::MatchesFolded(MapFilters::Fold(ItemPresentation::Text(marker,"Name")+" "+kind),query))continue;
        const auto& loc=marker.at("Location");const MapProjection::Point xy{loc[0].get<double>(),loc[1].get<double>()};if(!Data.Projection.Contains(xy))continue;
        const auto at=screen(xy);if(at.x<min.x||at.x>max.x||at.y<min.y||at.y>max.y)continue;
        const float r=t.X(kind=="Lodestone"?7:4);const auto color=ImGui::GetColorU32(placed?t.accentHover:kind=="Lodestone"?ImVec4(0.55f,0.8f,1,1):kind=="Resource"?ImVec4(0.55f,0.78f,0.57f,1):ImVec4(0.9f,0.5f,0.45f,1));
        draw->AddCircleFilled(at,r+2,IM_COL32(8,8,8,230));
        if(kind=="Lodestone"){draw->AddQuadFilled({at.x,at.y-r},{at.x+r,at.y},{at.x,at.y+r},{at.x-r,at.y},color);draw->AddCircle(at,r+3,color);}
        else if(kind=="AI")draw->AddTriangleFilled({at.x,at.y-r},{at.x+r,at.y+r},{at.x-r,at.y+r},color);
        else if(kind=="Chest")MapLayerIcons::Draw("chests",at,t.Y(20));else if(kind=="Resource")draw->AddCircleFilled(at,r,color);else draw->AddRectFilled({at.x-r,at.y-r},{at.x+r,at.y+r},color);
        const float distance=(io.MousePos.x-at.x)*(io.MousePos.x-at.x)+(io.MousePos.y-at.y)*(io.MousePos.y-at.y);
        if(hovered&&distance<nearest){nearest=distance;hoveredMarker=&marker;hoveredReference=nullptr;}
#if defined(HELPY_PREVIEW)
        if(kind=="Lodestone")HelpyPreviewHooks::MapLodestone={at.x-r,at.y-r,at.x+r,at.y+r};
        if(kind=="Chest")HelpyPreviewHooks::MapChest={at.x-t.X(10),at.y-t.Y(10),at.x+t.X(10),at.y+t.Y(10)};
#endif
    }
    if(Data.HasPoint&&Data.Projection.Valid()){
        const auto at=screen(Data.Point);const float r=t.X(9);const auto gold=ImGui::GetColorU32(t.accentHover);
        draw->AddCircleFilled(at,r+3,IM_COL32(0,0,0,210));draw->AddCircle(at,r,gold,24,2);
        draw->AddLine({at.x-r-5,at.y},{at.x+r+5,at.y},gold,2);draw->AddLine({at.x,at.y-r-5},{at.x,at.y+r+5},gold,2);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MapSelected={at.x-r,at.y-r,at.x+r,at.y+r};
#endif
    }
    const auto hoverUV=Data.View.ToUV({io.MousePos.x-min.x,io.MousePos.y-min.y});
    if(hovered&&imageReady&&MapProjection::Viewport::InMap(hoverUV)){
        const auto point=Data.Projection.ToWorld(hoverUV);const auto now=Clock::now();
        if(!Data.HoverActive||Data.HoverPoint.X!=point.X||Data.HoverPoint.Y!=point.Y){Data.HoverActive=true;Data.HoverPoint=point;Data.HoverStableAt=now+std::chrono::milliseconds(120);Data.HoverGround=Json::object();if(!Data.GroundReadSelected)Data.GroundReadPending=0;}
        if(!Data.Popup&&!Data.Dragged&&now>=Data.HoverStableAt&&!Data.GroundReadPending&&(Data.HoverGround.empty()||now>=Data.HoverFreshUntil))ReadGround(point,false);
        const auto& ground=Data.HoverGround;
        const bool haveZ=now<Data.HoverFreshUntil&&ground.value("GroundAvailable",false)&&ground.contains("Location")&&ground["Location"].is_array()&&ground["Location"].size()==3&&ground["Location"][2].is_number();
        char label[160];if(haveZ)std::snprintf(label,sizeof(label),"X %.0f   Y %.0f   Z %.0f (ground)",point.X,point.Y,ground["Location"][2].get<double>());
        else std::snprintf(label,sizeof(label),"X %.0f   Y %.0f   Z: %s",point.X,point.Y,Data.GroundReadPending?"tracing...":ground.empty()?"pause to trace":"ground unavailable");
        draw->AddRectFilled({min.x+4,max.y-t.Y(32)},{max.x-4,max.y-4},IM_COL32(0,0,0,220));draw->AddText(ImVec2(min.x+t.X(10),max.y-t.Y(24)),ImGui::GetColorU32(t.accentHover),label);
        const bool context=ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        if(context||(ImGui::IsMouseReleased(ImGuiMouseButton_Left)&&!Data.Dragged)){
            const bool resourceClick=!context&&!Data.BuildingPick&&Data.PartyPicking.empty()&&(!hoveredPlayer||playerDistance>nearest);
            if(resourceClick&&hoveredMarker&&ItemPresentation::Text(*hoveredMarker,"Kind")=="Chest")PlayerInventoryPanel::RequestContainer(*hoveredMarker);
            if(resourceClick&&hoveredReference&&referenceCount==1){const auto& category=mapCache.Data->Categories[hoveredReference->Category];Data.ResourceDetails={{"Name",hoveredReference->Name},{"WikiName",category.Name},{"Note",hoveredReference->Note},{"X",hoveredReference->X},{"Y",hoveredReference->Y}};Data.ResourceDetailsWanted=true;}
            if(resourceClick&&hoveredMarker&&ItemPresentation::Text(*hoveredMarker,"Kind")=="Resource"){Data.ResourceDetails=*hoveredMarker;Data.ResourceDetails["X"]=(*hoveredMarker)["Location"][0];Data.ResourceDetails["Y"]=(*hoveredMarker)["Location"][1];Data.ResourceDetailsWanted=true;}
            Data.Point=point;Data.NearMarker=false;Data.SelectionName.clear();
            if(hoveredReference&&referenceCount==1){Data.Point={hoveredReference->X,hoveredReference->Y};Data.SelectionName=hoveredReference->Name+" (cached reference)";}
            if(hoveredMarker){const auto& loc=hoveredMarker->at("Location");Data.Point={loc[0].get<double>(),loc[1].get<double>()};Data.SelectionName=ItemPresentation::Text(*hoveredMarker,"Name");if(ItemPresentation::Text(*hoveredMarker,"Kind")=="Lodestone"){Data.Operation=2;Data.NearMarker=true;}}
            if(hoveredPlayer&&playerDistance<=nearest){const auto& loc=hoveredPlayer->at("Location");Data.Point={loc[0].get<double>(),loc[1].get<double>()};Data.SelectionName=RuneSchemaProfile::Name(*hoveredPlayer);Data.PartySelection=ItemPresentation::Text(*hoveredPlayer,"Path");Data.NearMarker=false;}
            const bool samePoint=haveZ&&Data.Point.X==Data.HoverPoint.X&&Data.Point.Y==Data.HoverPoint.Y;
            Data.HasPoint=true;Data.Ground=samePoint?Data.HoverGround:Json::object();Data.GroundReadPending=0;Data.GroundReadSelected=true;Data.NextGroundRead={};Data.ProbeWanted=Data.Operation!=2&&context&&!samePoint;
            if(!context&&!Data.PartyPicking.empty()){Data.PartyMapPoint=Data.Point;Data.PartyPicking.clear();Data.PartyConfirmWanted=true;Data.ProbeWanted=false;}
            if(context||Data.BuildingPick){Data.ProbeWanted=Data.Operation!=2&&!samePoint;Data.MapPopupAnchor=io.MousePos;Data.Popup=true;Data.BuildingPick=false;ImGui::OpenPopup("##map-place");}
        }
    }else{Data.HoverActive=false;Data.HoverGround=Json::object();if(!Data.GroundReadSelected)Data.GroundReadPending=0;}
    if(hoveredMarker){const auto label=ItemPresentation::Text(*hoveredMarker,"Name")+(ItemPresentation::Text(*hoveredMarker,"Kind")=="Lodestone"?" · RMB for travel options":"");draw->AddRectFilled({min.x+4,min.y+4},{max.x-4,min.y+t.Y(30)},IM_COL32(0,0,0,225));draw->AddText({min.x+t.X(10),min.y+t.Y(8)},ImGui::GetColorU32(t.text),label.c_str());
        if(hoveredMarker->contains("ResourceObservation")&&!Data.Popup){const auto& node=hoveredMarker->at("ResourceObservation");const auto at=node.value("ObservedUnixMs",int64_t{}),now=PartyMemberView::Now();
            const auto& location=hoveredMarker->at("Location");const auto markerAt=screen({location[0].get<double>(),location[1].get<double>()});const float radius=t.X(8);
            HelpyImGuiTheme::BeginDetailsTooltip({markerAt.x-radius,markerAt.y-radius},{markerAt.x+radius,markerAt.y+radius});PS::HelpyImGuiTheme::DetailsTextUnformatted(label.c_str());
            if(at>0&&now>=at&&now-at<=10000){if(node.contains("Quantity"))PS::HelpyImGuiTheme::DetailsText("%s: %d / %d",ItemPresentation::Text(node,"State").c_str(),node.value("Quantity",0),node.value("Maximum",0));PS::HelpyImGuiTheme::DetailsText("%s",ItemPresentation::Text(node,"TimerReason").c_str());}
            else PS::HelpyImGuiTheme::DetailsText("Observation expired; waiting for a nearby loaded node check.");HelpyImGuiTheme::EndDetailsTooltip();}
    }
    else if(hoveredReference){const auto& category=mapCache.Data->Categories[hoveredReference->Category];const auto label=hoveredReference->Name+" · "+category.Name+" · cached reference"+(referenceCount>1?" ("+std::to_string(referenceCount)+" nearby; zoom in)":"");draw->AddRectFilled({min.x+4,min.y+4},{max.x-4,min.y+t.Y(30)},IM_COL32(0,0,0,225));draw->AddText({min.x+t.X(10),min.y+t.Y(8)},ImGui::GetColorU32(t.text),label.c_str());}
    draw->PopClipRect();draw->AddRect(min,max,ImGui::GetColorU32(t.border));PlacePopup();
    if(mapCache.Data&&mapCache.Filters.ShowCached)ImGui::TextLinkOpenURL("Wiki / Wildsforge map data - CC BY-NC-SA 3.0",MapFilters::SourceUrl);
    Party(min,max);ResourceDetails();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Map={min.x,min.y,max.x,max.y};
#endif
}
}
