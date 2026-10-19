#pragma once
#include "Core/JournalWorkspace.h"
#include "Core/HelpyIconPack.h"
#include "Runtime/HostServices.h"
#include "Generator/HelpyImGuiTheme.h"
#include <future>
#include <cstdio>
namespace PS::JournalAuthorPanel {
using Json=nlohmann::json;
inline char Id[129]="my_mod_lore",Title[1025]{},Image[2049]{},Category[2049]{},Text[65537]{};
inline Json Pages=Json::array({""});
inline int Page=0,Type=0;
inline char Section[257]{},CategoryName[257]{},GroupId[129]{},GroupName[257]{},PlacementKey[129]{};
inline char Recipe[2049]{},OutputItem[2049]{},StationTable[2049]{},StationRow[129]{};
inline char TriggerAsset[2049]{},MapRegion[2049]{},Extra[65537]="{}";
inline int Placement=0,Discovery=0,Trigger=0,RegionId=0;inline bool CreateGroup=false,UseRegionId=false;
inline bool OpenRequested=false;
inline std::string Status;
inline std::future<std::string> Exporter;
inline void SetPage(int page){Page=std::clamp(page,0,int(Pages.size())-1);std::snprintf(Text,sizeof(Text),"%s",Pages[Page].get<std::string>().c_str());}
inline void Template(const Json& row){
    std::snprintf(Title,sizeof(Title),"%s",row.value("Name",std::string{}).c_str());std::snprintf(Image,sizeof(Image),"%s",row.value("Icon",std::string{}).c_str());
    Pages=row.value("Pages",Json::array());if(Pages.empty())Pages=Json::array({row.value("Description",std::string{})});SetPage(0);
    const auto type=row.value("Type",std::string{});Type=type=="People"?1:type=="Place"?2:type=="Treasure"?3:type=="World"?4:0;
    Status="Template copied. Choose a NEW mod-prefixed ID; the original entry and persistence identity are not edited.";
    OpenRequested=true;
    Category[0]=Section[0]=CategoryName[0]=GroupId[0]=GroupName[0]=PlacementKey[0]=Recipe[0]=OutputItem[0]=StationTable[0]=StationRow[0]=TriggerAsset[0]=MapRegion[0]=0;Placement=Discovery=Trigger=0;CreateGroup=UseRegionId=false;std::snprintf(Extra,sizeof(Extra),"{}");
}
inline void Field(const char* label,char* buffer,size_t capacity){ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);ImGui::InputText((std::string("##journal-")+label).c_str(),buffer,capacity);}
inline Json Fields(){
    auto result=Json::parse(Extra);if(!result.is_object())throw std::runtime_error("Advanced schema fields must be a JSON object.");
    if(Placement==1){if(!Section[0]||!CategoryName[0])throw std::runtime_error("Choose a section and category.");result["AddTo"]={{"Section",Section},{"Category",CategoryName}};}
    else if(Category[0])result["AddTo"]={{"SubCategory",Category}};
    if(GroupId[0]){if(!result.contains("AddTo"))throw std::runtime_error("Choose placement before adding a group.");result["AddTo"]["Group"]={{"Id",GroupId},{"CreateIfMissing",CreateGroup}};if(GroupName[0])result["AddTo"]["Group"]["DisplayName"]=GroupName;}
    if(PlacementKey[0]){if(!result.contains("AddTo"))throw std::runtime_error("Placement key requires a category.");result["AddTo"]["Key"]=PlacementKey;}
    result["Unlock"]=Discovery==1;if(Discovery==2)result["UnlockOnAcquire"]=true;
    if(Discovery==3){const char* triggers[]{"PlayerStart","ItemFirstPickup","RecipeUnlocked","Interaction","AIKill","RegionUnlocked","Manual","NeverUnlock"};auto& condition=result["UnlockCondition"];condition=Json{{"UnlockType",triggers[Trigger]}};const char* asset=Trigger==1?"UnlockingItemData":Trigger==2?"UnlockingRecipe":Trigger==3?"UnlockingInteractionActor":Trigger==4?"AIData":"";if(*asset)condition[asset]=TriggerAsset;if(MapRegion[0])condition["MapRegion"]=MapRegion;if(UseRegionId)condition["RegionID"]=RegionId;}
    if(Recipe[0])result["RecipeData"]=Recipe;if(OutputItem[0])result["ItemData"]=OutputItem;if(StationTable[0]||StationRow[0]){if(!StationTable[0]||!StationRow[0])throw std::runtime_error("Station table and row are both required.");result["StationTableRowHandle"]={{"DataTable",StationTable},{"RowName",StationRow}};}return result;
}
inline void Shutdown(){if(Exporter.valid())Exporter.wait();}
inline void Render(){
    if(Exporter.valid()&&Exporter.wait_for(std::chrono::seconds(0))==std::future_status::ready)Status=Exporter.get();
    ImGui::TextWrapped("Create a new RuneSchema journal/lore definition. Export writes a draft only: no installed mod or character save changes.");
    const char* types[]{"Lore","People","Place","Treasure","World","Recipe"};ImGui::Combo("Entry type",&Type,types,6);
    ImGui::TextUnformatted("Unique mod-prefixed ID");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-id",Id,sizeof(Id));
    ImGui::TextUnformatted("Title");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-title",Title,sizeof(Title));
    ImGui::TextUnformatted("Cooked image asset");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-image",Image,sizeof(Image));
    if(ImGui::CollapsingHeader("Placement",ImGuiTreeNodeFlags_DefaultOpen)){
        const char* modes[]{"Native subcategory","Section / category"};ImGui::Combo("Place into",&Placement,modes,2);
        if(Placement==0)Field("Native subcategory",Category,sizeof(Category));else if(ImGui::BeginTable("##journal-placement-fields",2,ImGuiTableFlags_SizingStretchSame)){ImGui::TableNextColumn();Field("Section",Section,sizeof(Section));ImGui::TableNextColumn();Field("Category",CategoryName,sizeof(CategoryName));ImGui::EndTable();}
        if(ImGui::TreeNode("Group options")){Field("Group ID",GroupId,sizeof(GroupId));Field("Group name",GroupName,sizeof(GroupName));ImGui::Checkbox("Create group if missing",&CreateGroup);Field("Placement key",PlacementKey,sizeof(PlacementKey));ImGui::TreePop();}
    }
    if(Type==5&&ImGui::CollapsingHeader("Recipe",ImGuiTreeNodeFlags_DefaultOpen)){Field("Recipe asset",Recipe,sizeof(Recipe));Field("Output item (optional)",OutputItem,sizeof(OutputItem));Field("Station data table (optional)",StationTable,sizeof(StationTable));Field("Station row",StationRow,sizeof(StationRow));}
    if(ImGui::CollapsingHeader("Discovery")){
        const char* modes[]{"Manual / undiscovered","Immediate discovery","On item acquisition","Native condition"};ImGui::Combo("Unlock",&Discovery,modes,4);
        if(Discovery==3){const char* triggers[]{"Player start","First item pickup","Recipe unlocked","Interaction","AI defeated","Region unlocked","Manual","Never"};ImGui::Combo("Trigger",&Trigger,triggers,8);
            if(Trigger>=1&&Trigger<=4)Field(Trigger==1?"Unlocking item":Trigger==2?"Unlocking recipe":Trigger==3?"Interaction actor":"AI data",TriggerAsset,sizeof(TriggerAsset));
            if(Trigger==5){Field("Map region asset",MapRegion,sizeof(MapRegion));ImGui::Checkbox("Specify region ID",&UseRegionId);if(UseRegionId)ImGui::InputInt("Region ID",&RegionId);ImGui::TextDisabled("Region trigger support must be verified in the target game.");}
        }
    }
    ImGui::Text("Page %d / %d",Page+1,int(Pages.size()));
    if(ImGui::Button("Previous page")&&Page>0){Pages[Page]=Text;SetPage(Page-1);}ImGui::SameLine();
    if(ImGui::Button("Next page")&&Page+1<int(Pages.size())){Pages[Page]=Text;SetPage(Page+1);}ImGui::SameLine();
    if(ImGui::Button("Add page")&&Pages.size()<128){Pages[Page]=Text;Pages.push_back("");SetPage(int(Pages.size())-1);}
    if(ImGui::Button("Remove page")&&Pages.size()>1){Pages.erase(Page);SetPage(std::min(Page,int(Pages.size())-1));}
    ImGui::InputTextMultiline("##journal-page",Text,sizeof(Text),{-1,HelpyImGuiTheme::Current.Y(210)});
    if(ImGui::CollapsingHeader("Advanced schema fields")){ImGui::InputTextMultiline("##journal-extra-fields",Extra,sizeof(Extra),{-1,130});ImGui::TextDisabled("Additional schema/native fields · JSON object · draft only");}
    ImGui::BeginDisabled(Exporter.valid());
    if(ImGui::Button("Export new journal/lore draft"))try{Pages[Page]=Text;const auto doc=JournalWorkspace::Draft(Id,types[Type],Title,Pages,Image,"",Fields());
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();const auto file=HostServices::ExportsDirectory()/"journal-drafts"/(std::string(Id)+"-"+std::to_string(stamp)+".json");
        Exporter=std::async(std::launch::async,[file,doc](){try{const auto text=doc.dump(2);HelpyIconPack::Publish(file,{text.begin(),text.end()},false);return "Draft exported: "+file.string();}catch(const std::exception& e){return std::string(e.what());}});
    }catch(const std::exception& e){Status=e.what();}ImGui::EndDisabled();
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
}
}
