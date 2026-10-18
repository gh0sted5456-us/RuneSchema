#pragma once
#include "Generator/AuthoringPanel.h"
#include "Generator/ItemIconCache.h"
#include "Generator/QuickMenuDecorations.h"
#include <commdlg.h>
#pragma comment(lib,"comdlg32.lib")
namespace PS::SaveEditorPanel {
using Json=nlohmann::json;
inline char File[4097]{},Value[8193]{};
inline std::string Pointer,Selected,Token,Error;
inline Json Browser=Json::object(),Review=Json::object();
inline uint64_t Applied=0,Generation=0;
inline bool Offline=false,Confirmed=false,Opened=false;
inline Json Items=Json::object();inline bool InventoryRequested=false,ConfirmRemoval=false;
inline std::set<std::string> ItemSelection;inline std::map<std::string,int> StackDrafts;
inline char ItemSearch[257]{};inline std::string ItemSource;
inline std::map<std::string,std::string> ItemNames;
inline void ResetInventory(){Items=Json::object();ItemSelection.clear();StackDrafts.clear();ItemNames.clear();InventoryRequested=false;ConfirmRemoval=false;}
inline void Invalidate(){Token.clear();Review=Json::object();Confirmed=false;}
inline void Choose(){
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Character JSON payload (*.json)\0*.json\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if(GetOpenFileNameW(&dialog)){const auto text=std::filesystem::path(file).u8string();AuthoringPanel::Copy(File,sizeof(File),std::string(text.begin(),text.end()));Invalidate();}
}
inline bool Button(const char* label,const char* op,const Json& args=Json::object()){return AuthoringPanel::Button(label,op,args);}
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
    if(op=="SaveEditorClose"){Opened=false;Browser=Json::object();Invalidate();ResetInventory();}
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"){
        Items=result;InventoryRequested=true;StackDrafts.clear();ConfirmRemoval=false;
        if(Items.contains("Items"))for(const auto& item:Items["Items"])if(item.contains("DisplayName")&&item["DisplayName"].is_string()&&!item["DisplayName"].get_ref<const std::string&>().empty()&&ItemNames.size()<8192)ItemNames[item.at("PersistenceID").get<std::string>()]=item["DisplayName"].get<std::string>();
        if(op!="SaveEditorInventory"){Invalidate();ItemSelection.clear();Browser=Json::object();Pointer.clear();}
    }
    if(op=="SaveEditorPreview"){Review=result;Token=result.value("Token",std::string{});Confirmed=false;}
    if(op=="SaveEditorExport")Invalidate();
}
inline std::string ItemText(const Json& row,const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};}
inline void Inventory(){
    ImGui::SeparatorText("Inventory");
    if(!InventoryRequested&&!AuthoringClient::Busy()){InventoryRequested=AuthoringClient::Submit("SaveEditorInventory");}
    const auto offset=Items.value("Offset",size_t{}),total=Items.value("Total",size_t{});
    if(Button("Refresh inventory names / sources","SaveEditorInventory",{{"Offset",offset}}))InventoryRequested=true;
    if(!Items.contains("Items")){ImGui::TextDisabled("Reading the saved inventory...");return;}
    ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(235));ImGui::InputTextWithHint("##save-item-search","Search this page: name, ID or source",ItemSearch,sizeof(ItemSearch));ImGui::SameLine();
    std::set<std::string> sources;for(const auto& row:Items["Items"]){const auto source=ItemText(row,"SourceName");sources.insert(source.empty()?"Unresolved":source);}
    if(ImGui::BeginCombo("Source on this page",ItemSource.empty()?"All sources":ItemSource.c_str())){
        if(ImGui::Selectable("All sources",ItemSource.empty()))ItemSource.clear();for(const auto& name:sources)if(ImGui::Selectable(name.c_str(),ItemSource==name))ItemSource=name;ImGui::EndCombo();}
    std::vector<const Json*> visible;auto search=HelpyCacheSources::Lower(ItemSearch);
    for(const auto& row:Items["Items"]){auto source=ItemText(row,"SourceName");if(source.empty())source="Unresolved";
        if(!ItemSource.empty()&&source!=ItemSource)continue;
        const auto text=HelpyCacheSources::Lower(ItemText(row,"DisplayName")+" "+ItemText(row,"PersistenceID")+" "+ItemText(row,"AssetPath")+" "+source);
        if(search.empty()||text.find(search)!=text.npos)visible.push_back(&row);}
    std::stable_sort(visible.begin(),visible.end(),[](const auto* a,const auto* b){return std::pair{ItemText(*a,"SourceName"),ItemText(*a,"DisplayName")}<std::pair{ItemText(*b,"SourceName"),ItemText(*b,"DisplayName")};});
    if(HelpyImGuiTheme::SoftButton("Select visible")){for(const auto* row:visible)if(ItemSelection.size()<128)ItemSelection.insert(row->at("Pointer").get<std::string>());ConfirmRemoval=false;}ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Clear selection")){ItemSelection.clear();ConfirmRemoval=false;}ImGui::SameLine();ImGui::TextDisabled("%zu selected · %zu saved item records",ItemSelection.size(),total);
    if(ImGui::BeginTable("##save-inventory",4,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,HelpyImGuiTheme::Current.Y(245)})){
        ImGui::TableSetupColumn("Select",ImGuiTableColumnFlags_WidthFixed,HelpyImGuiTheme::Current.X(40));ImGui::TableSetupColumn("Item / source");ImGui::TableSetupColumn("Stack",ImGuiTableColumnFlags_WidthFixed,HelpyImGuiTheme::Current.X(120));ImGui::TableSetupColumn("PersistenceID / saved slot");ImGui::TableHeadersRow();
        for(const auto* entry:visible){const auto& row=*entry;const auto pointer=row.at("Pointer").get<std::string>();ImGui::PushID(pointer.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();
            bool selected=ItemSelection.contains(pointer);if(ImGui::Checkbox("##select",&selected)){if(selected&&ItemSelection.size()<128)ItemSelection.insert(pointer);else ItemSelection.erase(pointer);ConfirmRemoval=false;}
            ImGui::TableNextColumn();const auto at=ImGui::GetCursorScreenPos();const float size=HelpyImGuiTheme::Current.Y(30);const auto icon=ItemText(row,"Icon");
            if(!icon.empty())ItemIconCache::Draw(icon,at,{at.x+size,at.y+size},true);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::BeginGroup();
            auto name=ItemText(row,"DisplayName");if(name.empty())name=ItemText(row,"InternalName");if(name.empty())name="Unresolved item";ImGui::TextWrapped("%s",name.c_str());
            auto source=ItemText(row,"SourceName");if(source.empty())source="Unresolved — load its content to identify";ImGui::TextDisabled("%s",source.c_str());
            if(row.value("Equipped",false))ImGui::TextDisabled("Equipped · removing also unlinks its native loadout slot");ImGui::EndGroup();
            if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextWrapped("%s",ItemText(row,"AssetPath").c_str());ImGui::TextWrapped("Instance: %s",row.value("InstanceID",Json{}).dump().c_str());HelpyImGuiTheme::EndDetailsTooltip();}
            ImGui::TableNextColumn();if(row.value("StackEditable",false)){
                auto [draft,added]=StackDrafts.try_emplace(pointer,row.at("Count").get<int>());ImGui::SetNextItemWidth(-1);ImGui::InputInt("##stack",&draft->second,0,0);
                if(HelpyImGuiTheme::SoftButton("Stage count"))if(AuthoringClient::Submit("SaveEditorStack",{{"Pointer",pointer},{"Count",draft->second},{"Revision",Items.at("Revision")},{"Offset",offset}}))Invalidate();
                ImGui::TextDisabled("Native max: %s",row.contains("MaxStackSize")?row.at("MaxStackSize").dump().c_str():"unknown; decrease only");
            }else ImGui::TextDisabled("%s · single/read-only",row.value("Count",Json(1)).dump().c_str());
            ImGui::TableNextColumn();ImGui::TextWrapped("%s",ItemText(row,"PersistenceID").c_str());ImGui::TextDisabled("%s / %s",ItemText(row,"Container").c_str(),ItemText(row,"Slot").c_str());
            ImGui::PopID();
        }ImGui::EndTable();
    }
    ImGui::BeginDisabled(offset==0);if(Button("Previous items","SaveEditorInventory",{{"Offset",offset>=100?offset-100:0}}))ConfirmRemoval=false;ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(offset+100>=total);if(Button("Next items","SaveEditorInventory",{{"Offset",offset+100}}))ConfirmRemoval=false;ImGui::EndDisabled();
    if(!ItemSelection.empty()){
        ImGui::Checkbox("I reviewed the selected item instances for removal",&ConfirmRemoval);ImGui::BeginDisabled(!ConfirmRemoval);
        if(Button("Stage selected removals","SaveEditorRemoveItems",{{"Pointers",Json(ItemSelection)},{"Revision",Items.at("Revision")},{"Confirm",ConfirmRemoval}}))Invalidate();ImGui::EndDisabled();
    }
    ImGui::TextWrapped("Names, icons and sources use currently loaded items. An unresolved ID is not proof that its mod is missing. Changes affect only the draft and require reviewed export.");
    if(Items.contains("MetadataWarning"))ImGui::TextWrapped("%s",Items.at("MetadataWarning").get_ref<const std::string&>().c_str());
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
inline void Render(){
    AuthoringClient::Poll();Apply();const auto& snap=AuthoringClient::Snapshot;
    if(!snap.value("Active",false)){
        ImGui::TextWrapped("Enable RuneSchema authoring support, then activate this session to edit copies. No save is attached automatically.");
        if(HelpyImGuiTheme::SoftButton("Activate save tools"))AuthoringClient::Activate();return;
    }
    ImGui::BeginDisabled(AuthoringClient::Busy());
    if(ImGui::BeginTabBar("Save editor workflows")){
        if(ImGui::BeginTabItem("Edit / export")){Edit();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Clean copy")){AuthoringPanel::ApplyResult();AuthoringPanel::Cleanup();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Mod roots / compatibility")){
            Button("Refresh roots and runtime","SaveEditorList");const auto& list=AuthoringClient::Result("SaveEditorList");
            ImGui::TextWrapped("Runtime: %s",list.value("Runtime",std::string("Waiting for host")).c_str());
            ImGui::TextWrapped("Imported Steam/Epic character JSON remains editable on Game Pass. Attaching/converting requires the runtime's verified native save-provider adapter; it is not enabled in this build. Never copy JSON directly into WGS storage.");
            if(list.contains("Mods"))for(const auto& row:list["Mods"]){ImGui::Separator();ImGui::TextWrapped("%s — /%s",row.at("Mod").get_ref<const std::string&>().c_str(),row.at("Source").get_ref<const std::string&>().c_str());ImGui::TextWrapped("%s",row.at("Root").get_ref<const std::string&>().c_str());ImGui::TextDisabled("%s",row.value("Registered",false)?"Registered source":"Declared source; not yet registered");}
            ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }ImGui::EndDisabled();
    if(!Error.empty())ImGui::TextWrapped("%s",Error.c_str());
    ImGui::TextWrapped("%s",AuthoringClient::Error.empty()?snap.value("Status",std::string{}).c_str():AuthoringClient::Error.c_str());
}
}
