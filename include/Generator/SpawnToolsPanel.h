#pragma once
#include "Generator/ToolRequest.h"
#include "Runtime/HelpySettings.h"
#include "Generator/ItemCloneRequest.h"
#include <cstdio>
#include <cstdint>
#include "Generator/ItemGridPicker.h"
#include <algorithm>
#include <cctype>
#include <random>
#include <vector>
#include <imgui.h>
namespace PS::SpawnToolsPanel {
inline void Render() {
    static std::string player,definition;
    static char ai[1024]{},name[257]{},definitionFilter[257]{},itemFilter[257]{},dropItemFilter[257]{},buildingFilter[257]{},selectedBuilding[1024]{};
    static int count=1,kind=0;static float distance=5,scale=1,yaw=0,height=0;static bool custom=false,boss=false,resource=false,confirmed=false;
    static bool grid=false,overrideGhost=false,ghost=false,overrideDrops=false;
    static int rows=2,columns=2,dropMin=1,dropMax=1;static float spacing=5,chance=100;
    static char item[1024]{},selectedItem[1024]{},cloneName[257]{},cloneIcon[2049]{},cloneFilter[257]{},cloneEditor[65536]="{}";
    static bool cloneGive=true,cloneAck=false;
    static std::string cloneError,cloneStatus,cloneSoft;
    static nlohmann::json cloneSnapshot=nlohmann::json::object();
    static uint64_t cloneSequence=(uint64_t{1}<<63),clonePending=0,seenGeneration=0;
    static int itemCount=1,itemAction=0,buildingTime=0;static bool rebuildCatalogs=false,cloneReady=false,allowDeconstruction=false;
    static ItemGridPicker::State itemPickerState{};
    static ItemGridPicker::State dropItemPickerState{0,25,{}};
    static nlohmann::json drops=nlohmann::json::array();
    const auto result=SpawnToolRequests::Read();
    const auto generation=SpawnToolRequests::Generation.load();
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
    static unsigned cacheAttempts=0;
    if(result.value("Status",std::string{}).starts_with("World changed"))cacheAttempts=0;
    if(SpawnToolRequests::Available.load() && !SpawnToolRequests::Waiting.load()
        && !result.value("CatalogReady",false) && cacheAttempts++==0)
        SpawnToolRequests::Submit({{"Action","Catalog"}});
    if(ImGui::CollapsingHeader("RuneSchema Helpy")) {
        HelpySettings::EnsureLoaded();
        const auto activeKey=HelpyHotkeys::Name();
        if(ImGui::BeginCombo("Open / close hotkey##helpy",activeKey.c_str())) {
            for(const auto& key:HelpyHotkeys::Choices())if(ImGui::Selectable(key.name.c_str(),key.name==activeKey))HelpySettings::SetKey(key.name);
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Choose a key not used by the game or another mod. Helpy settings are stored in RuneSchema/settings/settings.jsonc. The saved RSDW index is reused without automatic expiration.");
        const auto message=HelpySettings::Message();ImGui::TextWrapped("%s",message.c_str());
        ImGui::BeginDisabled(!SpawnToolRequests::Available.load()||SpawnToolRequests::Waiting.load()||result.value("_CatalogIndexing",false));
        if(ImGui::Button("Update RSDW reference index##helpy"))try {submitClone({{"Action","UpdateHelpyReference"}});}catch(const std::exception& e){cloneError=e.what();}
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("Authority-side spawn authoring (including dedicated UE4SS Settings). Creates temporary actors, or installs permanent placements when checked. Normal applicable quest credit and drops apply; specific quest/event spawn identities are not impersonated.");
    if(ImGui::BeginTabBar("SpawnRosterKinds")) {
        if(ImGui::BeginTabItem("AI")){if(kind!=0){kind=0;resource=false;definition.clear();confirmed=false;}ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Resources")){if(kind!=1){kind=1;resource=true;boss=false;definition.clear();confirmed=false;}ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Items")){if(kind!=2){kind=2;definition.clear();confirmed=false;}ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Buildings")){if(kind!=3){kind=3;definition.clear();confirmed=false;}ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
    if(ImGui::Button("Load saved catalogs / refresh players")){confirmed=false;SpawnToolRequests::Submit({{"Action","Catalog"}});}
    ImGui::SameLine();ImGui::TextDisabled("Uses runtime/live/saved catalog data without rescanning assets.");
    ImGui::Checkbox("I understand catalog refresh loads candidate AI/resource/building assets once",&rebuildCatalogs);
    ImGui::BeginDisabled(!rebuildCatalogs);
    if(ImGui::Button("Build or overwrite AI, resource, item and building catalogs")) {
        confirmed=false;rebuildCatalogs=false;SpawnToolRequests::Submit({{"Action","RebuildCatalogs"}});
    }
    ImGui::EndDisabled();
    auto label=std::string("Select player");
    if(result.contains("Players"))for(const auto& row:result["Players"])if(row["Path"]==player)label=row["Name"];
    if(ImGui::BeginCombo("Player##spawn",label.c_str())) {
        if(result.contains("Players"))for(const auto& row:result["Players"]) {
            const auto path=row["Path"].get<std::string>();ImGui::PushID(path.c_str());
            if(ImGui::Selectable(row["Name"].get_ref<const std::string&>().c_str(),player==path)){player=path;confirmed=false;}
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if(kind==2) {
        if(ImGui::BeginTabBar("ItemSpawnSubtabs")) {
            if(ImGui::BeginTabItem("Spawn item")) {
                ItemGridPicker::Render("inventory",result,itemFilter,sizeof(itemFilter),selectedItem,sizeof(selectedItem),itemPickerState);
                ImGui::InputInt("Quantity",&itemCount);itemCount=std::clamp(itemCount,1,10000);
                ImGui::RadioButton("Give to selected player's inventory",&itemAction,0);
                ImGui::RadioButton("Spawn pickup at selected player",&itemAction,1);
                if(itemAction==1)ImGui::TextWrapped("World pickup creation remains blocked until the native ownership/replication contract is verified.");
                ImGui::BeginDisabled(player.empty() || selectedItem[0]!='/' || itemAction!=0 || SpawnToolRequests::Waiting.load());
                if(ImGui::Button("Run item command"))SpawnToolRequests::Submit({{"Action","GiveItem"},{"Player",player},{"Item",selectedItem},{"Count",itemCount}});
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Clone creator (Experimental)")) {
                ImGui::TextWrapped("Single-player experiment. Create a distinct item from a loaded source. This does not edit the source; native save/reload safety is NOT established.");
                const auto picker=ItemGridPicker::Render("clone-source",result,itemFilter,sizeof(itemFilter),selectedItem,sizeof(selectedItem),itemPickerState);
                if(picker.Changed || (cloneReady && cloneSnapshot.value("Source",std::string{})!=selectedItem)) {
                    cloneReady=false;cloneAck=false;cloneSnapshot=nlohmann::json::object();cloneSoft.clear();
                    cloneError.clear();cloneStatus.clear();
                    std::snprintf(cloneName,sizeof(cloneName),"%s",(picker.SelectedTitle+" Clone").c_str());
                    cloneIcon[0]=0;std::snprintf(cloneEditor,sizeof(cloneEditor),"{}");
                }
                ImGui::BeginDisabled(player.empty() || selectedItem[0]!='/' || clonePending || SpawnToolRequests::Waiting.load());
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
                ImGui::InputTextWithHint("Find item setting","name or property type",cloneFilter,sizeof(cloneFilter));
                if(cloneReady && ImGui::BeginChild("CloneReflectedFields",ImVec2(0,220),true)) {
                    auto lower=[](std::string value){std::ranges::transform(value,value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;};
                    const auto filter=lower(cloneFilter);
                    for(const auto& field:cloneSnapshot.at("Fields")) {
                        const auto key=field.at("Name").get<std::string>(),type=field.at("Type").get<std::string>();
                        if(!filter.empty() && lower(key+" "+type).find(filter)==std::string::npos)continue;
                        const bool editable=field.value("Editable",false),hasValue=field.value("HasValue",false);
                        const auto label=key+" ("+type+") = "+(hasValue?field["Value"].dump():"[inherited]");
                        ImGui::PushID(key.c_str());ImGui::BeginDisabled(!editable);
                        if(ImGui::Selectable(label.c_str()))try {
                            if(!hasValue)throw std::runtime_error("This composite field is inherited. Enter a schema-compatible override for '"+key+"' in the JSON editor.");
                            auto overrides=Authoring::ParseCloneJson(cloneEditor);
                            if(!overrides.is_object())throw std::runtime_error("Overrides must be a JSON object.");
                            if(!overrides.contains(key))overrides[key]=field["Value"];
                            const auto text=overrides.dump(2);
                            if(text.size()>=sizeof(cloneEditor))throw std::runtime_error("Overrides exceed this editor's capacity.");
                            std::snprintf(cloneEditor,sizeof(cloneEditor),"%s",text.c_str());cloneError.clear();
                        }catch(const std::exception& e){cloneError=e.what();}
                        ImGui::EndDisabled();
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s\n%s",key.c_str(),field.value("Reason",std::string("Click to copy this current value into overrides.")).c_str());
                        ImGui::PopID();
                    }
                }
                // BeginChild requires EndChild even when the window is clipped.
                if(cloneReady)ImGui::EndChild();
                ImGui::TextWrapped("Override JSON: only these fields change. Click a setting above to add its current value; delete a key to inherit it. Explicit JSON overrides take precedence over the name/icon boxes. Identity and soft-delete fields are backend-managed.");
                ImGui::InputTextMultiline("Item overrides JSON",cloneEditor,sizeof(cloneEditor),ImVec2(-1,220));
                ImGui::Checkbox("I accept experimental testing on a disposable or backed-up save",&cloneAck);
                ImGui::BeginDisabled(!cloneReady || !cloneAck || player.empty() || clonePending || SpawnToolRequests::Waiting.load() || (!permanent&&cloneSoft.empty()));
                if(ImGui::Button("Create clone now"))try {
                    auto overrides=Authoring::ParseCloneJson(cloneEditor);
                    submitClone(Authoring::CloneRequest(selectedItem,cloneName,cloneIcon,std::move(overrides),permanent,cloneGive,itemCount,cloneAck,player));
                    cloneAck=false;
                }catch(const std::exception& e){cloneError=e.what();}
                ImGui::EndDisabled();
                if(!cloneError.empty())ImGui::TextWrapped("Not submitted: %s",cloneError.c_str());
                if(!cloneStatus.empty())ImGui::TextWrapped("%s",cloneStatus.c_str());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
        return;
    }
    if(kind==3) {
        ImGui::InputTextWithHint("Search loaded buildings","name or BuildingPieceData path",buildingFilter,sizeof(buildingFilter));
        auto label=std::string(selectedBuilding);bool selectedBuildingSafe=false;std::string selectedBuildingReason;
        size_t verifiedBuildings=0,blockedBuildings=0;
        if(result.contains("Buildings"))for(const auto& row:result["Buildings"]) {
            row.value("Safe",false)?++verifiedBuildings:++blockedBuildings;
            if(row.value("Path",std::string{})==selectedBuilding){label=row.value("Name",label);selectedBuildingSafe=row.value("Safe",false);selectedBuildingReason=row.value("SafetyReason",std::string{});}
        }
        if(verifiedBuildings+blockedBuildings)ImGui::TextDisabled("Safety scope: %zu verified / %zu blocked",verifiedBuildings,blockedBuildings);
        if(ImGui::BeginCombo("Loaded building",label.empty()?"Select building":label.c_str())) {
            auto lower=[](std::string value){std::ranges::transform(value,value.begin(),
                [](unsigned char character){return static_cast<char>(std::tolower(character));});return value;};
            const auto filter=lower(buildingFilter);size_t shown=0;
            if(result.contains("Buildings"))for(const auto& row:result["Buildings"]) {
                const auto path=row.value("Path",std::string{}),title=row.value("Name",path);
                if(!filter.empty() && lower(path+" "+title).find(filter)==std::string::npos)continue;
                if(++shown>250)continue;
                ImGui::PushID(path.c_str());
                const bool safe=row.value("Safe",false);
                ImGui::BeginDisabled(!safe);
                if(ImGui::Selectable(title.c_str(),path==selectedBuilding)){std::snprintf(selectedBuilding,sizeof(selectedBuilding),"%s",path.c_str());confirmed=false;}
                ImGui::EndDisabled();
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s\nSafety: %s",path.c_str(),row.value("SafetyReason",safe?std::string("Verified"):std::string("Blocked")).c_str());
                ImGui::PopID();
            }
            if(shown>250)ImGui::TextDisabled("More than 250 matches; narrow the filter.");
            if(!shown)ImGui::TextDisabled("No loaded BuildingPieceData matches this filter.");
            ImGui::EndCombo();
        }
        if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx"))confirmed=false;
        ImGui::TextDisabled("Safety mode: one building preview per spawn; grid/batch placement is disabled.");
        if(ImGui::SliderFloat("Distance from player (metres)",&distance,3,50))confirmed=false;
        if(ImGui::InputFloat("Placement yaw (degrees)",&yaw))confirmed=false;
        if(ImGui::InputFloat("Height offset above ground (cm)",&height))confirmed=false;
        if(ImGui::Checkbox("Allow deconstruction",&allowDeconstruction))confirmed=false;
        ImGui::Text("Availability");ImGui::SameLine();
        if(ImGui::RadioButton("Any##building-time",&buildingTime,0))confirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Day##building-time",&buildingTime,1))confirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Night##building-time",&buildingTime,2))confirmed=false;
        ImGui::TextWrapped("Temporary previews use the selected BuildingPieceData's native buildable actor only after a matching native in-world instance proves the exact data binding. The spawned actor is transient, excluded from the native save store, and re-verified after construction before RuneSchema keeps it.");
        if(selectedBuilding[0]=='/' && !selectedBuildingSafe)ImGui::TextWrapped("Blocked by safety preflight: %s",selectedBuildingReason.c_str());
        ImGui::Checkbox("I want to spawn one verified temporary building preview",&confirmed);
        ImGui::BeginDisabled(!confirmed || player.empty() || selectedBuilding[0]!='/' || !selectedBuildingSafe);
        if(ImGui::Button("Spawn building near selected player")) {
            nlohmann::json request={{"Action","SpawnBuilding"},{"Player",player},{"Building",selectedBuilding},
                {"Yaw",yaw},{"Height",height},{"Scale",scale},{"Count",1},{"Distance",distance},
                {"AllowDeconstruction",allowDeconstruction},{"TimeOfDay",buildingTime==1?"Day":buildingTime==2?"Night":"Any"}};
            SpawnToolRequests::Submit(std::move(request));confirmed=false;
        }
        ImGui::EndDisabled();
        if(ImGui::Button("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
        ImGui::TextWrapped("The catalog keeps all loaded BuildingPieceData for inspection. Saved safety results are never trusted across worlds: rebuild in the current authoritative world to enable only pieces that match a live native instance, exact BuildingPieceData binding, and save-exclusion contract.");
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
        return;
    }
    ImGui::BeginDisabled(kind==2);
    bool permanent=Authoring::PermanentSpawn.load();
    if(ImGui::Checkbox("Permanent spawn",&permanent)){Authoring::PermanentSpawn=permanent;confirmed=false;}
    if(permanent)ImGui::TextWrapped("Saves original placements to RuneSchema/mods/runeschema/spawns/. Applies in every world using this mod. New AI placements do not automatically enable respawn. Disable/remove the JSON and restart to uninstall.");
    if(kind==0) {
        bool overridePower=Authoring::EnemyPower.load()!=-1;
        if(ImGui::Checkbox("Override enemy power level",&overridePower)){Authoring::EnemyPower=overridePower?1:-1;confirmed=false;}
        if(overridePower) {
            int power=Authoring::EnemyPower.load();
            if(ImGui::InputInt("Enemy power level (1..100)",&power)) {Authoring::EnemyPower=std::clamp(power,1,100);confirmed=false;}
            ImGui::TextWrapped("Permanent uses the native AI spawn-point power. Temporary requires a reflected PowerLevel on that AI and refuses unsupported classes; it does not invent stat multipliers.");
        }else ImGui::TextDisabled("Power level: native/default");
    }
    if(ImGui::Checkbox(kind==0?"Ad-hoc AI class":"Ad-hoc resource class",&custom))confirmed=false;
    if(custom) {
        resource=kind==1;
        if(ImGui::InputText("Blueprint class path",ai,sizeof(ai)))confirmed=false;
        if(ImGui::InputText("Spawn name (optional)",name,sizeof(name)))confirmed=false;
        if(!resource && ImGui::Checkbox("Use custom name for boss title too",&boss))confirmed=false;
    }else {
        ImGui::InputTextWithHint("Search loaded AI/resources","name, class, type or mod id",
            definitionFilter,sizeof(definitionFilter));
        auto selectedLabel=definition.empty()?std::string("Select spawn"):definition;
        if(result.contains("Definitions"))for(const auto& row:result["Definitions"])
            if(row.value("Key",std::string{})==definition) {
                selectedLabel="["+row.value("Type",std::string("AI"))+"] "+
                    row.value("Name",row.value("Key",std::string{}));
                break;
            }
        if(ImGui::BeginCombo("Loaded spawn",selectedLabel.c_str())) {
            bool shown=false;
            auto lower=[](std::string value){std::ranges::transform(value,value.begin(),
                [](unsigned char character){return static_cast<char>(std::tolower(character));});return value;};
            const auto filter=lower(definitionFilter);
            if(result.contains("Definitions"))for(const auto& row:result["Definitions"]) {
                const auto key=row.value("Key",std::string{}),type=row.value("Type",std::string("AI"));
                if((kind==0 && type!="AI") || (kind==1 && type!="Resource"))continue;
                const auto title=row.value("Name",key),path=row.value("Class",std::string{});
                if(!filter.empty() && lower(key+" "+type+" "+title+" "+path).find(filter)==std::string::npos)continue;
                shown=true;const auto display="["+type+"] "+title+" — "+key;
                ImGui::PushID(key.c_str());
                if(ImGui::Selectable(display.c_str(),key==definition)){definition=key;confirmed=false;}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.c_str());
                ImGui::PopID();
            }
            if(!shown)ImGui::TextDisabled("No loaded AI or resource matches this filter.");
            ImGui::EndCombo();
        }
        if(ImGui::InputText("Spawn name override (optional)",name,sizeof(name)))confirmed=false;
        if(kind==0 && ImGui::Checkbox("Use name for boss title too",&boss))confirmed=false;
    }
    if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx"))confirmed=false;
    if(ImGui::Checkbox("Grid placement",&grid))confirmed=false;
    if(grid) {
        if(ImGui::SliderInt("Grid rows",&rows,1,10))confirmed=false;
        if(ImGui::SliderInt("Grid columns",&columns,1,10))confirmed=false;
        if(ImGui::SliderFloat("Grid spacing (metres)",&spacing,0.1f,100))confirmed=false;
        ImGui::TextWrapped("First point is the anchor near the player. Grid rotates with placement yaw; each point snaps to ground.");
    }else if(ImGui::SliderInt("Count",&count,1,20))confirmed=false;
    if(ImGui::Checkbox("Override ghost mesh setting",&overrideGhost))confirmed=false;
    if(overrideGhost && ImGui::Checkbox("Ghost mesh overlay",&ghost))confirmed=false;
    if(ImGui::Checkbox("Override additional drops (AI / resources)",&overrideDrops))confirmed=false;
    if(overrideDrops) {
        ImGui::SeparatorText("Additional loot");
        const auto dropPicker=ItemGridPicker::Render("additional-drop",result,dropItemFilter,sizeof(dropItemFilter),
            item,sizeof(item),dropItemPickerState);
        if(dropPicker.Changed)confirmed=false;
        if(item[0]=='/') {
            ImGui::Text("Selected: %s",dropPicker.SelectedTitle.c_str());
            ImGui::TextDisabled("%s",item);
        }
        if(ImGui::CollapsingHeader("Advanced item path override")) {
            ImGui::InputText("Item asset path##additional-drop",item,sizeof(item));
            ImGui::TextDisabled("Use this only for a valid loaded item path that is not available in the current catalog.");
        }
        ImGui::InputInt("Minimum quantity",&dropMin);ImGui::InputInt("Maximum quantity",&dropMax);
        ImGui::SliderFloat("Drop chance (%)",&chance,0,100);
        ImGui::BeginDisabled(drops.size()>=16 || item[0]!='/' || dropMin<1 || dropMax<dropMin || dropMax>10000);
        if(ImGui::Button("Add selected item to additional loot")) {
            drops.push_back({{"Item",item},{"Min",dropMin},{"Max",dropMax},{"ChancePercent",chance}});confirmed=false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();ImGui::TextDisabled("%zu / 16 entries",drops.size());

        if(!drops.empty() && ImGui::BeginTable("AdditionalDropList",4,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Item",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Quantity",ImGuiTableColumnFlags_WidthFixed,90.0f);
            ImGui::TableSetupColumn("Chance",ImGuiTableColumnFlags_WidthFixed,80.0f);
            ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,70.0f);
            ImGui::TableHeadersRow();
            for(size_t i=0;i<drops.size();++i) {
                const auto path=drops[i].value("Item",std::string{});
                const auto title=ItemGridPicker::Title(result,path);
                ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(title.c_str());
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.c_str());
                ImGui::TableSetColumnIndex(1);ImGui::Text("%d-%d",drops[i].value("Min",1),drops[i].value("Max",1));
                ImGui::TableSetColumnIndex(2);ImGui::Text("%.1f%%",drops[i].value("ChancePercent",100.0f));
                ImGui::TableSetColumnIndex(3);
                if(ImGui::SmallButton("Remove")){drops.erase(i);confirmed=false;ImGui::PopID();break;}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if(ImGui::SliderFloat("Distance from player (metres)",&distance,3,50))confirmed=false;
    if(ImGui::InputFloat("Placement yaw (degrees)",&yaw))confirmed=false;
    if(ImGui::InputFloat("Height offset above ground (cm)",&height))confirmed=false;
    ImGui::Checkbox(permanent?"I want to spawn and install these placements":"I want to create these functioning temporary actors",&confirmed);
    ImGui::BeginDisabled(kind==2 || !confirmed || player.empty() || (!custom && definition.empty()));
    if(ImGui::Button("Spawn near selected player")) {
        nlohmann::json request={{"Action","Spawn"},{"Player",player},{"Definition",custom?"":definition},
            {"Class",ai},{"Name",name},{"Boss",boss},{"Resource",custom && resource},{"Yaw",yaw},{"Height",height},
            {"Scale",scale},{"Count",grid?rows*columns:count},{"Distance",distance},{"Permanent",permanent}};
        if(kind==0 && Authoring::EnemyPower.load()!=-1)request["PowerLevel"]=Authoring::EnemyPower.load();
        if(grid)request["Grid"]={{"Rows",rows},{"Columns",columns},{"SpacingMeters",spacing}};
        if(overrideGhost)request["GhostMesh"]=ghost;
        if(overrideDrops)request["AdditionalDrops"]=drops;
        SpawnToolRequests::Submit(std::move(request));confirmed=false;
    }
    ImGui::EndDisabled();
    if(ImGui::Button("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
    ImGui::TextWrapped("Exports use original placement, not wandering AI positions. Export/clear do not install a mod or change live actors. Resource classes must expose verified save exclusion; unsupported classes are refused.");
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    ImGui::EndDisabled();
}

inline void RenderSessionCleanup() {
    static std::string player;
    const auto result=SpawnToolRequests::Read();
    ImGui::TextWrapped("Clean only actors and staged placements created by the current Mod Authoring session. Installed /spawns content and saves are not changed.");
    if(ImGui::Button("Refresh authoritative players"))SpawnToolRequests::Submit({{"Action","Catalog"}});
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
    if(ImGui::Button("Clean up live tool-created actors"))SpawnToolRequests::Submit({{"Action","Cleanup"},{"Player",player}});
    ImGui::EndDisabled();
    if(ImGui::Button("Clear staged export placements"))SpawnToolRequests::Submit({{"Action","ClearPlacements"}});
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
}
}
