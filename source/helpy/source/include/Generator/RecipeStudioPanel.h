#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/RecipeWorkspace.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ToolRequest.h"
#include <imgui_internal.h>
#include <array>
#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
namespace PS::RecipeStudioPanel {
using Json=nlohmann::json;
struct Workspace {
    Json Inputs=Json::array(),Outputs=Json::array(),Catalog=Json::object(),Stations=Json::array(),Placements=Json::array(),Station=Json::object();
    std::map<std::string,Json> Items;
    std::string Status,Error,LoadedRecipe,StationRoot;
    char Search[257]{},Source[1025]{},Category[129]="Custom",StationIcon[1025]{},ItemFilter[257]{},SelectedItem[1025]{};
    char StationSearch[257]{};
    bool Override=false,AddStation=false,Unlock=true,Duration=false,InputsGrid=true,OutputsGrid=true,CatalogRequested=false;
    float MinSeconds=1,MaxSeconds=1;
    uint64_t Pending=0,Sequence=0x6000000000000000ull,Revision=0,Generation=0;
    ItemGridPicker::State Bay;
};
inline Workspace& State(){static Workspace value;return value;}
inline void Submit(Json request) {
    auto& state=State();if(state.Pending){state.Error="Wait for the current authoring request.";return;}
    const auto id=++state.Sequence;request["_RequestId"]=id;request["_Source"]="HelpyStandalone";
    if(!SpawnToolRequests::TrySubmit(std::move(request))){state.Error="Load a world and wait for the current runtime operation.";return;}
    state.Pending=id;state.Error.clear();
}
inline void Poll() {
    auto& state=State();const auto generation=SpawnToolRequests::CurrentGeneration();
    if(state.Generation!=generation){state.Generation=generation;state.Pending=0;state.Catalog=Json::object();state.Stations=Json::array();state.Placements=Json::array();state.Station=Json::object();state.Items.clear();state.Revision=0;state.LoadedRecipe.clear();state.Override=false;state.CatalogRequested=false;state.Inputs=Json::array();state.Outputs=Json::array();}
    if(!state.Pending)return;Json receipt;if(!SpawnToolRequests::TakeCompleted(state.Pending,receipt))return;state.Pending=0;
    state.Status=receipt.value("Status",std::string{});
    if(receipt.contains("RecipeWorkspaceCatalog")) {
        state.Catalog=receipt["RecipeWorkspaceCatalog"];state.Stations=state.Catalog.value("Stations",Json::array());
    }
    if(receipt.contains("RecipeWorkspaceSource"))try {
        const auto& source=receipt["RecipeWorkspaceSource"];
        const auto inputs=source.at("ItemsConsumed"),outputs=source.at("ItemsCreated");
        RecipeWorkspace::CheckRows(inputs,"Loaded inputs",true);RecipeWorkspace::CheckRows(outputs,"Loaded outputs");
        state.Inputs=inputs;state.Outputs=outputs;state.LoadedRecipe=source.at("SourceRecipe").get<std::string>();
        for(const auto& row:source.value("Items",Json::array())){const auto path=row.value("Path",std::string{});if(!path.empty()&&(state.Items.size()<32768||state.Items.contains(path)))state.Items.insert_or_assign(path,row);}
        std::snprintf(state.Source,sizeof(state.Source),"%s",state.LoadedRecipe.c_str());state.Override=true;state.AddStation=false;
        state.Stations=source.value("Stations",Json::array());
        state.Placements=source.value("Placements",Json::array());state.Station=RecipeWorkspace::LoadedStation(state.Placements);
        std::snprintf(state.Category,sizeof(state.Category),"%s",state.Station.value("Category",std::string("Custom")).c_str());
        state.StationIcon[0]='\0';state.StationSearch[0]='\0';state.StationRoot.clear();
        state.Duration=source.contains("MinProcessingTime")&&source.contains("MaxProcessingTime");
        if(state.Duration){state.MinSeconds=source.at("MinProcessingTime").get<float>();state.MaxSeconds=source.at("MaxProcessingTime").get<float>();}
    }catch(const std::exception& error){state.Error=error.what();}
    if(receipt.contains("RecipeExportResult"))state.Status="Saved: "+receipt["RecipeExportResult"].value("File",std::string{})+". Restart to apply.";
}
inline void Search(int offset=0){auto& s=State();Submit({{"Action","RecipeWorkspaceCatalog"},{"Query",s.Search},{"Offset",offset}});}
inline const Json* Metadata(const std::string& path){auto& s=State();const auto found=s.Items.find(path);return found==s.Items.end()?nullptr:&found->second;}
inline void AddSelected(Json& side){auto& s=State();try{RecipeWorkspace::AddItem(side,{{"Path",s.SelectedItem}});s.Error.clear();}catch(const std::exception& error){s.Error=error.what();}}
inline void SelectItem(const std::string& path) {
    auto& s=State();std::snprintf(s.SelectedItem,sizeof(s.SelectedItem),"%s",path.c_str());
    ItemGridPicker::SelectShared(path);
}
inline bool AcceptItem(Json& side,std::optional<std::size_t> replace={}) {
    if(const auto item=ItemGridPicker::AcceptItemDrop(State().Error)) {
        try{if(replace)RecipeWorkspace::ReplaceItem(side,*replace,item->at("Path").get<std::string>());else RecipeWorkspace::AddItem(side,*item);State().Error.clear();return true;}
        catch(const std::exception& error){State().Error=error.what();}
    }return false;
}
inline void CenterText(const std::string& text,bool disabled=false){
    const auto x=ImGui::GetCursorPosX(),width=ImGui::GetContentRegionAvail().x;
    const float measured=ImGui::CalcTextSize(text.c_str(),nullptr,false,width).x;
    ImGui::SetCursorPosX(x+std::max(0.f,(width-measured)*.5f));
    if(disabled)ImGui::PushStyleColor(ImGuiCol_Text,HelpyImGuiTheme::Current.textDisabled);
    ImGui::PushTextWrapPos(x+width);ImGui::TextUnformatted(text.c_str());ImGui::PopTextWrapPos();
    if(disabled)ImGui::PopStyleColor();ImGui::SetCursorPosX(x);
}
inline ImRect ItemNode(const char* id,const char* title,Json& side,bool& grid,float height) {
    const auto& t=HelpyImGuiTheme::Current;ImRect bounds;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.panelAlt);
    if(ImGui::BeginChild(id,ImVec2(0,height),true)) {
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();bounds=ImRect(pos,ImVec2(pos.x+size.x,pos.y+size.y));
#if defined(HELPY_PREVIEW)
        if(std::string_view(title)=="Inputs")HelpyPreviewHooks::Inputs=HelpyPreviewHooks::WindowRect();
        else if(std::string_view(title)=="Outputs")HelpyPreviewHooks::Outputs=HelpyPreviewHooks::WindowRect();
#endif
        CenterText(std::string(title)+"  "+std::to_string(side.size())+" / 32");ImGui::Separator();
        ImGui::PushID(id);const float left=ImGui::GetCursorPosX();
        const float controls=ImGui::CalcTextSize("Grid").x+ImGui::CalcTextSize("Rows").x+ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemInnerSpacing.x*2+ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(left+std::max(0.f,(ImGui::GetContentRegionAvail().x-controls)*.5f));
        if(ImGui::RadioButton("Grid",grid))grid=true;ImGui::SameLine();if(ImGui::RadioButton("Rows",!grid))grid=false;ImGui::SetCursorPosX(left);
        if(side.empty()){ImGui::Dummy({0,std::max(0.f,(ImGui::GetContentRegionAvail().y-ImGui::GetTextLineHeight())*.5f)});CenterText("Drag items here from the bay below.",true);}
        const auto width=ImGui::GetContentRegionAvail().x;
        const int columns=grid?std::max(1,static_cast<int>(width/std::max(t.X(124),ImGui::GetFontSize()*6.5f))):1;
        std::optional<std::size_t> remove;
        std::optional<std::pair<std::size_t,std::string>> replacement;
        const bool tableOpen=ImGui::BeginTable("recipe-cards",columns,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_PadOuterX);
        if(tableOpen)for(std::size_t i=0;i<side.size();++i) {
            auto& row=side[i];const auto path=row.value("ItemData",std::string{});const auto* meta=Metadata(path);
            const auto name=meta?meta->value("Name",path):QuickDecorations::ReadableAssetName(path);
            ImGui::TableNextColumn();ImGui::PushID(static_cast<int>(i));ImGui::BeginGroup();
            Json drag=meta?*meta:Json::object();drag["Path"]=path;drag["Name"]=name;drag["Count"]=row.value("Count",1);
            const auto at=ImGui::GetCursorScreenPos();const ImVec2 cardSize(ImGui::GetContentRegionAvail().x,t.Y(grid?t.gridCardHeight:58.0f));
            if(ImGui::InvisibleButton("select-item",cardSize,ImGuiButtonFlags_EnableNav))SelectItem(path);
            ItemGridPicker::DrawPlacard(drag,at,ImVec2(at.x+cardSize.x,at.y+cardSize.y),State().SelectedItem==path,ImGui::IsItemHovered());ItemGridPicker::DragItem(drag);
            if(ImGui::BeginDragDropTarget()){if(auto item=ItemGridPicker::AcceptItemDrop(State().Error))replacement={{i,item->at("Path").get<std::string>()}};ImGui::EndDragDropTarget();}
            if(HelpyImGuiTheme::TooltipReady(std::string("recipe:")+id+path,ImGui::IsItemHovered()))ItemGridPicker::VerboseTooltip(drag,name,path,false,true);

            int count=row.value("Count",1);ImGui::SetNextItemWidth(std::max(t.X(50),ImGui::GetContentRegionAvail().x-t.X(30)));
            if(ImGui::InputInt("##quantity",&count,0,0))row["Count"]=std::clamp(count,1,10000);ImGui::SameLine();
            if(ImGui::SmallButton("X"))remove=i;
            ImGui::EndGroup();
            if(ImGui::BeginPopupContextItem("item-options")) {
                if(ImGui::MenuItem("Add to cart"))ItemGridPicker::AddToCart(drag);
                if(ImGui::MenuItem("Select this item"))SelectItem(path);
                if(ImGui::MenuItem("Replace with selected bay item",nullptr,false,State().SelectedItem[0]!=0))
                    try{RecipeWorkspace::ReplaceItem(side,i,State().SelectedItem);State().Error.clear();}catch(const std::exception& error){State().Error=error.what();}
                if(ImGui::MenuItem("Remove"))remove=i;ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if(tableOpen)ImGui::EndTable();
        if(replacement)try{RecipeWorkspace::ReplaceItem(side,replacement->first,replacement->second);State().Error.clear();}catch(const std::exception& error){State().Error=error.what();}
        if(remove)side.erase(*remove);
        if(ImGui::BeginDragDropTargetCustom(bounds,ImGui::GetID("recipe-drop"))) {
            AcceptItem(side);
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }ImGui::EndChild();ImGui::PopStyleColor();return bounds;
}
inline ImRect StationNode(float height) {
    auto& s=State();const auto& t=HelpyImGuiTheme::Current;ImRect bounds;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.panelAlt);
    if(ImGui::BeginChild("recipe-station",ImVec2(0,height),true)) {
        const auto p=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();bounds=ImRect(p,ImVec2(p.x+size.x,p.y+size.y));
        CenterText("Station & timing");ImGui::Separator();
        if(s.Override){HelpyImGuiTheme::Toggle("Also add this station",&s.AddStation);ImGui::TextDisabled("%zu original placement(s) retained",s.Placements.size());}
        const auto icon=s.StationIcon[0]?std::string(s.StationIcon):ItemPresentation::StationIcon(s.Station);
        const auto left=ImGui::GetCursorPosX();ImGui::SetCursorPosX(left+std::max(0.f,(ImGui::GetContentRegionAvail().x-t.Y(42))*.5f));
        const auto at=ImGui::GetCursorScreenPos();const ImVec2 end(at.x+t.Y(42),at.y+t.Y(42));
        if(icon.empty()||!ItemIconCache::Draw(icon,at,end,true))ItemIconCache::Draw(QuickDecorations::CraftingStationIcon,at,end,true);
        ImGui::Dummy(ImVec2(t.Y(42),t.Y(42)));
        ImGui::SetCursorPosX(left);
        if(!s.Station.empty()){
            CenterText(s.Station.value("DisplayName",s.Station.value("Name",std::string{})));
            if(!s.Station.value("PhysicalStation",true))CenterText("Native recipe menu; no station building is linked.",true);
            else CenterText(s.Station.value("Array",std::string{}).empty()?"Crafting station":"Processing station",true);
        }
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##station",s.Station.value("DisplayName",s.Station.value("Name",std::string("Choose a native station"))).c_str())) {
            ImGui::SetNextItemWidth(std::max(t.X(180),ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##station-search","Search name, table, row or mod path",s.StationSearch,sizeof(s.StationSearch));
            if(ImGui::BeginCombo("Source",s.StationRoot.empty()?"All sources":s.StationRoot.c_str())){
                if(ImGui::Selectable("All sources",s.StationRoot.empty()))s.StationRoot.clear();
                std::set<std::string> roots;for(const auto& station:s.Stations)if(station.value("PhysicalStation",true))roots.insert(RecipeWorkspace::StationSource(station));
                for(const auto& root:roots)if(ImGui::Selectable(root.c_str(),s.StationRoot==root))s.StationRoot=root;ImGui::EndCombo();
            }
            std::size_t matches=0;
            for(const auto& station:s.Stations){if(!station.value("PhysicalStation",true)||!RecipeWorkspace::StationMatches(station,s.StationSearch,s.StationRoot))continue;++matches;
                const auto name=station.value("Name",std::string{}),id=station.value("Id",name);ImGui::PushID(id.c_str());
                const auto point=ImGui::GetCursorScreenPos();const float extent=t.Y(20);
                if(ImGui::IsRectVisible(ImVec2(extent,extent))){const auto stationIcon=ItemPresentation::StationIcon(station);const ImVec2 end(point.x+extent,point.y+extent);
                    if(stationIcon.empty()||!ItemIconCache::Draw(stationIcon,point,end,true))ItemIconCache::Draw(QuickDecorations::CraftingStationIcon,point,end,true);}
                ImGui::Dummy(ImVec2(extent,extent));ImGui::SameLine();
                if(ImGui::Selectable(station.value("DisplayName",name).c_str(),s.Station.value("Id",std::string{})==id)){
                    s.Station=station;bool original=false;for(const auto& placement:s.Placements)if(placement.value("Id",std::string{})==id){s.Station=placement;original=true;break;}
                    if(s.Override&&!original)s.AddStation=true;
                    if(s.Station.contains("Category"))std::snprintf(s.Category,sizeof(s.Category),"%s",s.Station.value("Category",std::string{}).c_str());
                }
                if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();PS::HelpyImGuiTheme::DetailsTextUnformatted(station.value("DisplayName",name).c_str());
                    if(t.verboseTooltips)PS::HelpyImGuiTheme::DetailsText("%s",ItemPresentation::Text(station,"Path").c_str());HelpyImGuiTheme::EndDetailsTooltip();}
                ImGui::PopID();
            }
            if(!matches)ImGui::TextDisabled("No loaded compatible stations match.");ImGui::EndCombo();
        }
        if(s.Override&&s.Placements.empty())ImGui::TextWrapped("No loaded station references this recipe. Existing placements are not changed.");
        ImGui::BeginDisabled(s.Override&&!s.AddStation);
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##category","Recipe category",s.Category,sizeof(s.Category));
        ImGui::EndDisabled();
        HelpyImGuiTheme::Toggle("Set processing duration",&s.Duration);
        if(s.Duration){ImGui::TextUnformatted("Minimum seconds");ImGui::SetNextItemWidth(-1);ImGui::InputFloat("##min-seconds",&s.MinSeconds,0.5f,5.0f,"%.2f");ImGui::TextUnformatted("Maximum seconds");ImGui::SetNextItemWidth(-1);ImGui::InputFloat("##max-seconds",&s.MaxSeconds,0.5f,5.0f,"%.2f");}
        if(HelpyImGuiTheme::Current.advancedMode){ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##station-icon","Custom cooked station icon path",s.StationIcon,sizeof(s.StationIcon));}
    }ImGui::EndChild();ImGui::PopStyleColor();return bounds;
}
inline void Connector(const ImRect& left,const ImRect& right) {
    if(left.GetWidth()<=0||right.GetWidth()<=0)return;
    const auto& t=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    const bool horizontal=right.Min.x>left.Max.x;const auto a=horizontal?ImVec2(left.Max.x,left.GetCenter().y):ImVec2(left.GetCenter().x,left.Max.y);
    const auto b=horizontal?ImVec2(right.Min.x,right.GetCenter().y):ImVec2(right.GetCenter().x,right.Min.y);
    const auto c1=horizontal?ImVec2((a.x+b.x)*0.5f,a.y):ImVec2(a.x,(a.y+b.y)*0.5f),c2=horizontal?ImVec2((a.x+b.x)*0.5f,b.y):ImVec2(b.x,(a.y+b.y)*0.5f);
    const auto color=ImGui::GetColorU32(t.accentHover);draw->AddBezierCubic(a,c1,c2,b,color,t.X(2));
    const float r=t.X(4);draw->AddTriangleFilled(b,horizontal?ImVec2(b.x-r,b.y-r):ImVec2(b.x-r,b.y-r),horizontal?ImVec2(b.x-r,b.y+r):ImVec2(b.x+r,b.y-r),color);
}
inline void Render(const Json& catalog,uint64_t revision) {
    Poll();auto& s=State();const auto& t=HelpyImGuiTheme::Current;
    if(!s.CatalogRequested&&!s.Pending){s.CatalogRequested=true;Search();}
    if(s.Revision!=revision){s.Revision=revision;if(catalog.contains("Items")&&catalog["Items"].is_array())for(const auto& row:catalog["Items"]){const auto path=row.value("Path",std::string{});if(!path.empty()&&(s.Items.size()<32768||s.Items.contains(path)))s.Items.insert_or_assign(path,row);}}
    const bool browseExisting=ImGui::CollapsingHeader("Load an existing recipe",s.Override?ImGuiTreeNodeFlags_DefaultOpen:0);
    if(browseExisting){ImGui::BeginDisabled(s.Pending!=0);
    const float browseWidth=ImGui::CalcTextSize("Browse").x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(t.X(60),ImGui::GetContentRegionAvail().x-browseWidth));
    const bool search=ImGui::InputTextWithHint("##recipe-search","Search cooked and RuneSchema recipes, names or mods",s.Search,sizeof(s.Search),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Browse")||search)Search();
    if(ImGui::BeginCombo("Recipes",s.LoadedRecipe.empty()?"Choose a cooked or RuneSchema recipe":s.LoadedRecipe.c_str())) {
        for(const auto& row:s.Catalog.value("Recipes",Json::array())){const auto path=row.value("Path",std::string{}),reference=row.value("Reference",path),owner=row.value("Owner",std::string{});
            ImGui::PushID((path+reference).c_str());ImGui::BeginDisabled(path.empty()||!row.value("Available",true));
            const auto label=row.value("Name",path)+(owner.empty()?"":"  ["+owner+"]");if(ImGui::Selectable(label.c_str()))Submit({{"Action","InspectRecipe"},{"SourceRecipe",path}});
            ImGui::EndDisabled();if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))PS::HelpyImGuiTheme::Tooltip("%s\n%s",path.empty()?reference.c_str():path.c_str(),row.value("Available",true)?row.value("Source",std::string{}).c_str():"Definition is present but its native recipe is not ready; inspect loader errors.");ImGui::PopID();}ImGui::EndCombo();
    }
    if(s.Catalog.contains("Total"))ImGui::TextDisabled("%zu matching recipes",s.Catalog.value("Total",std::size_t{}));
    if(s.Catalog.contains("MountedCatalogReady")&&!s.Catalog.value("MountedCatalogReady",false))ImGui::TextWrapped("Mounted content metadata is unavailable; showing loaded recipes and declared definitions only. Browse again when ready.");
    else if(s.Catalog.contains("SubclassCatalogReady")&&!s.Catalog.value("SubclassCatalogReady",false))ImGui::TextWrapped("Subclass metadata is unavailable; unloaded Blueprint-derived recipes may be missing. Loaded recipes and known native/declared recipes remain available.");
    const auto recipeOffset=s.Catalog.value("Offset",0);if(recipeOffset>0&&ImGui::SmallButton("Previous recipe page"))Search(std::max(0,recipeOffset-128));
    if(s.Catalog.value("HasMore",false)){if(recipeOffset>0)ImGui::SameLine();if(ImGui::SmallButton("Next recipe page"))Search(recipeOffset+128);}
    const float loadWidth=ImGui::CalcTextSize("Load").x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(t.X(60),ImGui::GetContentRegionAvail().x-loadWidth));ImGui::InputTextWithHint("##recipe-source","Or paste a cooked RecipeData object path",s.Source,sizeof(s.Source));ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Load")&&s.Source[0])Submit({{"Action","InspectRecipe"},{"SourceRecipe",s.Source}});
    if(!s.LoadedRecipe.empty())HelpyImGuiTheme::Toggle("Save as an override of the loaded recipe",&s.Override);
    ImGui::EndDisabled();}
    const int columns=ImGui::GetContentRegionAvail().x>=std::max(t.X(690),ImGui::CalcTextSize("Station & timing").x*4.5f)?3:1;
    const float height=std::max(t.Y(170),std::min(t.Y(240),ImGui::GetContentRegionAvail().y*0.45f));
    std::array<ImRect,3> nodes;ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(t.X(10),t.Y(9)));
    if(ImGui::BeginTable("recipe-flow",columns,ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();nodes[0]=ItemNode("recipe-inputs","Inputs",s.Inputs,s.InputsGrid,height);
        ImGui::TableNextColumn();nodes[1]=StationNode(height);
        ImGui::TableNextColumn();nodes[2]=ItemNode("recipe-outputs","Outputs",s.Outputs,s.OutputsGrid,height);
        ImGui::EndTable();
    }ImGui::PopStyleVar();Connector(nodes[0],nodes[1]);Connector(nodes[1],nodes[2]);
    HelpyImGuiTheme::Toggle("Unlock the saved recipe",&s.Unlock);
    if(ImGui::GetContentRegionAvail().x>ImGui::CalcTextSize("Unlock the saved recipe    Save recipe override").x)ImGui::SameLine();ImGui::BeginDisabled(s.Pending!=0);
    if(HelpyImGuiTheme::SoftButton(s.Override?"Save recipe override":"Save new recipe"))try {
        Json options={{"Ingredients",s.Inputs},{"Outputs",s.Outputs},{"Unlock",s.Unlock},{"Category",s.Category},{"Station",s.Station},{"AddStation",s.AddStation}};
        if(s.Override){if(s.LoadedRecipe.empty())throw std::runtime_error("Load a native recipe first");options["SourceRecipe"]=s.LoadedRecipe;}
        if(s.Duration){options["MinProcessingTime"]=s.MinSeconds;options["MaxProcessingTime"]=s.MaxSeconds;}
        const auto properties=RecipeWorkspace::Properties(options);(void)properties;
        if((!s.Override||s.AddStation)&&s.Station.empty())throw std::runtime_error("Choose a crafting or processing station");
        Submit({{"Action","ExportRecipe"},{"Source",s.Outputs.at(0).at("ItemData")},{"Recipe",std::move(options)}});
    }catch(const std::exception& error){s.Error=error.what();}
    ImGui::EndDisabled();if(s.Override)ImGui::TextDisabled("An added station does not remove the recipe from its original stations.");
    if(!s.Error.empty())ImGui::TextWrapped("%s",s.Error.c_str());else if(!s.Status.empty())ImGui::TextWrapped("%s",s.Status.c_str());
    if(!ItemGridPicker::DockedBay){ImGui::SeparatorText("Item Bay");ImGui::TextDisabled("Drag to Inputs or Outputs, or select an item and add it below.");}
    ItemGridPicker::RenderShared(catalog,revision);
    std::snprintf(s.SelectedItem,sizeof(s.SelectedItem),"%s",ItemGridPicker::Bay.Selected);
    ImGui::BeginDisabled(!s.SelectedItem[0]);if(HelpyImGuiTheme::SoftButton("Add selected to inputs"))AddSelected(s.Inputs);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Add selected to outputs"))AddSelected(s.Outputs);ImGui::EndDisabled();
}
}
