#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/AuthoringPanel.h"
#include "Generator/ItemIconCache.h"
#include "Generator/QuickMenuDecorations.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ItemBayBrowser.h"
#include "Generator/CharacterPreviewPanel.h"
#include "Core/CharacterWorkbench.h"
#include "Core/PlayerStatus.h"
#include "Core/LoadoutEligibility.h"
#include <commdlg.h>
#pragma comment(lib,"comdlg32.lib")
namespace PS::SaveEditorPanel {
using Json=nlohmann::json;
inline char File[4097]{},Value[8193]{};
inline std::string Pointer,Selected,Token,Error;
inline Json Browser=Json::object(),Review=Json::object();
inline uint64_t Applied=0,Generation=0;
inline bool Offline=false,Confirmed=false,Opened=false;
inline bool FollowCurrentCharacter=true;
inline std::string LiveCharacterGuid;
inline std::string LiveCharacterPlatform;
inline std::string ViewedCurrentGuid;
inline std::string ProfileCharacterGuid,ProfileCharacterPlatform;
inline bool ProfileListRequested=false;
inline bool RequestProfileCharacter(const std::string& guid,const std::string& platform){
    const auto id=HelpyPersonalization::Guid(guid);if(id.empty())return false;
    ProfileCharacterGuid=id;ProfileCharacterPlatform=platform;ProfileListRequested=false;FollowCurrentCharacter=false;Error.clear();return true;
}
inline bool WorkflowInitialized=false;
inline ImTextureRef SteamIcon, EpicIcon, XboxIcon;
inline void ProviderIcon(const Json& file){
    const auto provider=file.value("Provider",std::string("Imported"));const float side=HelpyImGuiTheme::Current.Y(20);
    const auto icon=[&](ImTextureRef texture,const char* fallback){if(texture.GetTexID())ImGui::Image(texture,{side,side});else ImGui::TextUnformatted(fallback);ImGui::SameLine();};
    if(provider=="Steam"||provider=="SteamEpic")icon(SteamIcon,"Steam");
    if(provider=="Epic"||provider=="SteamEpic")icon(EpicIcon,"Epic");
    if(provider=="GamePass")icon(XboxIcon,"Xbox");
    if(provider=="Imported")icon({},"JSON");
}
inline Json Items=Json::object();inline bool InventoryRequested=false,ConfirmRemoval=false;
inline std::set<std::string> ItemSelection;inline std::map<std::string,int> StackDrafts;
inline char ItemSearch[257]{};inline std::string ItemSource;
inline std::string InventoryGroup="Inventory";
inline bool InventoryRecordTable=false;
inline bool FavoritesOnly=false;inline std::string PersonalTag;
inline std::string FocusedSavedItem;
inline Json DroppedInventoryItem=Json::object();
inline bool FocusInventoryGrid=false;
#if defined(HELPY_PREVIEW)
inline size_t SavedPlacardsDrawn=0;
#endif
inline std::map<std::string,std::string> ItemNames;
inline void ResetInventory(){Items=Json::object();ItemSelection.clear();StackDrafts.clear();ItemNames.clear();InventoryRequested=false;ConfirmRemoval=false;FocusedSavedItem.clear();}
inline void Invalidate(){Token.clear();Review=Json::object();Confirmed=false;}
inline void Choose(){
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Character JSON payload (*.json)\0*.json\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if(GetOpenFileNameW(&dialog)){const auto text=std::filesystem::path(file).u8string();AuthoringPanel::Copy(File,sizeof(File),std::string(text.begin(),text.end()));Invalidate();}
}
inline bool Button(const char* label,const char* op,const Json& args=Json::object()){
    if(std::string_view(op)=="SaveEditorRemoveItems"&&args.contains("Pointers")){
        bool safe=Items.contains("Items");if(safe)for(const auto& pointer:args.at("Pointers")){const Json* found=nullptr;for(const auto& row:Items["Items"])if(row.at("Pointer")==pointer){found=&row;break;}
            if(!found||HelpyPersonalization::Locked(*found)){safe=false;break;}}
        if(!safe){ImGui::BeginDisabled();AuthoringPanel::Button(label,op,args);ImGui::EndDisabled();ImGui::TextWrapped("A selected item is locked or no longer on the current page. Refresh and review the selection.");return false;}
    }return AuthoringPanel::Button(label,op,args);
}
inline void Browse(const std::string& pointer,size_t offset=0){Selected.clear();AuthoringClient::Submit("SaveEditorBrowse",{{"Pointer",pointer},{"Offset",offset}});}
inline void Apply(){
    const auto& snap=AuthoringClient::Snapshot;const auto generation=snap.value("Generation",uint64_t{});
    if(generation!=Generation){Generation=generation;Applied=0;Browser=Json::object();Opened=false;Invalidate();Selected.clear();ResetInventory();}
    const auto id=snap.value("RequestId",uint64_t{});if(!id||id==Applied)return;Applied=id;
    const auto op=snap.value("Operation",std::string{});const auto& result=AuthoringClient::Result(op.c_str());
    if(result.contains("Error")){if(op=="SaveEditorOpen"){Opened=false;Browser=Json::object();Invalidate();ResetInventory();}return;}
    if(op=="CleanupInspect"){Opened=false;Browser=Json::object();Invalidate();}
    if(op=="SaveEditorOpen"||op=="SaveEditorBrowse"||op=="SaveEditorSet"){
        Browser=result;Pointer=result.value("Pointer",std::string{});Selected.clear();Opened=true;
        if(op=="SaveEditorOpen"||op=="SaveEditorSet")ResetInventory();
        if(op!="SaveEditorBrowse")Invalidate();
    }
    if(op=="SaveEditorOpen")Error.clear();
    if(op=="SaveEditorClose"){Opened=false;Browser=Json::object();Invalidate();ResetInventory();}
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"||op=="SaveEditorAddItem"){
        Items=result;InventoryRequested=true;StackDrafts.clear();ConfirmRemoval=false;
        if(Items.contains("Items"))for(const auto& item:Items["Items"])if(item.contains("DisplayName")&&item["DisplayName"].is_string()&&!item["DisplayName"].get_ref<const std::string&>().empty()&&ItemNames.size()<8192)ItemNames[item.at("PersistenceID").get<std::string>()]=item["DisplayName"].get<std::string>();
        if(op!="SaveEditorInventory"){Invalidate();ItemSelection.clear();Browser=Json::object();Pointer.clear();}
    }
    if(op=="SaveEditorPreview"){Review=result;Token=result.value("Token",std::string{});Confirmed=false;}
    if(op=="SaveEditorExport")Invalidate();
}
inline std::string ItemText(const Json& row,const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};}
inline Json SavedPlacard(const Json& record){
    auto row=record;const auto path=ItemText(record,"AssetPath");row["Path"]=path;
    // Native save resolution supplies icons/source; DrawPlacard also reuses
    // cached details by canonical path. Never scan the whole Item Bay per card.
    row["Name"]=ItemText(record,"DisplayName");if(row["Name"].get_ref<const std::string&>().empty())row["Name"]=ItemText(record,"InternalName");if(row["Name"].get_ref<const std::string&>().empty())row["Name"]="Unresolved saved item";return row;
}
inline void InventoryPlacards(const std::vector<const Json*>& visible,bool compact,size_t offset){
    const auto& theme=HelpyImGuiTheme::Current;
    if(FocusInventoryGrid){ImGui::SetScrollHereY(.1f);FocusInventoryGrid=false;}
#if defined(HELPY_PREVIEW)
    SavedPlacardsDrawn=0;
#endif
    if(ImGui::BeginChild("##saved-inventory-placards",{0,theme.Y(compact?420:300)},true)){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::InventoryViewport=HelpyPreviewHooks::WindowRect();
#endif
        const auto dropMin=ImGui::GetWindowPos(),dropSize=ImGui::GetWindowSize();
        const float gap=ImGui::GetStyle().ItemSpacing.x,available=std::max(1.f,ImGui::GetContentRegionAvail().x),minimum=theme.X(theme.gridMinCardWidth);
        const int columns=ItemBayLayout::Columns(available,minimum,gap,theme.itemGridColumns);const float width=std::max(1.f,(available-gap*(columns-1))/columns),height=theme.Y(theme.gridCardHeight);
        ImGuiListClipper clipper;clipper.Begin(int((visible.size()+columns-1)/columns),height+ImGui::GetStyle().ItemSpacing.y);
        while(clipper.Step())for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;++line)for(int column=0;column<columns;++column){const auto index=size_t(line*columns+column);if(index>=visible.size())break;const auto& saved=*visible[index];const auto pointer=saved.at("Pointer").get<std::string>();const auto row=SavedPlacard(saved);const auto path=ItemText(row,"Path"),name=ItemPresentation::Name(row);
            ImGui::PushID(pointer.c_str());const auto a=ImGui::GetCursorScreenPos();const ImVec2 b(a.x+width,a.y+height);const bool clicked=ImGui::InvisibleButton("##saved-item-card",{width,height}),hovered=ImGui::IsItemHovered();
            if(clicked){FocusedSavedItem=pointer;if(!ImGui::GetIO().KeyCtrl)ItemSelection.clear();if(!ItemSelection.erase(pointer)&&ItemSelection.size()<128&&!HelpyPersonalization::Locked(row))ItemSelection.insert(pointer);ConfirmRemoval=false;}
            ItemGridPicker::DrawPlacard(row,a,b,ItemSelection.contains(pointer),hovered);
#if defined(HELPY_PREVIEW)
            ++SavedPlacardsDrawn;
            if(SavedPlacardsDrawn==1)HelpyPreviewHooks::SavedItemRect=HelpyPreviewHooks::LastRect();
#endif
            auto* draw=ImGui::GetWindowDrawList();const float footer=std::min(theme.Y(42),height*.42f);const auto count="×"+saved.value("Count",Json(1)).dump();const auto extent=ImGui::CalcTextSize(count.c_str());const ImVec2 quantity(b.x-theme.X(6)-extent.x,b.y-footer-theme.Y(5)-extent.y);
            draw->AddRectFilled({quantity.x-theme.X(2),quantity.y},{b.x-theme.X(4),quantity.y+extent.y},ImGui::GetColorU32(theme.panelAlt),theme.X(2));draw->AddText(quantity,ImGui::GetColorU32(theme.text),count.c_str());
            if(saved.value("Equipped",false))draw->AddText({a.x+theme.X(25),b.y-footer-theme.Y(5)-ImGui::GetTextLineHeight()},ImGui::GetColorU32(theme.accent),"Equipped");
            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)){ImGui::SetDragDropPayload("RS_SAVE_ITEM",pointer.c_str(),pointer.size()+1);ItemGridPicker::HeldItem(row,"Saved item");ImGui::EndDragDropSource();}
            if(ImGui::BeginPopupContextItem("##saved-item-actions")){PersonalizationPanel::Menu(row);ImGui::TextWrapped("%s",name.c_str());if(ImGui::MenuItem("Preview",nullptr,false,ItemPreviewRequest::Valid(row)))ItemPreviewRequest::Show(row);if(ImGui::MenuItem("Select / edit this saved instance")){FocusedSavedItem=pointer;ItemSelection={pointer};ConfirmRemoval=false;}if(ImGui::MenuItem("Show item details",nullptr,false,!path.empty()))ItemGridPicker::PinDetails(row);ImGui::TextDisabled("Removal requires review below.");ImGui::EndPopup();}
            if(HelpyImGuiTheme::TooltipReady("saved-item:"+pointer,hovered))ItemGridPicker::VerboseTooltip(row,name,path,false,true);
            ImGui::PopID();if(column+1<columns&&index+1<visible.size())ImGui::SameLine();
        }
        if(visible.empty())ImGui::TextWrapped("No saved items in this tab on the current page.");
        if(ImGui::BeginDragDropTargetCustom(ImRect(dropMin,{dropMin.x+dropSize.x-ImGui::GetStyle().ScrollbarSize,dropMin.y+dropSize.y}),ImGui::GetID("##saved-inventory-drop"))){if(auto row=ItemGridPicker::AcceptItemDrop(Error))DroppedInventoryItem=*row;ImGui::EndDragDropTarget();}
    }ImGui::EndChild();
    if(!FocusedSavedItem.empty())for(const auto* entry:visible)if(entry->at("Pointer")==FocusedSavedItem){const auto& row=*entry;ImGui::SeparatorText("Selected saved instance");ImGui::TextWrapped("%s",ItemPresentation::Name(SavedPlacard(row)).c_str());
        if(row.value("StackEditable",false)){auto [draft,added]=StackDrafts.try_emplace(FocusedSavedItem,row.at("Count").get<int>());ImGui::SetNextItemWidth(100);ImGui::InputInt("Count",&draft->second,0,0);if(HelpyImGuiTheme::SoftButton("Stage count"))if(AuthoringClient::Submit("SaveEditorStack",{{"Pointer",FocusedSavedItem},{"Count",draft->second},{"Revision",Items.at("Revision")},{"Offset",offset}}))Invalidate();}else ImGui::TextWrapped("Single item · no editable native stack count");break;}
}
inline void Inventory(bool compact=false){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::InventorySearchId=ImGui::GetID("Search");HelpyPreviewHooks::InventoryRefreshId=ImGui::GetID("Refresh");
#endif
    ImGui::SeparatorText("Inventory");
    if(ImGui::BeginTabBar("##native-inventory-tabs",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_Reorderable)){for(const auto* group:CharacterWorkbench::Groups)if(ImGui::BeginTabItem(group)){if(InventoryGroup!=group){InventoryGroup=group;ItemSelection.clear();}ImGui::EndTabItem();}ImGui::EndTabBar();}
    if(!InventoryRequested&&!AuthoringClient::Busy()){InventoryRequested=AuthoringClient::Submit("SaveEditorInventory");}
    const auto offset=Items.value("Offset",size_t{}),total=Items.value("Total",size_t{});
    if(!compact&&Button("Refresh inventory names / sources","SaveEditorInventory",{{"Offset",offset}}))InventoryRequested=true;
    if(!Items.contains("Items")){ImGui::TextDisabled("Reading the saved inventory...");return;}
    ImGui::SetNextItemWidth(compact?std::max(60.f,ImGui::GetContentRegionAvail().x-HelpyImGuiTheme::Current.X(135)):HelpyImGuiTheme::Current.X(235));ImGui::InputTextWithHint("##save-item-search","Search inventory…",ItemSearch,sizeof(ItemSearch));
    if(compact){ImGui::SameLine();HelpyImGuiTheme::SoftButton("Search");ImGui::SameLine();if(Button("Refresh","SaveEditorInventory",{{"Offset",offset}}))InventoryRequested=true;}else ImGui::SameLine();
    std::set<std::string> sources;for(const auto& row:Items["Items"]){const auto source=ItemText(row,"SourceName");sources.insert(source.empty()?"Unresolved":source);}
    if(compact)ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo(compact?"##source-on-page":"Source on this page",ItemSource.empty()?"All sources":ItemSource.c_str())){
        if(ImGui::Selectable("All sources",ItemSource.empty()))ItemSource.clear();for(const auto& name:sources)if(ImGui::Selectable(name.c_str(),ItemSource==name))ItemSource=name;ImGui::EndCombo();}
    PersonalizationPanel::Filters(FavoritesOnly,PersonalTag);
    std::vector<const Json*> visible;auto search=HelpyCacheSources::Lower(ItemSearch);
    for(const auto& row:Items["Items"]){auto source=ItemText(row,"SourceName");if(source.empty())source="Unresolved";
        if(InventoryGroup=="Equipped"?!row.value("Equipped",false):row.value("Group",CharacterWorkbench::InventoryGroup(row))!=InventoryGroup)continue;
        if(!ItemSource.empty()&&source!=ItemSource)continue;
        if(!PersonalizationPanel::Matches(row,FavoritesOnly,PersonalTag))continue;
        const auto text=HelpyCacheSources::Lower(ItemText(row,"DisplayName")+" "+HelpyPersonalization::SearchText(row)+" "+ItemText(row,"PersistenceID")+" "+ItemText(row,"AssetPath")+" "+source);
        if(search.empty()||text.find(search)!=text.npos)visible.push_back(&row);}
    std::stable_sort(visible.begin(),visible.end(),[](const auto* a,const auto* b){return std::pair{ItemText(*a,"SourceName"),ItemText(*a,"DisplayName")}<std::pair{ItemText(*b,"SourceName"),ItemText(*b,"DisplayName")};});
    if(!compact){if(HelpyImGuiTheme::SoftButton("Select visible")){for(const auto* row:visible)if(ItemSelection.size()<128&&!HelpyPersonalization::Locked(*row))ItemSelection.insert(row->at("Pointer").get<std::string>());ConfirmRemoval=false;}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Clear selection")){ItemSelection.clear();ConfirmRemoval=false;}}
    const auto selectRemoval=[&](bool junk){ItemSelection.clear();ConfirmRemoval=false;for(const auto* row:visible)if(HelpyPersonalization::BulkEligible(*row,junk))ItemSelection.insert(row->at("Pointer").get<std::string>());};
    if(HelpyImGuiTheme::SoftButton("Remove Junk..."))selectRemoval(true);if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Select junk on this filtered page for reviewed draft removal. Skips locked, favorite, equipped and unresolved items.");
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Remove All..."))selectRemoval(false);if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Select eligible items on this filtered page, not unseen pages. Nothing is removed until confirmed and staged; export is separate.");
    ImGui::TextDisabled("%zu items · %zu selected",visible.size(),ItemSelection.size());
    if(!compact)ImGui::Checkbox("Advanced record table",&InventoryRecordTable);
    if(!InventoryRecordTable)InventoryPlacards(visible,compact,offset);
    else if(ImGui::BeginTable("##save-inventory",compact?2:4,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,HelpyImGuiTheme::Current.Y(245)})){
        if(!compact)ImGui::TableSetupColumn("Select",ImGuiTableColumnFlags_WidthFixed,HelpyImGuiTheme::Current.X(40));ImGui::TableSetupColumn("Item / source");ImGui::TableSetupColumn("Stack",ImGuiTableColumnFlags_WidthFixed,HelpyImGuiTheme::Current.X(compact?88:120));if(!compact)ImGui::TableSetupColumn("PersistenceID / saved slot");ImGui::TableHeadersRow();
        for(const auto* entry:visible){const auto& row=*entry;const auto pointer=row.at("Pointer").get<std::string>();ImGui::PushID(pointer.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();
            bool selected=ItemSelection.contains(pointer);ImGui::BeginDisabled(HelpyPersonalization::Locked(row));if(ImGui::Checkbox("##select",&selected)){if(selected&&ItemSelection.size()<128)ItemSelection.insert(pointer);else ItemSelection.erase(pointer);ConfirmRemoval=false;}ImGui::EndDisabled();
            if(compact)ImGui::SameLine();else ImGui::TableNextColumn();const auto at=ImGui::GetCursorScreenPos();const float size=HelpyImGuiTheme::Current.Y(30);const auto icon=ItemText(row,"Icon");
            if(!icon.empty())ItemIconCache::Draw(icon,at,{at.x+size,at.y+size},true);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::BeginGroup();
            auto name=ItemText(row,"DisplayName");if(name.empty())name=ItemText(row,"InternalName");if(name.empty())name="Unresolved item";ImGui::TextWrapped("%s",name.c_str());
            auto source=ItemText(row,"SourceName");if(source.empty())source="Unresolved — load its content to identify";ImGui::TextWrapped("%s",source.c_str());
            if(row.value("Equipped",false))ImGui::TextWrapped("Equipped · removing also unlinks its native loadout slot");ImGui::EndGroup();
            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID|ImGuiDragDropFlags_SourceNoPreviewTooltip)){ImGui::SetDragDropPayload("RS_SAVE_ITEM",pointer.c_str(),pointer.size()+1);ItemGridPicker::HeldItem(row,"Saved item");ImGui::EndDragDropSource();}
            if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextWrapped("%s",ItemText(row,"AssetPath").c_str());ImGui::TextWrapped("Persistence ID: %s",ItemText(row,"PersistenceID").c_str());ImGui::TextWrapped("Saved slot: %s / %s",ItemText(row,"Container").c_str(),ItemText(row,"Slot").c_str());ImGui::TextWrapped("Instance: %s",row.value("InstanceID",Json{}).dump().c_str());HelpyImGuiTheme::EndDetailsTooltip();}
            ImGui::TableNextColumn();if(row.value("StackEditable",false)){
                auto [draft,added]=StackDrafts.try_emplace(pointer,row.at("Count").get<int>());ImGui::SetNextItemWidth(-1);ImGui::InputInt("##stack",&draft->second,0,0);
                ImGui::BeginDisabled(HelpyPersonalization::Locked(row));if(HelpyImGuiTheme::SoftButton("Stage count"))if(AuthoringClient::Submit("SaveEditorStack",{{"Pointer",pointer},{"Count",draft->second},{"Revision",Items.at("Revision")},{"Offset",offset}}))Invalidate();ImGui::EndDisabled();
                ImGui::TextWrapped("Max: %s",row.contains("MaxStackSize")?row.at("MaxStackSize").dump().c_str():"unknown; decrease only");
            }else ImGui::TextWrapped("%s · single/read-only",row.value("Count",Json(1)).dump().c_str());
            if(!compact){ImGui::TableNextColumn();ImGui::TextWrapped("%s",ItemText(row,"PersistenceID").c_str());ImGui::TextDisabled("%s / %s",ItemText(row,"Container").c_str(),ItemText(row,"Slot").c_str());}
            ImGui::PopID();
        }ImGui::EndTable();
    }
    ImGui::BeginDisabled(offset==0);if(Button("Previous items","SaveEditorInventory",{{"Offset",offset>=100?offset-100:0}}))ConfirmRemoval=false;ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(offset+100>=total);if(Button("Next items","SaveEditorInventory",{{"Offset",offset+100}}))ConfirmRemoval=false;ImGui::EndDisabled();
    if(!ItemSelection.empty()){
        ImGui::Checkbox("I reviewed the selected item instances for removal",&ConfirmRemoval);ImGui::BeginDisabled(!ConfirmRemoval);
        if(Button("Stage selected removals","SaveEditorRemoveItems",{{"Pointers",Json(ItemSelection)},{"Revision",Items.at("Revision")},{"Confirm",ConfirmRemoval}}))Invalidate();ImGui::EndDisabled();
    }
    HelpyImGuiTheme::SoftButton("Drop saved item here to select it for deletion");
    if(ImGui::BeginDragDropTarget()){if(const auto* drop=ImGui::AcceptDragDropPayload("RS_SAVE_ITEM"))if(drop->DataSize>1&&drop->DataSize<4097&&static_cast<const char*>(drop->Data)[drop->DataSize-1]=='\0'){ItemSelection.insert(static_cast<const char*>(drop->Data));ConfirmRemoval=false;}ImGui::EndDragDropTarget();}
    if(!compact)ImGui::TextWrapped("Names, icons and sources use currently loaded items. An unresolved ID is not proof that its mod is missing. Changes affect only the draft and require reviewed export.");
    if(Items.contains("MetadataWarning"))ImGui::TextWrapped("%s",Items.at("MetadataWarning").get_ref<const std::string&>().c_str());
    if(Items.contains("LayoutWarning"))ImGui::TextWrapped("%s",Items.at("LayoutWarning").get_ref<const std::string&>().c_str());
}
inline void Edit(){
    if(Button("Refresh saves and mod roots","SaveEditorList"))Error.clear();ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Import JSON..."))Choose();
    const auto& list=AuthoringClient::Result("SaveEditorList");
    if(ImGui::BeginCombo("Available character saves",File[0]?File:"Select a character JSON")){
        if(list.contains("Files"))for(const auto& entry:list["Files"]){const auto path=entry.at("Path").get<std::string>();
            ImGui::PushID(path.c_str());if(ImGui::Selectable((entry.at("Name").get<std::string>()+" — "+entry.at("Storage").get<std::string>()).c_str())){AuthoringPanel::Copy(File,sizeof(File),path);Invalidate();}ImGui::PopID();}
        ImGui::EndCombo();
    }
    if(ImGui::InputText("Character JSON path",File,sizeof(File)))Invalidate();
    ImGui::Checkbox("This character is not currently loaded",&Offline);
    ImGui::BeginDisabled(!Offline);Button("Open editable copy","SaveEditorOpen",{{"Path",File},{"Offline",Offline}});ImGui::EndDisabled();
    ImGui::TextWrapped("Copy-only editing: original files, identities, ownership and unknown fields stay intact. World/SPUD and opaque WGS containers are not supported yet.");
    if(!Opened)return;
    Inventory();
    if(ImGui::CollapsingHeader("Advanced: raw character fields")){
    ImGui::SeparatorText("Character fields");ImGui::TextWrapped("%s",Pointer.empty()?"/":Pointer.c_str());
    if(HelpyImGuiTheme::SoftButton("Root"))Browse("");ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Up"))Browse(Pointer.substr(0,Pointer.find_last_of('/')));
    const auto offset=Browser.value("Offset",size_t{}),total=Browser.value("Total",size_t{});
    ImGui::BeginDisabled(offset==0);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Previous fields"))Browse(Pointer,offset>=100?offset-100:0);ImGui::EndDisabled();
    ImGui::BeginDisabled(offset+100>=total);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Next fields"))Browse(Pointer,offset+100);ImGui::EndDisabled();
    if(ImGui::BeginTable("##save-fields",3,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,260})){
        ImGui::TableSetupColumn("Field");ImGui::TableSetupColumn("Value / type");ImGui::TableSetupColumn("Action");ImGui::TableHeadersRow();
        if(Browser.contains("Rows"))for(const auto& row:Browser["Rows"]){const auto pointer=row.at("Pointer").get<std::string>();ImGui::PushID(pointer.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(row.at("Key").get_ref<const std::string&>().c_str());ImGui::TableNextColumn();
            const auto type=row.at("Type").get<std::string>();const auto text=row.contains("Value")?row["Value"].dump():type+" ("+std::to_string(row.value("Children",size_t{}))+")";
            ImGui::TextWrapped("%s",text.c_str());ImGui::TableNextColumn();
            if(type=="object"||type=="array"){if(HelpyImGuiTheme::SoftButton("Browse"))Browse(pointer);}
            else if(row.value("Editable",false)&&row.contains("Value")){if(HelpyImGuiTheme::SoftButton("Edit")){Selected=pointer;AuthoringPanel::Copy(Value,sizeof(Value),row["Value"].dump());}}
            else ImGui::TextDisabled("Read-only");ImGui::PopID();
        }ImGui::EndTable();
    }
    if(!Selected.empty()){
        ImGui::TextWrapped("Editing %s",Selected.c_str());ImGui::InputTextMultiline("JSON value",Value,sizeof(Value),{-1,65});
        if(HelpyImGuiTheme::SoftButton("Stage field edit")){try{auto value=Json::parse(Value);if(AuthoringClient::Submit("SaveEditorSet",{{"Pointer",Selected},{"Value",value},{"Revision",Browser.at("Revision")}})){Invalidate();Error.clear();}}catch(const std::exception& e){Error=e.what();}}
    }
    }
    if(Button("Preview edited copy","SaveEditorPreview"))Invalidate();ImGui::SameLine();Button("Discard draft","SaveEditorClose");
    if(!Token.empty()){
        ImGui::SeparatorText("Review changes");
        if(Review.contains("Summary"))for(const auto& row:Review["Summary"]){ImGui::TextWrapped("%s",row.at("Label").get_ref<const std::string&>().c_str());
            if(row.contains("Item")){const auto id=row.at("Item").get<std::string>();ImGui::TextWrapped("Item: %s",ItemNames.contains(id)?ItemNames.at(id).c_str():id.c_str());}
            ImGui::TextDisabled("%s",row.at("Field").get_ref<const std::string&>().c_str());
            if(row.contains("Before")||row.contains("After"))ImGui::TextWrapped("%s → %s",row.value("Before",Json{}).dump().c_str(),row.value("After",Json{}).dump().c_str());}
        if(ImGui::CollapsingHeader("Raw JSON patch"))AuthoringPanel::TextJson(Review.at("Changes"),"##save-editor-diff",140);
        ImGui::Checkbox("I reviewed these changes; export a new copy",&Confirmed);ImGui::BeginDisabled(!Confirmed);
        Button("Export verified copy","SaveEditorExport",{{"Token",Token},{"Confirm",Confirmed}});ImGui::EndDisabled();
    }
    const auto& exported=AuthoringClient::Result("SaveEditorExport");if(exported.contains("File"))ImGui::TextWrapped("Export: %s",exported.at("File").get_ref<const std::string&>().c_str());
}
#include "Generator/CharacterStudioPanel.inl"
inline void Render(const Json& catalog=Json::object(),uint64_t catalogRevision=0){
    AuthoringClient::Poll();Apply();const auto& snap=AuthoringClient::Snapshot;
    const bool active=snap.value("Active",false);const auto* local=PlayerStatus::LocalCharacter(catalog);
    LiveCharacterGuid=local?PlayerStatus::Cache::Guid(local->at("Guid").get<std::string>()):std::string{};
    LiveCharacterPlatform=local?local->value("Platform",std::string{}):std::string{};
    const bool selectCharacter=!WorkflowInitialized||(FollowCurrentCharacter&&!LiveCharacterGuid.empty()&&LiveCharacterGuid!=ViewedCurrentGuid);WorkflowInitialized=true;ViewedCurrentGuid=LiveCharacterGuid;
    const auto activate=[](){ImGui::TextWrapped("Activate save tools to edit offline character copies.");if(HelpyImGuiTheme::SoftButton("Activate save tools"))AuthoringClient::Activate();};
    if(local){ProviderIcon(Json{{"Provider",local->value("Platform",std::string{})=="Game Pass"?"GamePass":local->value("Platform",std::string{})}});ImGui::Text("Current character: %s",local->value("Name",std::string("Player")).c_str());}
    StudioApply();
    if(!ProfileCharacterGuid.empty()){
        if(!active)AuthoringClient::EnsureActive();
        else if(!AuthoringClient::Busy()){
            if(!ProfileListRequested){ProfileListRequested=AuthoringClient::Submit("SaveEditorList");}
            else {
                if(const auto* match=StudioCurrentSave(AuthoringClient::Result("SaveEditorList"),ProfileCharacterGuid,ProfileCharacterPlatform))StudioSelectSave(match->at("Path").get<std::string>(),match->at("Name").get<std::string>(),ProfileCharacterGuid);
                else Error="No unique local save found for this character/platform. Choose a save or import a JSON copy.";
                ProfileCharacterGuid.clear();ProfileCharacterPlatform.clear();
            }
        }
    }
    if(local&&FollowCurrentCharacter&&!active)AuthoringClient::EnsureActive();
    if(active)StudioHeader();
    if(ImGui::BeginTabBar("Save editor workflows",ImGuiTabBarFlags_Reorderable)){
        if(ImGui::BeginTabItem("Appearance",nullptr,selectCharacter?ImGuiTabItemFlags_SetSelected:0)){if(active){Studio(catalog,catalogRevision);}else activate();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Inventory")){if(active){Studio(catalog,catalogRevision,true);}else activate();ImGui::EndTabItem();}
        if(active&&ImGui::BeginTabItem("Export copy")){
            ImGui::SeparatorText("1  Choose source");ImGui::TextWrapped("%s",File[0]?File:"Choose a character save above.");
            ImGui::SeparatorText("2  Choose destination");
            ImGui::TextUnformatted("Steam / Epic · verified character JSON copy");
            ImGui::BeginDisabled();ImGui::Button("Xbox / Game Pass · provider adapter unavailable");ImGui::EndDisabled();
            ImGui::TextWrapped("A JSON export is not a WGS conversion. Game Pass attachment is unavailable until its save-provider adapter is verified. Never place JSON directly into WGS storage.");
            ImGui::SeparatorText("3  Review and export");if(Opened)StudioReview();else ImGui::TextDisabled("Open an offline character copy first.");ImGui::EndTabItem();}
        if(active&&HelpyImGuiTheme::Current.advancedMode&&ImGui::BeginTabItem("Advanced / export")){Edit();ImGui::EndTabItem();}
        if(active&&HelpyImGuiTheme::Current.advancedMode&&ImGui::BeginTabItem("Clean copy")){AuthoringPanel::ApplyResult();AuthoringPanel::Cleanup();ImGui::EndTabItem();}
        if(active&&HelpyImGuiTheme::Current.advancedMode&&ImGui::BeginTabItem("Mod roots / compatibility")){
            Button("Refresh roots and runtime","SaveEditorList");const auto& list=AuthoringClient::Result("SaveEditorList");
            ImGui::TextWrapped("Runtime: %s",list.value("Runtime",std::string("Waiting for host")).c_str());
            ImGui::TextWrapped("Imported Steam/Epic character JSON remains editable on Game Pass. Attaching/converting requires the runtime's verified native save-provider adapter; it is not enabled in this build. Never copy JSON directly into WGS storage.");
            if(list.contains("Mods"))for(const auto& row:list["Mods"]){ImGui::Separator();ImGui::TextWrapped("%s — /%s",row.at("Mod").get_ref<const std::string&>().c_str(),row.at("Source").get_ref<const std::string&>().c_str());ImGui::TextWrapped("%s",row.at("Root").get_ref<const std::string&>().c_str());ImGui::TextDisabled("%s",row.value("Registered",false)?"Registered source":"Declared source; not yet registered");}
            ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    if(!Error.empty())ImGui::TextWrapped("%s",Error.c_str());
    ImGui::TextWrapped("%s",AuthoringClient::Error.empty()?snap.value("Status",std::string{}).c_str():AuthoringClient::Error.c_str());
}
}
