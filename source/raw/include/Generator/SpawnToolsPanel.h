#pragma once
#include "Generator/ToolRequest.h"
#include "Runtime/HelpySettings.h"
#include "Generator/ItemCloneRequest.h"
#include "Generator/HelpyImGuiTheme.h"
#include <cstdio>
#include <cstdint>
#include <chrono>
#include "Generator/ItemGridPicker.h"
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyNodes.h"
#include <algorithm>
#include <cctype>
#include <random>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
namespace PS::SpawnToolsPanel {
enum class View : unsigned char { All, Items, Ai, Resources, ItemLab, Buildings, Cleanup };
inline int CatalogCategory(View view) {
    if(view==View::All)return -1;
    if(view==View::Items||view==View::ItemLab)return 0;
    if(view==View::Ai)return 1;
    if(view==View::Resources)return 2;
    return -1;
}
inline bool SearchOnEnter(const char* label,const char* hint,char* buffer,size_t capacity,std::string& committed) {
    const auto& theme=HelpyImGuiTheme::Current;const float button=ImGui::GetFrameHeight(),spacing=ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(theme.X(140.0f),ImGui::GetContentRegionAvail().x-button-spacing));
    const bool enter=ImGui::InputTextWithHint(label,hint,buffer,capacity,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();const bool click=HelpyImGuiTheme::SearchIconButton(label);
    if(enter||click){committed=buffer;return true;}return false;
}
inline void SameLineIfRoom(float logicalWidth=150.0f) {if(ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(logicalWidth))ImGui::SameLine();}
struct MatchCache {uint64_t Revision=0;int Kind=-1;std::string Query;std::vector<size_t> Rows;};
struct ActorUiState {std::string Definition,Query,MetaKey,SelectedLabel="Select spawn";char ClassPath[1024]{},Name[257]{},Filter[257]{};MatchCache Matches;uint64_t MetaRevision=0;int Mode=0;bool Custom=false,Boss=false,Confirmed=false,GridView=false,ViewInitialized=false;float ResultPaneHeightLogical=-1.0f;};
inline std::string Lower(std::string value){std::ranges::transform(value,value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
inline void CacheDefinitions(const nlohmann::json& result,uint64_t revision,int kind,int mode,const std::string& query,MatchCache& cache) {
    const int cacheKind=kind*8+mode;if(cache.Revision==revision&&cache.Kind==cacheKind&&cache.Query==query)return;cache={revision,cacheKind,query,{}};
    if(!result.contains("Definitions")||!result["Definitions"].is_array())return;const auto filter=Lower(query);const auto& rows=result["Definitions"];
    cache.Rows.reserve(rows.size());for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto type=row.value("Type",std::string("AI"));
        if(kind==0&&((mode==0&&type!="NPC")||(mode==1&&type!="AI")))continue;
        if(kind==1){if(type!="Resource")continue;const auto rk=HelpyNodes::ResourceKind(row.value("Class",std::string{}),row.value("ResourceFamily",std::string{}));if((mode==0&&rk!=HelpyNodes::Kind::Tree)||(mode==1&&rk==HelpyNodes::Kind::Tree))continue;}
        const auto key=row.value("Key",std::string{}),title=row.value("Name",key),path=row.value("Class",std::string{}),modId=row.value("ModId",std::string{}),family=row.value("ResourceFamily",std::string{});
        if(filter.empty()||Lower(key+" "+type+" "+title+" "+path+" "+modId+" "+family).find(filter)!=std::string::npos)cache.Rows.push_back(i);}
}
inline void CacheBuildings(const nlohmann::json& result,uint64_t revision,const std::string& query,MatchCache& cache) {
    if(cache.Revision==revision&&cache.Query==query)return;cache={revision,-1,query,{}};
    if(!result.contains("Buildings")||!result["Buildings"].is_array())return;const auto filter=Lower(query);const auto& rows=result["Buildings"];
    cache.Rows.reserve(rows.size());for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto path=row.value("Path",std::string{}),title=row.value("Name",path);
        if(filter.empty()||Lower(path+" "+title).find(filter)!=std::string::npos)cache.Rows.push_back(i);}
}
inline nlohmann::json UiSnapshot={{"Status","Load a world before using runtime tools."}};
inline uint64_t UiRevision=0;inline int UiCategory=-99;
inline std::string SelectedPlayer;
inline uint64_t CartSequence=(uint64_t{1}<<62),CartGeneration=0;
inline void PollCartDelivery() {
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(CartGeneration!=generation) {
        if(ItemGridPicker::SharedCart.Delivery())ItemGridPicker::CartStatus="World changed during delivery. Cart retained; check your inventory before retrying.";
        ItemGridPicker::SharedCart.CancelDelivery();CartGeneration=generation;
    }
    nlohmann::json receipt;const auto request=ItemGridPicker::SharedCart.Delivery();
    if(request&&SpawnToolRequests::TakeCompleted(request,receipt)) {
        ItemGridPicker::SharedCart.CompleteDelivery(request,receipt);
        ItemGridPicker::CartStatus=receipt.value("Status",std::string("Delivery completed; inspect remaining items before retrying."));
    }
}
inline void RenderCartDock() {
    const auto& theme=HelpyImGuiTheme::Current;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
    if(ImGui::BeginChild("##item-cart-dock",ImVec2(0,theme.Y(244.0f)),true,ImGuiWindowFlags_AlwaysUseWindowPadding)){
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        if(!ItemGridPicker::SharedCart.Delivery()&&ImGui::BeginDragDropTargetCustom(ImRect(pos,ImVec2(pos.x+size.x,pos.y+size.y)),ImGui::GetID("cart-transfer"))){
            if(const auto* payload=ImGui::AcceptDragDropPayload("RS_ITEM_ROW")){
                if(payload->DataSize>0 && payload->DataSize<=8193 && static_cast<const char*>(payload->Data)[payload->DataSize-1]=='\0'){
                    try {ItemGridPicker::AddToCart(nlohmann::json::parse(static_cast<const char*>(payload->Data)));}catch(...){ItemGridPicker::CartStatus="This item payload could not be added.";}
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::TextUnformatted("Cart / staging chest");ImGui::SameLine();
        ImGui::TextDisabled("%zu / %zu",ItemGridPicker::SharedCart.Entries().size(),ItemCart::Model::Limit);
        ImGui::Separator();
        std::string removePath;
        const auto entries=ItemGridPicker::SharedCart.Entries();
        ImGui::BeginDisabled(ItemGridPicker::SharedCart.Delivery()!=0);
        if(ImGui::BeginChild("##cart-items",ImVec2(0,theme.Y(134.0f)),false)){
            if(entries.empty())ImGui::TextWrapped("Drag items from the inventory above, or right-click an item and choose Add to cart. Nothing is given until you confirm below.");
            const float spacing=ImGui::GetStyle().ItemSpacing.x,width=theme.X(118.0f);
            const int columns=std::max(1,static_cast<int>((ImGui::GetContentRegionAvail().x+spacing)/(width+spacing)));
            for(std::size_t index=0;index<entries.size();++index){
                const auto& entry=entries[index];const auto name=entry.Row.value("Name",entry.Path),icon=entry.Row.value("Icon",std::string{});
                ImGui::PushID(entry.Path.c_str());ImGui::BeginGroup();
                const auto iconMin=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##cart-slot",ImVec2(width,theme.Y(50.0f)));
                const ImVec2 imageMin(iconMin.x+(width-theme.Y(42.0f))*0.5f,iconMin.y),imageMax(imageMin.x+theme.Y(42.0f),imageMin.y+theme.Y(42.0f));
                if(icon.empty() || !ItemIconCache::Draw(icon,imageMin,imageMax,true))ItemGridPicker::DrawPlaceholder(imageMin,imageMax,theme);
                ItemGridPicker::DrawItemBadges(entry.Row,iconMin,ImVec2(iconMin.x+width,iconMin.y+theme.Y(50.0f)),theme);
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
                    ImGui::SetDragDropPayload("RS_CART_PATH",entry.Path.c_str(),entry.Path.size()+1);ImGui::TextUnformatted(name.c_str());ImGui::TextDisabled("Drop back into the inventory to remove.");ImGui::EndDragDropSource();
                }
                if(ImGui::BeginPopupContextItem("##cart-actions")){if(ImGui::MenuItem("Remove from cart"))removePath=entry.Path;ImGui::EndPopup();}
                ImGui::TextUnformatted(ItemGridPicker::Ellipsize(name,width).c_str());
                int quantity=entry.Quantity;ImGui::SetNextItemWidth(width);if(ImGui::InputInt("##quantity",&quantity,1,10))ItemGridPicker::SharedCart.SetQuantity(entry.Path,quantity);
                ImGui::EndGroup();ImGui::PopID();if((index+1)%columns && index+1<entries.size())ImGui::SameLine();
            }
        }ImGui::EndChild();
        if(!removePath.empty())ItemGridPicker::RemoveFromCart(removePath);
        ImGui::EndDisabled();
        if(SelectedPlayer.empty())ImGui::TextDisabled("Select a player first.");
        const bool blocked=SelectedPlayer.empty()||SpawnToolRequests::IsWaiting()||ItemGridPicker::SharedCart.Delivery()||ItemGridPicker::SharedCart.Entries().empty();
        const auto run=[&](const char* label,int mode){
            if(!ImGui::Button(label))return;
            auto items=ItemGridPicker::SharedCart.Payload(mode);if(items.empty())return;
            const auto request=++CartSequence;
            if(!ItemGridPicker::SharedCart.BeginDelivery(request,mode))return;
            if(!SpawnToolRequests::TrySubmit({{"_RequestId",request},{"Action","GiveItems"},{"_Source","HelpyStandaloneCart"},{"Player",SelectedPlayer},{"Items",std::move(items)}})) {
                ItemGridPicker::SharedCart.CancelDelivery();ItemGridPicker::CartStatus="Delivery was not submitted; cart retained.";
            } else ItemGridPicker::CartStatus="Delivering; confirmed items will leave the cart.";
        };
        ImGui::BeginDisabled(blocked);run("Deliver",0);
        if(theme.advancedMode){SameLineIfRoom(100);run("One each",1);SameLineIfRoom(110);run("Half stacks",2);SameLineIfRoom(110);run("Full stacks",3);}ImGui::EndDisabled();
        SameLineIfRoom(70);ImGui::BeginDisabled(ItemGridPicker::SharedCart.Delivery()!=0);if(ImGui::Button("Clear cart"))ItemGridPicker::ClearSharedCart();ImGui::EndDisabled();
        if(!ItemGridPicker::CartStatus.empty())ImGui::TextWrapped("%s",ItemGridPicker::CartStatus.c_str());
    }ImGui::EndChild();ImGui::PopStyleColor();
}
inline void RefreshSnapshot(int category,bool force=false) {
    if(category!=UiCategory){UiCategory=category;UiRevision=0;force=true;}
    const auto prior=UiRevision;SpawnToolRequests::ReadQuickIfChanged(UiRevision,category,UiSnapshot);
    if(force||UiRevision!=prior){LazyCatalog::Apply(UiSnapshot,UiRevision);LazyCatalog::ReleaseFailedWindow(UiRevision,UiSnapshot);LazyCatalog::Overlay(UiSnapshot,category);}
    else LazyCatalog::Overlay(UiSnapshot,category);
}
inline void Render(View view) {
    auto& player=SelectedPlayer;
    static char itemFilter[257]{},dropItemFilter[257]{},buildingFilter[257]{},selectedBuilding[1024]{};
    static std::string buildingQuery,cloneQuery;static MatchCache buildingMatches;static ActorUiState actors[2];
    static int count=1;static float distance=5,scale=1,yaw=0,height=0,radius=500,gridX=0,gridY=0,gridSize=100;static bool buildingConfirmed=false;
    static bool grid=false,overrideGhost=false,ghost=false,overrideDrops=false;
    static int dropMin=1,dropMax=1,npcDuration=HelpyNodes::DefaultDuration;static float chance=100;
    static char item[1024]{},selectedItem[1024]{},cloneName[257]{},cloneIcon[2049]{},cloneFilter[257]{},cloneEditor[65536]="{}";
    static bool cloneGive=true,cloneAck=false;
    static std::string cloneError,cloneStatus,cloneSoft,cloneFieldSource,cloneFieldQuery;static std::vector<size_t> cloneFieldMatches;
    static nlohmann::json cloneSnapshot=nlohmann::json::object();
    static uint64_t cloneSequence=(uint64_t{1}<<63),clonePending=0,seenGeneration=0;
    static int itemCount=1,itemAction=0,buildingTime=0;static bool rebuildCatalogs=false,cloneReady=false,allowDeconstruction=false;
    static ItemGridPicker::State itemPickerState{};
    static ItemGridPicker::State cloneItemPickerState{};
    static ItemGridPicker::State dropItemPickerState{};
    static nlohmann::json drops=nlohmann::json::array();
    RefreshSnapshot(CatalogCategory(view));const auto& result=UiSnapshot;const auto resultRevision=UiRevision;
    const auto& ui=HelpyImGuiTheme::Current;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(generation!=seenGeneration) {
        seenGeneration=generation;clonePending=0;cloneReady=false;cloneAck=false;
        cloneSnapshot=nlohmann::json::object();cloneError.clear();cloneStatus.clear();cloneSoft.clear();
    }
    nlohmann::json receipt;
    if(clonePending && SpawnToolRequests::TakeCompleted(clonePending,receipt)) {
        clonePending=0;cloneStatus=receipt.value("Status",std::string{});
        if(receipt.contains("CloneSource") && receipt["CloneSource"].value("Source",std::string{})==selectedItem) {
            cloneSnapshot=receipt["CloneSource"];cloneReady=true;cloneAck=false;
            cloneSoft=cloneSnapshot.value("SoftDeleteField",std::string{});
        }
        if(receipt.contains("CloneResult"))cloneAck=false;
    }
    const auto submitClone=[&](nlohmann::json request) {
        const auto id=++cloneSequence;request["_RequestId"]=id;request["_Source"]="Settings";
        if(!SpawnToolRequests::TrySubmit(std::move(request)))throw std::runtime_error("Runtime unavailable or another command is pending; nothing submitted.");
        clonePending=id;cloneError.clear();cloneStatus="Queued for the game thread.";
    };
    if(ui.advancedMode)ImGui::TextWrapped("Runtime actions stay serialized on the game thread; browsing and filtering remain UI-side.");
    if(ImGui::Button("Refresh current list")){buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;
        const int category=CatalogCategory(view);if(category>=0){auto& window=LazyCatalog::For(category);const char* mode=(category==0&&window.Query.empty())?"Random":"Search";LazyCatalog::Request(category,mode,window.Query,true,LazyCatalog::InitialRows);}}
    SameLineIfRoom(145.0f);if(ImGui::Button("Refresh players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});
    if(ui.advancedMode){ImGui::Checkbox("I understand Full Scan may load additional candidate assets",&rebuildCatalogs);
    ImGui::BeginDisabled(!rebuildCatalogs);
    if(ImGui::Button("Full Scan / rebuild live Helpy catalog")) {
        buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;rebuildCatalogs=false;SpawnToolRequests::Submit({{"Action","IndexQuickCatalog"}});
    }
    ImGui::EndDisabled();}
    auto label=std::string("Select player");
    if(result.contains("Players"))for(const auto& row:result["Players"])if(row["Path"]==player)label=row["Name"];
    if(ImGui::BeginCombo("Player##spawn",label.c_str())) {
        if(result.contains("Players"))for(const auto& row:result["Players"]) {
            const auto path=row["Path"].get<std::string>();ImGui::PushID(path.c_str());
            if(ImGui::Selectable(row["Name"].get_ref<const std::string&>().c_str(),player==path)){player=path;buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;}
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    const auto invalidateSharedPlacement=[&]{actors[0].Confirmed=false;actors[1].Confirmed=false;buildingConfirmed=false;};
    const auto renderActorSection=[&](int kind,const char* title,ImGuiTreeNodeFlags flags=0) {
        if(!HelpyImGuiTheme::SectionHeader(title,kind==0?HelpyImGuiTheme::Icon::Ai:HelpyImGuiTheme::Icon::Resource,flags))return;
        ImGui::PushID(kind);auto& state=actors[kind];auto& definition=state.Definition;auto& definitionQuery=state.Query;auto& definitionMatches=state.Matches;
        auto& ai=state.ClassPath;auto& name=state.Name;auto& definitionFilter=state.Filter;auto& custom=state.Custom;auto& boss=state.Boss;auto& confirmed=state.Confirmed;
        const bool resource=kind==1;
    if(kind==0) {
        if(ImGui::RadioButton("NPCs",state.Mode==0)){state.Mode=0;definition.clear();confirmed=false;}
        SameLineIfRoom(125.0f);if(ImGui::RadioButton("AI / Enemies",state.Mode==1)){state.Mode=1;definition.clear();confirmed=false;}
    } else {
        if(ImGui::RadioButton("Trees",state.Mode==0)){state.Mode=0;definition.clear();confirmed=false;}
        SameLineIfRoom(155.0f);if(ImGui::RadioButton("Stone / Ore / Other",state.Mode==1)){state.Mode=1;definition.clear();confirmed=false;}
    }
    const bool npc=kind==0&&state.Mode==0;
    bool permanent=Authoring::PermanentSpawn.load();
    if(ImGui::Checkbox("Permanent spawn",&permanent)){Authoring::PermanentSpawn=permanent;invalidateSharedPlacement();}
    if(permanent)ImGui::TextWrapped("Saves original placements to RuneSchema/mods/runeschema/spawns/. Applies in every world using this mod. New AI placements do not automatically enable respawn. Disable/remove the JSON and restart to uninstall.");
    if(kind==0&&!npc) {
        bool overridePower=Authoring::EnemyPower.load()!=-1;
        if(ImGui::Checkbox("Override enemy power level",&overridePower)){Authoring::EnemyPower=overridePower?1:-1;confirmed=false;}
        if(overridePower) {
            int power=Authoring::EnemyPower.load();
            if(ImGui::InputInt("Enemy power level (1..100)",&power)) {Authoring::EnemyPower=std::clamp(power,1,100);confirmed=false;}
            ImGui::TextWrapped("Permanent uses the native AI spawn-point power. Temporary requires a reflected PowerLevel on that AI and refuses unsupported classes; it does not invent stat multipliers.");
        }else ImGui::TextDisabled("Power level: native/default");
    }
    if(!npc&&ImGui::Checkbox(kind==0?"Ad-hoc AI class":"Ad-hoc resource class",&custom))confirmed=false;
    if(npc)custom=false;
    if(custom) {
        if(ImGui::InputText("Blueprint class path",ai,sizeof(ai)))confirmed=false;
        if(ImGui::InputText("Spawn name (optional)",name,sizeof(name)))confirmed=false;
        if(!resource && ImGui::Checkbox("Use custom name for boss title too",&boss))confirmed=false;
    }else {
        if(SearchOnEnter("Search loaded AI/resources","name, class, type or mod id",definitionFilter,sizeof(definitionFilter),definitionQuery))
            LazyCatalog::Search(kind==0?1:2,definitionQuery);
        if(kind==0){state.GridView=HelpyImGuiTheme::Current.aiGridView;state.ViewInitialized=true;if(ImGui::Button(state.GridView?"List view":"Grid view")){state.GridView=!state.GridView;HelpyImGuiTheme::Current.aiGridView=state.GridView;HelpyImGuiTheme::SaveCurrent();}}
        const int lazyCategory=kind==0?1:2;auto& lazyState=LazyCatalog::For(lazyCategory);
        if(!lazyState.Started&&!lazyState.Pending)LazyCatalog::Search(lazyCategory,definitionQuery);
        CacheDefinitions(result,resultRevision,kind,state.Mode,definitionQuery,definitionMatches);
        if(definitionMatches.Rows.size()<24&&lazyState.HasMore&&!lazyState.Pending)LazyCatalog::More(lazyCategory);
        if(state.MetaRevision!=resultRevision||state.MetaKey!=definition) {
            state.MetaRevision=resultRevision;state.MetaKey=definition;state.SelectedLabel=definition.empty()?std::string("Select spawn"):definition;
            if(result.contains("Definitions"))for(const auto& row:result["Definitions"])if(row.value("Key",std::string{})==definition){state.SelectedLabel="["+row.value("Type",std::string("AI"))+"] "+row.value("Name",row.value("Key",std::string{}));break;}
        }
        if(kind==0&&result.contains("Definitions")&&result["Definitions"].is_array()) {
            const auto& definitionRows=result["Definitions"];const auto& theme=HelpyImGuiTheme::Current;if(state.ResultPaneHeightLogical<0.0f)state.ResultPaneHeightLogical=theme.aiResultHeight;
            const float availableLogical=ImGui::GetContentRegionAvail().y/std::max(0.01f,theme.Scale()*theme.density);const float maxPane=std::max(180.0f,availableLogical-theme.Y(120.0f)/std::max(0.01f,theme.Scale()*theme.density));state.ResultPaneHeightLogical=std::clamp(state.ResultPaneHeightLogical,180.0f,std::max(180.0f,maxPane));
            HelpyImGuiTheme::BeginScroll("AIResults",ImVec2(0,theme.Y(state.ResultPaneHeightLogical)),true);const int total=static_cast<int>(definitionMatches.Rows.size());
            const auto tooltip=[&](const nlohmann::json& row,const std::string& title,const std::string& path){ImGui::BeginTooltip();ImGui::TextUnformatted(title.c_str());if(!path.empty()){ImGui::Separator();ImGui::TextUnformatted(path.c_str());}if(theme.verboseTooltips){const auto field=[&](const char* label,const char* key){const auto v=row.value(key,std::string{});if(!v.empty())ImGui::Text("%s: %s",label,v.c_str());};field("Key","Key");field("Type","Type");field("Class","Class");field("Mod ID","ModId");field("Resource family","ResourceFamily");if(row.contains("Available"))ImGui::Text("Available: %s",row.value("Available",true)?"true":"false");const auto reason=row.value("Reason",std::string{});if(!reason.empty())ImGui::TextWrapped("Reason: %s",reason.c_str());}ImGui::EndTooltip();};
            if(!state.GridView){const float iconSize=theme.Y(theme.listIconSize),itemHeight=std::max(ImGui::GetTextLineHeight()+theme.Y(4.0f),iconSize),rowHeight=itemHeight+ImGui::GetStyle().ItemSpacing.y;ImGuiListClipper clipper;clipper.Begin(total,rowHeight);while(clipper.Step()){if(clipper.DisplayEnd>=total-4)LazyCatalog::More(1);for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible){const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),type=row.value("Type",std::string("AI")),title=row.value("Name",key),path=row.value("Class",std::string{});const auto label="["+type+"] "+title;const float rowWidth=ImGui::GetContentRegionAvail().x;ImGui::PushID(visible);ImGui::PushID(key.c_str());const auto pos=ImGui::GetCursorScreenPos();if(ImGui::Selectable("##airow",key==definition,0,ImVec2(rowWidth,itemHeight))){definition=key;confirmed=false;}const auto c=ImVec2(pos.x+theme.X(8.0f)+iconSize*.5f,pos.y+itemHeight*.5f);HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Ai,c,iconSize*.72f,key==definition?theme.accentHover:theme.icon);ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x+theme.X(16.0f)+iconSize,pos.y+(itemHeight-ImGui::GetTextLineHeight())*.5f),ImGui::GetColorU32(theme.text),label.c_str());if(HelpyImGuiTheme::TooltipReady(std::string("ai:")+key+":"+std::to_string(visible),ImGui::IsItemHovered()))tooltip(row,title,path);ImGui::PopID();ImGui::PopID();}}}
            else {const float spacing=ImGui::GetStyle().ItemSpacing.x,minCard=theme.X(theme.gridMinCardWidth),avail=std::max(minCard,ImGui::GetContentRegionAvail().x-theme.X(2.0f));const int cols=std::max(1,static_cast<int>((avail+spacing)/(minCard+spacing)));const float cardW=(avail-spacing*(cols-1))/cols,cardH=theme.Y(theme.gridCardHeight),icon=theme.Y(theme.gridIconSize),gridRowH=cardH+ImGui::GetStyle().ItemSpacing.y;const int rows=(total+cols-1)/cols;ImGuiListClipper clipper;clipper.Begin(rows,gridRowH);while(clipper.Step()){if(clipper.DisplayEnd*cols>=total-4)LazyCatalog::More(1);for(int gr=clipper.DisplayStart;gr<clipper.DisplayEnd;++gr){for(int col=0;col<cols;++col){const int visible=gr*cols+col;if(visible>=total)break;const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),title=row.value("Name",key),path=row.value("Class",std::string{});ImGui::PushID(visible);ImGui::PushID(key.c_str());const auto a=ImGui::GetCursorScreenPos();const bool clicked=ImGui::InvisibleButton("##aicard",ImVec2(cardW,cardH));const auto b=ImVec2(a.x+cardW,a.y+cardH);auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,b,ImGui::GetColorU32(key==definition?theme.selection:theme.panelAlt),theme.X(theme.childRounding));draw->AddRect(a,b,ImGui::GetColorU32(key==definition?theme.accentHover:theme.border),theme.X(theme.childRounding),0,key==definition?theme.X(2.0f):theme.X(1.0f));HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Ai,ImVec2((a.x+b.x)*.5f,a.y+theme.Y(12.0f)+icon*.5f),icon,key==definition?theme.accentHover:theme.icon);const auto text=title.size()>40?title.substr(0,37)+"...":title;const auto ts=ImGui::CalcTextSize(text.c_str());draw->AddText(ImVec2(a.x+(cardW-ts.x)*.5f,b.y-theme.Y(29.0f)),ImGui::GetColorU32(theme.text),text.c_str());if(clicked){definition=key;confirmed=false;}if(HelpyImGuiTheme::TooltipReady(std::string("ai:")+key+":"+std::to_string(visible),ImGui::IsItemHovered()))tooltip(row,title,path);ImGui::PopID();ImGui::PopID();if(col<cols-1&&visible+1<total)ImGui::SameLine();}}}}
            HelpyImGuiTheme::EndScroll(theme);ImGui::PushID("ai-results-splitter");const float sh=std::max(theme.Y(theme.splitterWidth),theme.Y(5.0f));ImGui::InvisibleButton("##split",ImVec2(ImGui::GetContentRegionAvail().x,sh));const bool active=ImGui::IsItemActive(),hover=ImGui::IsItemHovered();if(active||hover)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);if(active&&ImGui::GetIO().MouseDelta.y!=0.0f){state.ResultPaneHeightLogical+=ImGui::GetIO().MouseDelta.y/std::max(0.01f,theme.Scale()*theme.density);state.ResultPaneHeightLogical=std::clamp(state.ResultPaneHeightLogical,180.0f,std::max(180.0f,maxPane));HelpyImGuiTheme::Current.aiResultHeight=state.ResultPaneHeightLogical;}static bool aiPaneDirty=false;if(active)aiPaneDirty=true;if(aiPaneDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();aiPaneDirty=false;}ImGui::PopID();
            if(definitionMatches.Rows.empty()){const auto& lazy=LazyCatalog::For(1);if(lazy.Pending)ImGui::TextDisabled("Loading runtime definitions...");else if(!lazy.LastError.empty())ImGui::TextWrapped("Definition query failed: %s",lazy.LastError.c_str());else ImGui::TextDisabled("No loaded AI matches this search.");}
            if(!definition.empty())ImGui::TextDisabled("Selected: %s",state.SelectedLabel.c_str());
        } else {
            if(ImGui::BeginCombo("Loaded spawn",state.SelectedLabel.c_str())) {if(result.contains("Definitions")&&result["Definitions"].is_array()){const auto& definitionRows=result["Definitions"];ImGuiListClipper clipper;clipper.Begin(static_cast<int>(definitionMatches.Rows.size()));while(clipper.Step()){if(clipper.DisplayEnd>=static_cast<int>(definitionMatches.Rows.size())-4)LazyCatalog::More(kind==0?1:2);for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible){const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),type=row.value("Type",std::string("AI")),title=row.value("Name",key),path=row.value("Class",std::string{});const auto display="["+type+"] "+title+" — "+key;ImGui::PushID(key.c_str());if(ImGui::Selectable(display.c_str(),key==definition)){definition=key;confirmed=false;}if(HelpyImGuiTheme::TooltipReady(std::string("spawn:")+key,ImGui::IsItemHovered()))ImGui::SetTooltip("%s",path.c_str());ImGui::PopID();}}}if(definitionMatches.Rows.empty()){const auto& lazy=LazyCatalog::For(kind==0?1:2);if(lazy.Pending)ImGui::TextDisabled("Loading runtime definitions...");else if(!lazy.LastError.empty())ImGui::TextWrapped("Definition query failed: %s",lazy.LastError.c_str());else ImGui::TextDisabled("No loaded AI or resource matches this search.");}ImGui::EndCombo();}
        }
        if(ImGui::InputText("Spawn name override (optional)",name,sizeof(name)))confirmed=false;
        if(kind==0&&!npc && ImGui::Checkbox("Use name for boss title too",&boss))confirmed=false;
    }
    std::string selectedClass;bool selectedAvailable=custom,selectedModeAllowed=custom;std::string selectedReason;
    if(!custom&&result.contains("Definitions"))for(const auto& row:result["Definitions"])if(row.value("Key",std::string{})==definition) {
        selectedClass=row.value("Class",std::string{});selectedAvailable=row.value("Available",true);
        selectedModeAllowed=npc?(permanent?row.value("PermanentAllowed",false):row.value("TemporaryAllowed",false)):selectedAvailable;
        selectedReason=npc?(permanent?row.value("PermanentReason",std::string{}):row.value("TemporaryReason",std::string{})):row.value("Reason",std::string{});break;
    }
    if(!selectedModeAllowed&&!definition.empty()&&!selectedReason.empty())ImGui::TextWrapped("Unavailable: %s",selectedReason.c_str());
    if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx"))invalidateSharedPlacement();
    if(ImGui::Checkbox("Grid placement",&grid))invalidateSharedPlacement();
    if(grid) {
        if(ImGui::InputFloat("Grid X offset (cells)",&gridX))invalidateSharedPlacement();
        if(ImGui::InputFloat("Grid Y offset (cells)",&gridY))invalidateSharedPlacement();
        if(ImGui::InputFloat("Grid cell size (cm)",&gridSize))invalidateSharedPlacement();
    }else if(ImGui::InputFloat("Spawn radius (cm)",&radius))invalidateSharedPlacement();
    if(!npc&&ImGui::SliderInt("Count",&count,1,20))invalidateSharedPlacement();
    if(npc&&!permanent){if(ImGui::InputInt("NPC lifetime (seconds)",&npcDuration))confirmed=false;npcDuration=std::clamp(npcDuration,HelpyNodes::MinDuration,HelpyNodes::MaxDuration);}
    if(!npc&&ImGui::Checkbox("Override ghost mesh setting",&overrideGhost))invalidateSharedPlacement();
    if(!npc&&overrideGhost && ImGui::Checkbox("Ghost mesh overlay",&ghost))invalidateSharedPlacement();
    if(!npc&&ImGui::Checkbox("Append additional drops (AI / resources)",&overrideDrops))invalidateSharedPlacement();
    if(overrideDrops) {
        ImGui::SeparatorText("Additional loot");
        const auto dropPicker=ItemGridPicker::Render("additional-drop",result,dropItemFilter,sizeof(dropItemFilter),
            item,sizeof(item),dropItemPickerState,resultRevision,true);
        if(dropPicker.Changed)invalidateSharedPlacement();
        if(item[0]=='/') {
            ImGui::Text("Selected: %s",dropPicker.SelectedTitle.c_str());
            ImGui::TextDisabled("%s",item);
        }
        if(ui.advancedMode&&ImGui::CollapsingHeader("Advanced item path override")) {
            ImGui::InputText("Item asset path##additional-drop",item,sizeof(item));
            ImGui::TextDisabled("Use this only for a valid loaded item path that is not available in the current catalog.");
        }
        ImGui::InputInt("Minimum quantity",&dropMin);ImGui::InputInt("Maximum quantity",&dropMax);
        ImGui::SliderFloat("Drop chance (%)",&chance,0,100);
        const bool haveDropSelection=!dropPicker.SelectedPaths.empty()||item[0]=='/';
        ImGui::BeginDisabled(drops.size()>=16 || !haveDropSelection || dropMin<1 || dropMax<dropMin || dropMax>10000);
        if(ImGui::Button(dropPicker.SelectedCount>1?"Add selected items to additional loot":"Add selected item to additional loot")) {
            std::vector<std::string> paths=dropPicker.SelectedPaths;if(paths.empty()&&item[0]=='/')paths.push_back(item);
            for(const auto& path:paths){if(drops.size()>=16)break;drops.push_back({{"Item",path},{"Min",dropMin},{"Max",dropMax},{"ChancePercent",chance}});}
            invalidateSharedPlacement();
        }
        ImGui::EndDisabled();
        SameLineIfRoom(105.0f);ImGui::TextDisabled("%zu / 16 entries",drops.size());

        static uint64_t dropTitlesRevision=0;static std::vector<std::string> dropPaths,dropTitles;
        std::vector<std::string> currentDropPaths;currentDropPaths.reserve(drops.size());for(const auto& drop:drops)currentDropPaths.push_back(drop.value("Item",std::string{}));
        if(dropTitlesRevision!=resultRevision||dropPaths!=currentDropPaths) {dropTitlesRevision=resultRevision;dropPaths=std::move(currentDropPaths);dropTitles.clear();dropTitles.reserve(dropPaths.size());for(const auto& path:dropPaths)dropTitles.push_back(ItemGridPicker::Title(result,path));}
        if(!drops.empty() && ImGui::BeginTable("AdditionalDropList",4,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Item",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Quantity",ImGuiTableColumnFlags_WidthFixed,ui.X(90.0f));
            ImGui::TableSetupColumn("Chance",ImGuiTableColumnFlags_WidthFixed,ui.X(80.0f));
            ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,ui.X(70.0f));
            ImGui::TableHeadersRow();
            for(size_t i=0;i<drops.size();++i) {
                const auto path=drops[i].value("Item",std::string{});
                const auto& title=i<dropTitles.size()?dropTitles[i]:path;
                ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(title.c_str());
                if(HelpyImGuiTheme::TooltipReady(std::string("drop:")+path+":"+std::to_string(i),ImGui::IsItemHovered()))ImGui::SetTooltip("%s",path.c_str());
                ImGui::TableSetColumnIndex(1);ImGui::Text("%d-%d",drops[i].value("Min",1),drops[i].value("Max",1));
                ImGui::TableSetColumnIndex(2);ImGui::Text("%.1f%%",drops[i].value("ChancePercent",100.0f));
                ImGui::TableSetColumnIndex(3);
                if(ImGui::SmallButton("Remove")){drops.erase(i);invalidateSharedPlacement();ImGui::PopID();break;}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if(custom&&ImGui::SliderFloat("Distance from player (metres)",&distance,3,50))invalidateSharedPlacement();
    if(!npc&&ImGui::InputFloat("Placement yaw (degrees)",&yaw))invalidateSharedPlacement();
    if(!npc&&ImGui::InputFloat("Height offset above ground (cm)",&height))invalidateSharedPlacement();
    ImGui::Checkbox(permanent?"I want to spawn and install these placements":"I want to create these functioning temporary actors",&confirmed);
    ImGui::BeginDisabled(!confirmed || player.empty() || (!custom && (definition.empty()||!selectedAvailable||!selectedModeAllowed)));
    if(ImGui::Button("Spawn near selected player")) {
        nlohmann::json request={{"Action","Spawn"},{"_Source","HelpyStandalone"},{"Player",player},{"Definition",custom?"":definition},
            {"Class",custom?std::string(ai):selectedClass},{"Name",name},{"Scale",scale},{"Permanent",permanent}};
        if(custom) {request["Boss"]=boss;request["Resource"]=resource;request["Yaw"]=yaw;request["Height"]=height;request["Count"]=count;request["Distance"]=distance;}
        else {
            request["NPC"]=npc;request["Resource"]=resource;request["Count"]=npc?1:count;request["Distance"]=5.0;
            request["PlacementMode"]=grid?"Grid":"Radius";
            if(grid){request["GridX"]=gridX;request["GridY"]=gridY;request["GridSize"]=gridSize;}else request["PlacementRadius"]=radius;
            if(npc)request["DurationSeconds"]=permanent?0:npcDuration;
            else {request["Yaw"]=yaw;request["Height"]=height;request["Boss"]=boss;if(kind==0&&Authoring::EnemyPower.load()!=-1)request["PowerLevel"]=Authoring::EnemyPower.load();
                if(overrideGhost)request["GhostMesh"]=ghost;if(overrideDrops){request["AppendAdditionalDrops"]=true;request["AdditionalDrops"]=drops;}}
        }
        SpawnToolRequests::Submit(std::move(request));confirmed=false;
    }
    ImGui::EndDisabled();
    if(ImGui::Button("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
    ImGui::TextWrapped("Exports use original placement, not wandering AI positions. Export/clear do not install a mod or change live actors. Resource classes must expose verified save exclusion; unsupported classes are refused.");
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    ImGui::PopID();
    };
    if(view==View::All||view==View::Ai)renderActorSection(0,"AI",ImGuiTreeNodeFlags_DefaultOpen);
    if(view==View::All||view==View::Resources)renderActorSection(1,"Resources",ImGuiTreeNodeFlags_DefaultOpen);
    if(view==View::All||view==View::Items||view==View::ItemLab) {
        if((view==View::All||view==View::Items) && HelpyImGuiTheme::SectionHeader("Items",HelpyImGuiTheme::Icon::Item,ImGuiTreeNodeFlags_DefaultOpen)) {
            if(ui.advancedMode&&ImGui::CollapsingHeader("Give / spawn item",ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto inventoryPicker=ItemGridPicker::Render("inventory",result,itemFilter,sizeof(itemFilter),selectedItem,sizeof(selectedItem),itemPickerState,resultRevision,true,true);
                if(inventoryPicker.RequestedAction==ItemGridPicker::ContextAction::GiveOne||inventoryPicker.RequestedAction==ItemGridPicker::ContextAction::GiveStack) {
                    if(player.empty())ImGui::OpenPopup("Select a player first##item-context-error");
                    else if(!SpawnToolRequests::IsWaiting()){nlohmann::json quickItems=nlohmann::json::array();if(inventoryPicker.ActionItems.is_array()&&!inventoryPicker.ActionItems.empty())quickItems=inventoryPicker.ActionItems;else quickItems.push_back({{"Item",inventoryPicker.ActionPath},{"Count",inventoryPicker.ActionCount}});SpawnToolRequests::Submit({{"Action","GiveItems"},{"_Source","HelpyStandalone"},{"Player",player},{"Items",std::move(quickItems)}});}
                }
                if(ImGui::BeginPopup("Select a player first##item-context-error")){ImGui::TextUnformatted("Select a player before using Give One Now or Give full stack.");if(ImGui::Button("OK"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
                // Inventory above, staging chest below. Selection never mutates
                // the cart and a transfer never gives an item implicitly.
                RenderCartDock();
            }
        }
        if((view==View::All||view==View::ItemLab) && HelpyImGuiTheme::SectionHeader("Item Lab",HelpyImGuiTheme::Icon::Lab,ImGuiTreeNodeFlags_DefaultOpen)) {
            if(ImGui::CollapsingHeader("Clone creator (Experimental)",ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Single-player experiment. Create a distinct item from a loaded source. This does not edit the source; native save/reload safety is NOT established.");
                const auto picker=ItemGridPicker::Render("clone-source",result,itemFilter,sizeof(itemFilter),selectedItem,sizeof(selectedItem),cloneItemPickerState,resultRevision,false);
                if(picker.Changed || (cloneReady && cloneSnapshot.value("Source",std::string{})!=selectedItem)) {
                    cloneReady=false;cloneAck=false;cloneSnapshot=nlohmann::json::object();cloneSoft.clear();
                    cloneError.clear();cloneStatus.clear();
                    std::snprintf(cloneName,sizeof(cloneName),"%s",(picker.SelectedTitle+" Clone").c_str());
                    cloneIcon[0]=0;std::snprintf(cloneEditor,sizeof(cloneEditor),"{}");
                }
                ImGui::BeginDisabled(player.empty() || selectedItem[0]!='/' || clonePending || SpawnToolRequests::IsWaiting());
                if(ImGui::Button("Inspect loaded item / reload fields"))try {
                    Authoring::ValidateObjectPath(selectedItem);cloneReady=false;
                    submitClone({{"Action","InspectClone"},{"Player",player},{"Source",selectedItem}});
                }catch(const std::exception& e){cloneError=e.what();}
                ImGui::EndDisabled();
                ImGui::InputText("Display name override (blank inherits)",cloneName,sizeof(cloneName));
                ImGui::InputText("Icon asset path override (blank inherits)",cloneIcon,sizeof(cloneIcon));
                bool permanent=Authoring::PermanentAsset.load();
                if(ImGui::Checkbox("Permanent asset",&permanent)){Authoring::PermanentAsset=permanent;cloneAck=false;}
                ImGui::TextWrapped("%s",permanent?"Install to RuneSchema/mods/runeschema/assets/. The definition reloads with the same identity; no automatic inventory re-grant.":
                    cloneSoft.empty()?"Temporary clone blocked until a reflected soft-delete Boolean is found.":("Temporary: "+cloneSoft+" = true. No installed asset JSON. Save safety remains experimental.").c_str());
                ImGui::Checkbox("Give the created clone to selected player",&cloneGive);
                ImGui::InputInt("Clone quantity",&itemCount);itemCount=std::clamp(itemCount,1,10000);
                SearchOnEnter("Find item setting","name or property type",cloneFilter,sizeof(cloneFilter),cloneQuery);
                if(cloneReady && cloneSnapshot.contains("Fields") && cloneSnapshot["Fields"].is_array()) {
                    const auto source=cloneSnapshot.value("Source",std::string{});
                    if(cloneFieldSource!=source||cloneFieldQuery!=cloneQuery) {
                        cloneFieldSource=source;cloneFieldQuery=cloneQuery;cloneFieldMatches.clear();const auto filter=Lower(cloneQuery);const auto& fields=cloneSnapshot["Fields"];
                        cloneFieldMatches.reserve(fields.size());for(size_t i=0;i<fields.size();++i){const auto& field=fields[i];const auto key=field.value("Name",std::string{}),type=field.value("Type",std::string{});
                            if(filter.empty()||Lower(key+" "+type).find(filter)!=std::string::npos)cloneFieldMatches.push_back(i);}
                    }
                    HelpyImGuiTheme::BeginScroll("CloneReflectedFields",ImVec2(0,ui.Y(220.0f)),true);const auto& fields=cloneSnapshot["Fields"];
                    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(cloneFieldMatches.size()));while(clipper.Step())for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible) {
                        const auto index=cloneFieldMatches[static_cast<size_t>(visible)];if(index>=fields.size())continue;const auto& field=fields[index];
                        const auto key=field.at("Name").get<std::string>(),type=field.at("Type").get<std::string>();const bool editable=field.value("Editable",false),hasValue=field.value("HasValue",false);
                        const auto label=key+" ("+type+") = "+(hasValue?field["Value"].dump():"[inherited]");ImGui::PushID(key.c_str());ImGui::BeginDisabled(!editable);
                        if(ImGui::Selectable(label.c_str()))try {if(!hasValue)throw std::runtime_error("This composite field is inherited. Enter a schema-compatible override for '"+key+"' in the JSON editor.");
                            auto overrides=Authoring::ParseCloneJson(cloneEditor);if(!overrides.is_object())throw std::runtime_error("Overrides must be a JSON object.");if(!overrides.contains(key))overrides[key]=field["Value"];
                            const auto text=overrides.dump(2);if(text.size()>=sizeof(cloneEditor))throw std::runtime_error("Overrides exceed this editor's capacity.");std::snprintf(cloneEditor,sizeof(cloneEditor),"%s",text.c_str());cloneError.clear();
                        }catch(const std::exception& e){cloneError=e.what();}
                        ImGui::EndDisabled();if(HelpyImGuiTheme::TooltipReady(std::string("field:")+key,ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))ImGui::SetTooltip("%s\n%s",key.c_str(),field.value("Reason",std::string("Click to copy this current value into overrides.")).c_str());ImGui::PopID();
                    }
                    HelpyImGuiTheme::EndScroll(ui);
                }
                ImGui::TextWrapped("Override JSON: only these fields change. Click a setting above to add its current value; delete a key to inherit it. Explicit JSON overrides take precedence over the name/icon boxes. Identity and soft-delete fields are backend-managed.");
                ImGui::InputTextMultiline("Item overrides JSON",cloneEditor,sizeof(cloneEditor),ImVec2(-1,ui.Y(220.0f)));
                ImGui::Checkbox("I accept experimental testing on a disposable or backed-up save",&cloneAck);
                ImGui::BeginDisabled(!cloneReady || !cloneAck || player.empty() || clonePending || SpawnToolRequests::IsWaiting() || (!permanent&&cloneSoft.empty()));
                if(ImGui::Button("Create clone now"))try {
                    auto overrides=Authoring::ParseCloneJson(cloneEditor);
                    submitClone(Authoring::CloneRequest(selectedItem,cloneName,cloneIcon,std::move(overrides),permanent,cloneGive,itemCount,cloneAck,player));
                    cloneAck=false;
                }catch(const std::exception& e){cloneError=e.what();}
                ImGui::EndDisabled();
                if(!cloneError.empty())ImGui::TextWrapped("Not submitted: %s",cloneError.c_str());
                if(!cloneStatus.empty())ImGui::TextWrapped("%s",cloneStatus.c_str());
            }
        }
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    }
    if((view==View::All||view==View::Buildings) && HelpyImGuiTheme::SectionHeader("Buildings",HelpyImGuiTheme::Icon::Building,ImGuiTreeNodeFlags_DefaultOpen)) {
        if(ImGui::Button("Load saved building catalog"))SpawnToolRequests::Submit({{"Action","Catalog"}});
        SameLineIfRoom(230.0f);if(ImGui::Button("Rebuild building safety catalog"))SpawnToolRequests::Submit({{"Action","RebuildCatalogs"}});
        SearchOnEnter("Search loaded buildings","name or BuildingPieceData path",buildingFilter,sizeof(buildingFilter),buildingQuery);
        CacheBuildings(result,resultRevision,buildingQuery,buildingMatches);
        static uint64_t buildingMetaRevision=0,buildingCountsRevision=0;static std::string buildingMetaPath,buildingLabel,selectedBuildingReason;static bool selectedBuildingSafe=false;static size_t verifiedBuildings=0,blockedBuildings=0;
        if(buildingCountsRevision!=resultRevision) {buildingCountsRevision=resultRevision;verifiedBuildings=blockedBuildings=0;if(result.contains("Buildings"))for(const auto& row:result["Buildings"])row.value("Safe",false)?++verifiedBuildings:++blockedBuildings;}
        if(buildingMetaRevision!=resultRevision||buildingMetaPath!=selectedBuilding) {
            buildingMetaRevision=resultRevision;buildingMetaPath=selectedBuilding;buildingLabel=selectedBuilding;selectedBuildingSafe=false;selectedBuildingReason.clear();
            if(result.contains("Buildings"))for(const auto& row:result["Buildings"])if(row.value("Path",std::string{})==selectedBuilding){buildingLabel=row.value("Name",buildingLabel);selectedBuildingSafe=row.value("Safe",false);selectedBuildingReason=row.value("SafetyReason",std::string{});break;}
        }
        auto label=buildingLabel;if(verifiedBuildings+blockedBuildings)ImGui::TextDisabled("Safety scope: %zu verified / %zu blocked",verifiedBuildings,blockedBuildings);
        if(ImGui::BeginCombo("Loaded building",label.empty()?"Select building":label.c_str())) {
            if(result.contains("Buildings")&&result["Buildings"].is_array()) {
                const auto& buildingRows=result["Buildings"];ImGuiListClipper clipper;clipper.Begin(static_cast<int>(buildingMatches.Rows.size()));
                while(clipper.Step())for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible) {
                    const auto index=buildingMatches.Rows[static_cast<size_t>(visible)];if(index>=buildingRows.size())continue;const auto& row=buildingRows[index];
                    const auto path=row.value("Path",std::string{}),title=row.value("Name",path);const bool safe=row.value("Safe",false);ImGui::PushID(path.c_str());ImGui::BeginDisabled(!safe);
                    if(ImGui::Selectable(title.c_str(),path==selectedBuilding)){std::snprintf(selectedBuilding,sizeof(selectedBuilding),"%s",path.c_str());buildingConfirmed=false;}
                    ImGui::EndDisabled();if(HelpyImGuiTheme::TooltipReady(std::string("building:")+path,ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))ImGui::SetTooltip("%s\nSafety: %s",path.c_str(),row.value("SafetyReason",safe?std::string("Verified"):std::string("Blocked")).c_str());ImGui::PopID();
                }
            }
            if(buildingMatches.Rows.empty())ImGui::TextDisabled("No loaded BuildingPieceData matches this search.");
            ImGui::EndCombo();
        }
        if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx")){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        ImGui::TextDisabled("Safety mode: one building preview per spawn; grid/batch placement is disabled.");
        if(ImGui::SliderFloat("Distance from player (metres)",&distance,3,50)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::InputFloat("Placement yaw (degrees)",&yaw)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::InputFloat("Height offset above ground (cm)",&height)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::Checkbox("Allow deconstruction",&allowDeconstruction))buildingConfirmed=false;
        ImGui::Text("Availability");ImGui::SameLine();
        if(ImGui::RadioButton("Any##building-time",&buildingTime,0))buildingConfirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Day##building-time",&buildingTime,1))buildingConfirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Night##building-time",&buildingTime,2))buildingConfirmed=false;
        ImGui::TextWrapped("Temporary previews use the selected BuildingPieceData's native buildable actor only after a matching native in-world instance proves the exact data binding. The spawned actor is transient, excluded from the native save store, and re-verified after construction before RuneSchema keeps it.");
        if(selectedBuilding[0]=='/' && !selectedBuildingSafe)ImGui::TextWrapped("Blocked by safety preflight: %s",selectedBuildingReason.c_str());
        ImGui::Checkbox("I want to spawn one verified temporary building preview",&buildingConfirmed);
        ImGui::BeginDisabled(!buildingConfirmed || player.empty() || selectedBuilding[0]!='/' || !selectedBuildingSafe);
        if(ImGui::Button("Spawn building near selected player")) {
            nlohmann::json request={{"Action","SpawnBuilding"},{"Player",player},{"Building",selectedBuilding},
                {"Yaw",yaw},{"Height",height},{"Scale",scale},{"Count",1},{"Distance",distance},
                {"AllowDeconstruction",allowDeconstruction},{"TimeOfDay",buildingTime==1?"Day":buildingTime==2?"Night":"Any"}};
            SpawnToolRequests::Submit(std::move(request));buildingConfirmed=false;
        }
        ImGui::EndDisabled();
        if(ImGui::Button("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
        ImGui::TextWrapped("The catalog keeps all loaded BuildingPieceData for inspection. Saved safety results are never trusted across worlds: rebuild in the current authoritative world to enable only pieces that match a live native instance, exact BuildingPieceData binding, and save-exclusion contract.");
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    }

}

inline void Render(){Render(View::All);}

inline void RenderSessionCleanup() {
    static std::string player;RefreshSnapshot(-1);const auto& result=UiSnapshot;
    ImGui::TextWrapped("Clean only actors and staged placements created by the current Mod Authoring session. Installed /spawns content and saves are not changed.");
    if(ImGui::Button("Refresh authoritative players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});
    auto label=std::string("Select player");
    if(result.contains("Players"))for(const auto& row:result["Players"])if(row.value("Path",std::string{})==player)label=row.value("Name",label);
    if(ImGui::BeginCombo("Player##cleanup",label.c_str())) {
        if(result.contains("Players"))for(const auto& row:result["Players"]) {
            const auto path=row.value("Path",std::string{});ImGui::PushID(path.c_str());
            if(ImGui::Selectable(row.value("Name",std::string("Player")).c_str(),player==path))player=path;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(player.empty());
    if(ImGui::Button("Dismiss Helpy temporary NPCs"))SpawnToolRequests::Submit({{"Action","DismissHelpyNpcs"},{"_Source","HelpyStandalone"},{"Player",player}});
    ImGui::SameLine();
    if(ImGui::Button("Clean up live tool-created actors"))SpawnToolRequests::Submit({{"Action","Cleanup"},{"Player",player}});
    ImGui::EndDisabled();
    if(ImGui::Button("Clear staged export placements"))SpawnToolRequests::Submit({{"Action","ClearPlacements"}});
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
}
}
