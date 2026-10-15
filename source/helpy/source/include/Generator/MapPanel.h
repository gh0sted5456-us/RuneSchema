#pragma once
#include "Core/MapProjection.h"
#include "Core/MapSpawnRequest.h"
#include "Core/SpawnCatalogSearch.h"
#include "Generator/SpawnToolsPanel.h"
#include <fstream>
#include <chrono>
namespace PS::MapPanel {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct State {
    Json Snapshot=Json::object(),Definition=Json::object(),Building=Json::object(),Ground=Json::object(),Buildings=Json::array();
    MapProjection::Projection Projection;
    MapProjection::Point Center{0.5,0.5},Point{},ProbePoint{};
    float Zoom=1,Radius=300,Yaw=0;int Kind=2,Count=1,Operation=0,SpawnType=0;
    bool Permanent=false,Popup=false,ShowPlayers=true,HasPoint=false,Dragged=false,ProbeWanted=false,BuildingsWanted=false,BuildingsMore=false;
    bool NearMarker=false;
    bool LiveSearch=false;std::string TypedSearch;
    bool ShowLodestones=true,ShowPlaced=true,ShowResources=false,ShowAI=false;
    size_t BuildingsOffset=0,BuildingsNext=0,BuildingsSentOffset=0;
    std::string BuildingsSentQuery,SelectionName;
    Clock::time_point SearchDue{},BuildingSearchDue{};bool SearchWanted=false;
    char BuildingSearch[257]{};
    char Search[257]{};std::string Error,Status,ImageFile;
    uint64_t Generation=0,Pending=0,SpawnPending=0,ProbePending=0,BuildingsPending=0,Sequence=0x7300000000000000ull;
    Clock::time_point Next{},Requested{},ActionRequested{},ProbeRequested{},BuildingsRequested{};
    ImTextureData* Texture=nullptr;std::vector<ImTextureData*> Retired;
};
inline State Data;
inline void Reap(){for(auto it=Data.Retired.begin();it!=Data.Retired.end();){auto* texture=*it;if(texture->Status!=ImTextureStatus_Destroyed){++it;continue;}ImGui::UnregisterUserTexture(texture);IM_DELETE(texture);it=Data.Retired.erase(it);}}
inline void LoadImage(const Json& image){
    const auto file=ItemPresentation::Text(image,"File");if(file.empty()||file==Data.ImageFile||ItemPresentation::Text(image,"Status")!="Ready")return;
    std::ifstream input(std::filesystem::path(file),std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Native map image is not accessible");
    const auto bytes=input.tellg();if(bytes<16||bytes>16+2048*2048*4)throw std::runtime_error("Native map image exceeds its bounds");
    input.seekg(0);char magic[4]{};uint32_t width=0,height=0,size=0;input.read(magic,4);input.read(reinterpret_cast<char*>(&width),4);input.read(reinterpret_cast<char*>(&height),4);input.read(reinterpret_cast<char*>(&size),4);
    if(!input||std::string(magic,4)!="RSI3"||width<1||height<1||width>2048||height>2048||size!=width*height*4||bytes!=static_cast<std::streamoff>(16+size))throw std::runtime_error("Native map image has an invalid format");
    std::vector<unsigned char> pixels(size);input.read(reinterpret_cast<char*>(pixels.data()),size);if(!input)throw std::runtime_error("Native map image is incomplete");
    auto* texture=IM_NEW(ImTextureData)();texture->Create(ImTextureFormat_RGBA32,width,height);std::memcpy(texture->GetPixels(),pixels.data(),size);texture->UseColors=true;texture->RefCount=1;texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(texture);
    if(Data.Texture){Data.Texture->SetStatus(ImTextureStatus_WantDestroy);Data.Retired.push_back(Data.Texture);}Data.Texture=texture;Data.ImageFile=file;
}
inline void Poll(){
    Reap();if(Data.Texture&&Data.Texture->Status==ImTextureStatus_OK&&Data.Texture->Pixels)Data.Texture->DestroyPixels();
    const auto generation=SpawnToolRequests::CurrentGeneration();const auto now=Clock::now();
    if(generation!=Data.Generation){Data.Generation=generation;Data.Pending=Data.SpawnPending=Data.ProbePending=Data.BuildingsPending=0;Data.Snapshot=Json::object();Data.Projection={};Data.Definition=Data.Building=Data.Ground=Json::object();Data.Buildings=Json::array();Data.ProbeWanted=Data.BuildingsWanted=Data.HasPoint=false;Data.Next={};Data.Popup=false;Data.Error.clear();Data.Status.clear();Data.Center={0.5,0.5};Data.Zoom=1;}
    Json receipt;
    if(Data.SpawnPending&&SpawnToolRequests::TakeCompleted(Data.SpawnPending,receipt)){Data.SpawnPending=0;Data.Status=ItemPresentation::Text(receipt,"Status");}
    if(Data.ProbePending&&SpawnToolRequests::TakeCompleted(Data.ProbePending,receipt)){Data.ProbePending=0;if(Data.Point.X==Data.ProbePoint.X&&Data.Point.Y==Data.ProbePoint.Y)Data.Ground=receipt.value("MapPoint",Json{{"GroundAvailable",false},{"Error",ItemPresentation::Text(receipt,"Status")}});}
    if(Data.BuildingsPending&&SpawnToolRequests::TakeCompleted(Data.BuildingsPending,receipt)){
        Data.BuildingsPending=0;
        if(Data.BuildingsSentQuery!=Data.BuildingSearch){}
        else if(receipt.contains("MapBuildings")&&receipt["MapBuildings"].is_object()){
            const auto& page=receipt["MapBuildings"];if(!Data.BuildingsSentOffset)Data.Buildings=Json::array();
            if(page.contains("Rows")&&page["Rows"].is_array())for(const auto& row:page["Rows"])if(row.is_object()&&Data.Buildings.size()<4096)Data.Buildings.push_back(row);
            Data.BuildingsNext=page.value("NextOffset",size_t{});Data.BuildingsMore=page.value("HasMore",false);
        }else Data.Error=ItemPresentation::Text(receipt,"Status");
    }
    if(Data.Pending&&SpawnToolRequests::TakeCompleted(Data.Pending,receipt)){
        Data.Pending=0;
        if(receipt.contains("MapSnapshot")&&receipt["MapSnapshot"].is_object()){
            Data.Snapshot=receipt["MapSnapshot"];Data.Projection=MapProjection::Projection::From(Data.Snapshot.value("Projection",Json::object()));
            try{const auto error=ItemPresentation::Text(Data.Snapshot,"Error");if(!error.empty())throw std::runtime_error(error);if(!Data.Projection.Valid())throw std::runtime_error("The game did not provide valid map alignment");LoadImage(Data.Snapshot.value("Image",Json::object()));Data.Error.clear();}
            catch(const std::exception& error){Data.Error=error.what();}
        }else Data.Error="Map data is unavailable. Install the matching RuneSchema Map bridge and load a world.";
    }
    if(Data.Pending&&now-Data.Requested>std::chrono::seconds(15)){Data.Pending=0;Data.Error="Map refresh timed out. Use Refresh to try again.";}
    if(Data.ProbePending&&now-Data.ProbeRequested>std::chrono::seconds(15)){Data.ProbePending=0;Data.Error="Coordinate lookup timed out. Select the point again.";}
    if(Data.BuildingsPending&&now-Data.BuildingsRequested>std::chrono::seconds(15)){Data.BuildingsPending=0;Data.Error="Building menu lookup timed out. Search again to retry.";}
    if(Data.SpawnPending&&now-Data.ActionRequested>std::chrono::seconds(30)){Data.SpawnPending=0;Data.Status="The action has not confirmed completion. Check the game before retrying.";}
    if(!SpawnToolRequests::IsAvailable()||SpawnToolRequests::IsWaiting()||Data.Pending||Data.SpawnPending||Data.ProbePending||Data.BuildingsPending)return;
    if(Data.ProbeWanted){const auto id=++Data.Sequence;if(SpawnToolRequests::TrySubmit({{"Action","MapProbe"},{"NearMarker",Data.NearMarker},{"_Source","HelpyMap"},{"_RequestId",id},{"TargetLocation",{{"X",Data.Point.X},{"Y",Data.Point.Y}}}})){Data.ProbePoint=Data.Point;Data.ProbePending=id;Data.ProbeRequested=now;Data.ProbeWanted=false;}return;}
    if(Data.BuildingsWanted&&now>=Data.BuildingSearchDue){const auto id=++Data.Sequence;if(SpawnToolRequests::TrySubmit({{"Action","MapBuildings"},{"_Source","HelpyMap"},{"_RequestId",id},{"Query",Data.BuildingSearch},{"Offset",Data.BuildingsOffset}})){Data.BuildingsPending=id;Data.BuildingsSentQuery=Data.BuildingSearch;Data.BuildingsSentOffset=Data.BuildingsOffset;Data.BuildingsRequested=now;Data.BuildingsWanted=false;}return;}
    if(!Data.Popup&&now>=Data.Next){
        const auto id=++Data.Sequence;Data.Next=now+std::chrono::milliseconds(750);
        if(SpawnToolRequests::TrySubmit({{"Action","MapSnapshot"},{"Lodestones",Data.ShowLodestones},{"Placements",Data.ShowPlaced},{"Resources",Data.ShowResources},{"AI",Data.ShowAI},{"_Source","HelpyMap"},{"_RequestId",id}})){Data.Pending=id;Data.Requested=now;}
    }
}
inline void PlacePopup(){
    if(!ImGui::BeginPopup("##map-place")){Data.Popup=false;return;}Data.Popup=true;const auto& t=HelpyImGuiTheme::Current;
    ImGui::TextUnformatted(Data.SelectionName.empty()?"Selected map point":Data.SelectionName.c_str());
    ImGui::Text("X %.0f   Y %.0f",Data.Point.X,Data.Point.Y);
    const bool groundReady=Data.Ground.value("GroundAvailable",false)&&Data.Ground.contains("Location")&&Data.Ground["Location"].is_array()&&Data.Ground["Location"].size()==3;
    if(groundReady){ImGui::Text("Z %.0f (ground)",Data.Ground["Location"][2].get<double>());ImGui::SameLine();if(ImGui::SmallButton("Copy coordinates")){char value[128];std::snprintf(value,sizeof(value),"X=%.0f Y=%.0f Z=%.0f",Data.Point.X,Data.Point.Y,Data.Ground["Location"][2].get<double>());ImGui::SetClipboardText(value);}}
    else ImGui::TextWrapped("%s",Data.ProbePending||Data.ProbeWanted?"Reading ground height...":ItemPresentation::Text(Data.Ground,"Error").c_str());
    ImGui::Separator();
    if(ImGui::RadioButton("Spawn",Data.Operation==0))Data.Operation=0;ImGui::SameLine();
    if(ImGui::RadioButton("Build",Data.Operation==1)){Data.Operation=1;if(Data.Buildings.empty()){Data.BuildingsOffset=0;Data.BuildingsWanted=true;}}ImGui::SameLine();
    if(ImGui::RadioButton("Teleport",Data.Operation==2))Data.Operation=2;
    if(Data.Operation==0){
    const char* types[]{"Resources", "Ore / mineral nodes", "Trees / wood", "Enemies / AI"};
    ImGui::SetNextItemWidth(t.X(340));if(ImGui::Combo("##spawn-type",&Data.SpawnType,types,4)){
        Data.Kind=Data.SpawnType==3?1:2;Data.Definition=Json::object();Data.SearchWanted=true;Data.SearchDue={};
    }
    SpawnToolsPanel::RefreshSnapshot(Data.Kind);auto& window=LazyCatalog::For(Data.Kind);
    if(!window.Started&&!window.Pending){Data.SearchWanted=true;Data.SearchDue={};}
    ImGui::SetNextItemWidth(t.X(340));const bool enter=ImGui::InputTextWithHint("##map-spawn-search","Iron ore node... (Enter to search)",Data.Search,sizeof(Data.Search),ImGuiInputTextFlags_EnterReturnsTrue);
    if(Data.TypedSearch!=Data.Search){Data.TypedSearch=Data.Search;Data.Definition=Json::object();Data.SearchWanted=Data.LiveSearch;Data.SearchDue=Clock::now()+std::chrono::milliseconds(400);}
    const bool search=ImGui::Button("Search");ImGui::SameLine();ImGui::Checkbox("Search as I type",&Data.LiveSearch);
    if(enter||search){Data.SearchWanted=true;Data.SearchDue={};}
    const auto query=std::string(Data.Search)+(Data.SpawnType==1?" ore node":Data.SpawnType==2?" tree":"");
    if(Data.SearchWanted&&Clock::now()>=Data.SearchDue){LazyCatalog::Search(Data.Kind,query);Data.SearchWanted=false;}
    ImGui::BeginChild("##spawn-results",ImVec2(t.X(340),t.Y(130)),true);
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
        ImGui::SetNextItemWidth(t.X(300));if(ImGui::InputTextWithHint("##map-building-search","Find building pieces / teleport stones...",Data.BuildingSearch,sizeof(Data.BuildingSearch))){Data.BuildingsOffset=0;Data.BuildingsWanted=true;Data.BuildingSearchDue=Clock::now()+std::chrono::milliseconds(220);Data.Building=Json::object();Data.Buildings=Json::array();}
        const auto label=ItemPresentation::Text(Data.Building,"Name");ImGui::SetNextItemWidth(t.X(300));
        if(ImGui::BeginCombo("##map-building",label.empty()?"Choose a building piece":label.c_str())){
            for(const auto& row:Data.Buildings){const auto path=ItemPresentation::Text(row,"Path");ImGui::PushID(path.c_str());if(ImGui::Selectable(ItemPresentation::Text(row,"Name").c_str(),path==ItemPresentation::Text(Data.Building,"Path")))Data.Building=row;ImGui::PopID();}
            if(Data.BuildingsMore&&!Data.BuildingsPending&&ImGui::Selectable("Load more...")){Data.BuildingsOffset=Data.BuildingsNext;Data.BuildingsWanted=true;}ImGui::EndCombo();
        }
        if(Data.BuildingsPending||Data.BuildingsWanted)ImGui::TextDisabled("Reading the game's building menu...");
        ImGui::SetNextItemWidth(t.X(140));ImGui::SliderFloat("Rotation",&Data.Yaw,0,360,"%.0f degrees");
        ImGui::TextWrapped("Pieces must pass the game's building checks. Placement uses one piece at a time.");
    }
    if(Data.Operation!=2){
    ImGui::Separator();if(ImGui::RadioButton("Temporary",!Data.Permanent))Data.Permanent=false;ImGui::SameLine();if(ImGui::RadioButton("Permanent",Data.Permanent))Data.Permanent=true;
    ImGui::TextDisabled("%s",Data.Permanent?"Saved as RuneSchema placements.":"Temporary placement for this session.");
    }
    if(Data.Operation==0){
    ImGui::SetNextItemWidth(t.X(140));ImGui::InputInt("Count",&Data.Count);Data.Count=std::clamp(Data.Count,1,20);
    if(Data.Count>1){ImGui::SetNextItemWidth(t.X(140));ImGui::InputFloat("Spread (cm)",&Data.Radius);Data.Radius=std::clamp(Data.Radius,0.0f,10000.0f);}
    }
    ImGui::TextUnformatted(Data.Operation==2?"Player to teleport":"Player context");SpawnToolsPanel::RenderCartRecipient(SpawnToolsPanel::UiSnapshot);
    if(Data.Operation==2)ImGui::TextWrapped("%s",Data.NearMarker?"Moves the player to verified ground beside this stone (within 2 metres).":"Moves the selected player to a clear landing point on this ground.");
    const bool choice=Data.Operation==2||(Data.Operation==1?!Data.Building.empty():!Data.Definition.empty());
    const bool authority=Data.Operation==0||Data.Snapshot.value("CanBuildAndTeleport",false);
    if(!authority)ImGui::TextWrapped("Use the host's Helpy for building placement and player teleportation.");
    const bool ready=authority&&groundReady&&Data.Projection.Valid()&&choice&&!SpawnToolsPanel::SelectedPlayer.empty()&&!SpawnToolRequests::IsWaiting()&&!Data.SpawnPending&&!Data.ProbePending&&!Data.ProbeWanted&&!Data.BuildingsPending;
    ImGui::BeginDisabled(!ready);
    const char* action=Data.Operation==2?"Teleport player here":Data.Operation==1?(Data.Permanent?"Place & save building":"Place building here"):(Data.Permanent?"Spawn & save placement":"Spawn here");
    if(HelpyImGuiTheme::SoftButton(action,ImVec2(-1,0))){try{
        const auto id=++Data.Sequence;auto request=Data.Operation==2?MapSpawnRequest::Target(Data.Projection,Data.Point,"MapTeleport",SpawnToolsPanel::SelectedPlayer,id):Data.Operation==1?MapSpawnRequest::Building(Data.Projection,Data.Point,Data.Building,SpawnToolsPanel::SelectedPlayer,Data.Permanent,Data.Yaw,id):MapSpawnRequest::Build(Data.Projection,Data.Point,Data.Definition,SpawnToolsPanel::SelectedPlayer,Data.Permanent,Data.Count,Data.Radius,id);
        request["NearMarker"]=Data.Operation==2&&Data.NearMarker;
        if(SpawnToolRequests::TrySubmit(std::move(request))){Data.SpawnPending=id;Data.ActionRequested=Clock::now();Data.Status="Applying action at the selected map position...";ImGui::CloseCurrentPopup();Data.Popup=false;}else Data.Error="The runtime is busy. Try again.";
    }catch(const std::exception& error){Data.Error=error.what();}}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::MapSpawn=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();ImGui::EndPopup();
}
inline void Render(){
    Poll();SpawnToolsPanel::RefreshSnapshot(-1);const auto& t=HelpyImGuiTheme::Current;
    if(HelpyImGuiTheme::SoftButton("Fit map")){Data.Zoom=1;Data.Center={0.5,0.5};}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Refresh")){Data.Next={};Data.Error.clear();}
    ImGui::SameLine();ImGui::Checkbox("Players",&Data.ShowPlayers);SpawnToolsPanel::SameLineIfRoom(360);ImGui::TextDisabled("Scroll to zoom · drag to pan · click for actions");
    if(ImGui::TreeNode("Map layers")){
        ImGui::Checkbox("Lodestones / teleport stones",&Data.ShowLodestones);ImGui::SameLine();ImGui::Checkbox("RuneSchema placements",&Data.ShowPlaced);
        ImGui::Checkbox("Vanilla resources",&Data.ShowResources);ImGui::SameLine();ImGui::Checkbox("AI spawn points",&Data.ShowAI);
        ImGui::TextWrapped("Live markers show loaded areas; saved RuneSchema placements also show their authored positions.");ImGui::TreePop();
    }
    if(Data.Snapshot.value("MarkersScanning",false))ImGui::TextDisabled("Discovering loaded map markers...");
    if(Data.HasPoint)ImGui::TextDisabled("Selected: X %.0f   Y %.0f",Data.Point.X,Data.Point.Y);
    if(!Data.Error.empty())ImGui::TextWrapped("%s",Data.Error.c_str());if(!Data.Status.empty())ImGui::TextWrapped("%s",Data.Status.c_str());
    const auto available=ImGui::GetContentRegionAvail();const float side=std::max(t.Y(160),std::min(available.x,available.y-t.Y(24)));
    const auto start=ImGui::GetCursorScreenPos();const ImVec2 min(start.x+(available.x-side)*0.5f,start.y),max(min.x+side,min.y+side);
    ImGui::SetCursorScreenPos(min);ImGui::InvisibleButton("##world-map",ImVec2(side,side),ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
    const bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
    if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))Data.Dragged=false;
    if(hovered&&io.MouseWheel!=0){Data.Zoom=std::clamp(Data.Zoom*std::pow(1.2f,io.MouseWheel),1.0f,8.0f);}
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){Data.Dragged=true;Data.Center.X-=io.MouseDelta.x/(side*Data.Zoom);Data.Center.Y-=io.MouseDelta.y/(side*Data.Zoom);}
    const double half=0.5/Data.Zoom;Data.Center.X=std::clamp(Data.Center.X,half,1-half);Data.Center.Y=std::clamp(Data.Center.Y,half,1-half);
    const ImVec2 uvMin(static_cast<float>(Data.Center.X-half),static_cast<float>(Data.Center.Y-half)),uvMax(static_cast<float>(Data.Center.X+half),static_cast<float>(Data.Center.Y+half));
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(min,max,ImGui::GetColorU32(t.panelAlt));
    const bool imageReady=Data.Texture&&Data.Texture->Status==ImTextureStatus_OK&&Data.Projection.Valid();
    draw->PushClipRect(min,max,true);
    if(imageReady)draw->AddImage(Data.Texture->GetTexRef(),min,max,uvMin,uvMax);
    else draw->AddText(ImVec2(min.x+t.X(14),min.y+t.Y(14)),ImGui::GetColorU32(t.textDisabled),SpawnToolRequests::IsAvailable()?"Loading the native world map...":"Load a world to view the map.");
    if(Data.ShowPlayers&&Data.Projection.Valid()&&Data.Snapshot.contains("Players")&&Data.Snapshot["Players"].is_array())for(const auto& player:Data.Snapshot["Players"]){
        if(!player.is_object()||!player.contains("Location")||!player["Location"].is_array()||player["Location"].size()!=3)continue;
        const auto& location=player["Location"];if(!location[0].is_number()||!location[1].is_number())continue;
        const MapProjection::Point xy{location[0].get<double>(),location[1].get<double>()};if(!Data.Projection.Contains(xy))continue;const auto uv=Data.Projection.ToUV(xy);
        if(uv.X<uvMin.x||uv.X>uvMax.x||uv.Y<uvMin.y||uv.Y>uvMax.y)continue;
        const ImVec2 point(min.x+static_cast<float>((uv.X-uvMin.x)*Data.Zoom)*side,min.y+static_cast<float>((uv.Y-uvMin.y)*Data.Zoom)*side);
        draw->AddCircleFilled(point,t.X(6),ImGui::GetColorU32(t.accentHover));draw->AddCircle(point,t.X(7),IM_COL32(255,255,255,255));const auto name=ItemPresentation::Text(player,"Name");draw->AddText(ImVec2(point.x+t.X(10),point.y-t.Y(7)),ImGui::GetColorU32(t.text),name.c_str());
    }
    const Json* hoveredMarker=nullptr;float nearest=t.X(11)*t.X(11);
    const auto screen=[&](MapProjection::Point xy){const auto uv=Data.Projection.ToUV(xy);return ImVec2(min.x+static_cast<float>((uv.X-uvMin.x)*Data.Zoom)*side,min.y+static_cast<float>((uv.Y-uvMin.y)*Data.Zoom)*side);};
    if(Data.Projection.Valid()&&Data.Snapshot.contains("Markers"))for(const auto& marker:Data.Snapshot["Markers"]){
        const auto kind=ItemPresentation::Text(marker,"Kind");const bool placed=marker.value("Managed",false);
        if(!(kind=="Lodestone"?Data.ShowLodestones:placed?Data.ShowPlaced:kind=="Resource"?Data.ShowResources:Data.ShowAI))continue;
        const auto& loc=marker.at("Location");const MapProjection::Point xy{loc[0].get<double>(),loc[1].get<double>()};if(!Data.Projection.Contains(xy))continue;
        const auto at=screen(xy);if(at.x<min.x||at.x>max.x||at.y<min.y||at.y>max.y)continue;
        const float r=t.X(kind=="Lodestone"?7:4);const auto color=ImGui::GetColorU32(placed?t.accentHover:kind=="Lodestone"?ImVec4(0.55f,0.8f,1,1):kind=="Resource"?ImVec4(0.55f,0.78f,0.57f,1):ImVec4(0.9f,0.5f,0.45f,1));
        draw->AddCircleFilled(at,r+2,IM_COL32(8,8,8,230));
        if(kind=="Lodestone"){draw->AddQuadFilled({at.x,at.y-r},{at.x+r,at.y},{at.x,at.y+r},{at.x-r,at.y},color);draw->AddCircle(at,r+3,color);}
        else if(kind=="AI")draw->AddTriangleFilled({at.x,at.y-r},{at.x+r,at.y+r},{at.x-r,at.y+r},color);
        else if(kind=="Resource")draw->AddCircleFilled(at,r,color);else draw->AddRectFilled({at.x-r,at.y-r},{at.x+r,at.y+r},color);
        const float distance=(io.MousePos.x-at.x)*(io.MousePos.x-at.x)+(io.MousePos.y-at.y)*(io.MousePos.y-at.y);
        if(hovered&&distance<nearest){nearest=distance;hoveredMarker=&marker;}
#if defined(HELPY_PREVIEW)
        if(kind=="Lodestone")HelpyPreviewHooks::MapLodestone={at.x-r,at.y-r,at.x+r,at.y+r};
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
    if(hovered&&imageReady){
        const MapProjection::Point uv{uvMin.x+(io.MousePos.x-min.x)/(side*Data.Zoom),uvMin.y+(io.MousePos.y-min.y)/(side*Data.Zoom)};const auto point=Data.Projection.ToWorld(uv);
        char label[96];std::snprintf(label,sizeof(label),"X %.0f   Y %.0f",point.X,point.Y);draw->AddText(ImVec2(min.x+t.X(10),max.y-t.Y(24)),ImGui::GetColorU32(t.accentHover),label);
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)||(ImGui::IsMouseReleased(ImGuiMouseButton_Left)&&!Data.Dragged)){
            Data.Point=point;Data.NearMarker=false;Data.SelectionName.clear();
            if(hoveredMarker){const auto& loc=hoveredMarker->at("Location");Data.Point={loc[0].get<double>(),loc[1].get<double>()};Data.SelectionName=ItemPresentation::Text(*hoveredMarker,"Name");if(ItemPresentation::Text(*hoveredMarker,"Kind")=="Lodestone"){Data.Operation=2;Data.NearMarker=true;}}
            Data.HasPoint=true;Data.Ground=Json::object();Data.ProbeWanted=true;Data.Popup=true;ImGui::OpenPopup("##map-place");
        }
    }
    if(hoveredMarker){const auto label=ItemPresentation::Text(*hoveredMarker,"Name")+(ItemPresentation::Text(*hoveredMarker,"Kind")=="Lodestone"?" · click to teleport":"");draw->AddRectFilled({min.x+4,min.y+4},{max.x-4,min.y+t.Y(30)},IM_COL32(0,0,0,225));draw->AddText({min.x+t.X(10),min.y+t.Y(8)},ImGui::GetColorU32(t.text),label.c_str());}
    draw->PopClipRect();draw->AddRect(min,max,ImGui::GetColorU32(t.border));PlacePopup();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Map={min.x,min.y,max.x,max.y};
#endif
}
}
