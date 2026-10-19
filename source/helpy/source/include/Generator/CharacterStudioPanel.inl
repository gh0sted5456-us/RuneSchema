// Included inside SaveEditorPanel. Shares the real authoring session and Item Bay.
inline Json StudioAppearance=Json::object(),StudioEquipment=Json::object(),StudioSelected=Json::object(),StudioAddition=Json::object();
inline Json StudioSkills=Json::object(),StudioSavedLoadout=Json::array();
inline bool StudioSkillsRequested=false,StudioLoadoutLoaded=false;
inline std::map<std::string,int> StudioSkillDrafts;
inline uint64_t StudioReceipt=0,StudioGeneration=0;
inline char StudioName[129]{},StudioFilter[257]{},StudioSelectedPath[2049]{};
inline ItemGridPicker::State StudioPicker;
inline std::string StudioView,StudioRequestedView;
inline std::string StudioPaletteField="HairColor";
inline bool StudioShowPreview=true;
inline std::string StudioPreviewMode="Body";
inline int StudioQuantity=1;inline bool StudioPersonal=false,StudioConfirm=false,StudioListRequested=false,StudioAppearanceRequested=false;
inline double StudioLastDraw=0;
inline std::string StudioSlot,StudioSlotQuery;
inline char StudioSlotSearch[257]{};
inline Json StudioSlotRows=Json::array();
inline size_t StudioSlotOffset=0;inline bool StudioSlotMore=false,StudioSlotPending=false,StudioSlotOpen=false;
inline std::string StudioSlotError;
inline void StudioResetSlot(){StudioSlotOpen=StudioSlotPending=StudioSlotMore=false;StudioSlotRows=Json::array();StudioSlotOffset=0;StudioSlot.clear();StudioSlotQuery.clear();StudioSlotError.clear();}
inline void StudioApply(){
    const auto generation=AuthoringClient::Snapshot.value("Generation",uint64_t{});
    if(generation!=StudioGeneration){StudioResetSlot();StudioGeneration=generation;StudioReceipt=0;StudioAppearance=Json::object();StudioRequestedView.clear();StudioEquipment=Json::object();StudioListRequested=false;StudioAppearanceRequested=false;StudioAddition=Json::object();StudioSelected=Json::object();StudioSkillsRequested=false;StudioSkills=Json::object();StudioSkillDrafts.clear();StudioSavedLoadout=Json::array();StudioLoadoutLoaded=false;CharacterPreviewPanel::Close();}
    const auto id=AuthoringClient::Snapshot.value("RequestId",uint64_t{});if(!id||id==StudioReceipt)return;StudioReceipt=id;
    const auto op=AuthoringClient::Snapshot.value("Operation",std::string{});const auto& result=AuthoringClient::Result(op.c_str());
    if(result.contains("Error")){if(op=="SaveEditorRender")CharacterPreviewPanel::Fail(result.at("Error").get<std::string>());if(op=="SaveEditorSlotItems"){StudioSlotPending=false;StudioSlotMore=false;StudioSlotError=result.at("Error").get<std::string>();}return;}
    if(op=="SaveEditorSlotItems"){
        StudioSlotPending=false;
        if(StudioSlotOpen&&result.value("Slot",std::string{})==StudioSlot){
            if(result.contains("Rows"))for(const auto& row:result["Rows"])if(StudioSlotRows.size()<2048&&LoadoutEligibility::Matches(row,StudioSlot))StudioSlotRows.push_back(row);
            StudioSlotOffset=result.value("NextOffset",size_t{});StudioSlotMore=result.value("HasMore",false);
        }
    }
    if(op=="SaveEditorOpen"||op=="SaveEditorClose"){StudioResetSlot();StudioAppearance=Json::object();StudioAppearanceRequested=false;StudioSkillsRequested=false;StudioSkills=Json::object();StudioSkillDrafts.clear();StudioSavedLoadout=Json::array();StudioLoadoutLoaded=false;StudioEquipment=Json::object();StudioRequestedView.clear();CharacterPreviewPanel::Close();}
    if(op=="SaveEditorAppearance"||op=="SaveEditorAppearanceSet"||op=="SaveEditorRename"){
        StudioAppearance=result;AuthoringPanel::Copy(StudioName,sizeof(StudioName),result.value("Name",std::string{}));
        if(result.contains("Revision"))Browser["Revision"]=result["Revision"];
        if(op!="SaveEditorAppearance"){ResetInventory();Invalidate();StudioRequestedView.clear();}
    }
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"||op=="SaveEditorAddItem")if(result.contains("Revision"))StudioAppearance["Revision"]=result["Revision"];
    if(op=="SaveEditorSkills"||op=="SaveEditorSkillSet"){StudioSkills=result;StudioSkillDrafts.clear();if(result.contains("Revision"))StudioAppearance["Revision"]=result["Revision"];if(op=="SaveEditorSkillSet"){ResetInventory();Invalidate();}}
    if(op=="SaveEditorInventory"&&result.contains("Loadout")){
        StudioSavedLoadout=result["Loadout"];if(!StudioLoadoutLoaded){StudioEquipment=Json::object();for(const auto& row:StudioSavedLoadout){const auto slot=row.value("Location",std::string{});if((slot=="Head"||slot=="Body"||slot=="Legs"||slot=="Cape"||slot=="Trinket")&&row.contains("AssetPath")&&row["AssetPath"].is_string())StudioEquipment[slot]=row["AssetPath"];}StudioLoadoutLoaded=true;StudioRequestedView.clear();}
    }
    if(op=="SaveEditorRender"&&result.contains("Channel"))try{CharacterPreviewPanel::Attach(result.at("Channel").get<std::string>());CharacterPreviewPanel::AnimationName=result.value("AnimationName",std::string{});}catch(const std::exception& error){CharacterPreviewPanel::Status=error.what();}
}
inline void StudioHeader(){
    if(!StudioListRequested&&!AuthoringClient::Busy())StudioListRequested=AuthoringClient::Submit("SaveEditorList");
    const auto& list=AuthoringClient::Result("SaveEditorList");
    ImGui::SetNextItemWidth(std::max(200.f,ImGui::GetContentRegionAvail().x*.6f));
    const auto current=File[0]?std::filesystem::path(File).filename().string():std::string("Choose a character save");
    if(ImGui::BeginCombo("##character-studio-save",current.c_str())){if(list.contains("Files"))for(const auto& file:list["Files"]){const auto path=file.at("Path").get<std::string>();if(ImGui::Selectable(file.at("Name").get_ref<const std::string&>().c_str(),path==File)){AuthoringPanel::Copy(File,sizeof(File),path);Invalidate();}}ImGui::EndCombo();}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Import JSON..."))Choose();ImGui::SameLine();if(Button("Refresh saves","SaveEditorList"))StudioListRequested=true;
    ImGui::Checkbox("This character is not currently loaded",&Offline);ImGui::SameLine();ImGui::BeginDisabled(!Offline||!File[0]);Button("Open character copy","SaveEditorOpen",{{"Path",File},{"Offline",Offline}});ImGui::EndDisabled();
    ImGui::TextDisabled("Native game rendering · preview only · original save is never overwritten");
    if(Opened&&!StudioAppearanceRequested&&!AuthoringClient::Busy())StudioAppearanceRequested=AuthoringClient::Submit("SaveEditorAppearance");
    if(Opened&&StudioAppearance.empty()&&StudioAppearanceRequested&&HelpyImGuiTheme::SoftButton("Retry native appearance choices"))StudioAppearanceRequested=false;
    ImGui::TextDisabled("Save-format reference: RSDWTools / RSDWArchive · native rendering: RuneSchema");
}
inline void StudioReview(){
    ImGui::Separator();Button("Review save changes","SaveEditorPreview");ImGui::SameLine();Button("Discard save draft","SaveEditorClose");
    if(!Token.empty()){
        ImGui::SeparatorText("Review edited copy");if(Review.contains("Summary"))for(const auto& row:Review["Summary"]){ImGui::TextWrapped("%s",row.at("Label").get_ref<const std::string&>().c_str());ImGui::TextDisabled("%s",row.at("Field").get_ref<const std::string&>().c_str());if(row.contains("Before")||row.contains("After"))ImGui::TextWrapped("%s → %s",row.value("Before",Json{}).dump().c_str(),row.value("After",Json{}).dump().c_str());}
        ImGui::Checkbox("I reviewed these changes; export a new copy",&Confirmed);ImGui::BeginDisabled(!Confirmed);Button("Export edited character copy","SaveEditorExport",{{"Token",Token},{"Confirm",Confirmed}});ImGui::EndDisabled();
    }
    const auto& exported=AuthoringClient::Result("SaveEditorExport");if(exported.contains("File"))ImGui::TextWrapped("Export: %s",exported.at("File").get_ref<const std::string&>().c_str());
}
inline void StudioStageAddition(const Json& item){StudioAddition=item;StudioQuantity=1;StudioConfirm=false;}
inline void StudioBayControls(){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BaySearchId=ImGui::GetID("Search");HelpyPreviewHooks::BayRefreshId=ImGui::GetID("Refresh");
#endif
    ItemBayBrowser::Controls(StudioPicker,StudioFilter,sizeof(StudioFilter));
}
inline void StudioPaintKit(){
    const auto& theme=HelpyImGuiTheme::Current;ImGui::SeparatorText("Painter's kit · native palette");
    if(ImGui::BeginTabBar("##native-paint-fields")){for(const auto& field:StudioAppearance["Choices"].at("Fields")){
        const auto key=field.at("Key").get<std::string>();if(key!="SkinTone"&&key!="EyeColor"&&key!="HairColor"&&key!="EyebrowColor")continue;
        const auto title=key=="SkinTone"?"Skin":key=="EyeColor"?"Eyes":key=="HairColor"?"Hair":"Eyebrows";
        if(ImGui::BeginTabItem(title)){StudioPaletteField=key;const auto current=StudioAppearance["Appearance"].at(key).at("rowName").get<std::string>();ImGui::TextWrapped("%s · %s",field.at("Label").get_ref<const std::string&>().c_str(),current.c_str());
            const float size=theme.Y(38),gap=ImGui::GetStyle().ItemSpacing.x,available=ImGui::GetContentRegionAvail().x;const int columns=std::max(1,int((available+gap)/(size+gap)));int i=0;
            for(const auto& value:field.at("Rows")){const auto row=value.get<std::string>();ImGui::PushID(row.c_str());const auto a=ImGui::GetCursorScreenPos();const bool clicked=ImGui::InvisibleButton("##paint-well",{size,size});const bool selected=row==current,hovered=ImGui::IsItemHovered();const ImVec2 center(a.x+size*.5f,a.y+size*.5f);auto* draw=ImGui::GetWindowDrawList();
                const auto swatches=StudioAppearance["Choices"].value("Swatches",Json::object());const auto colors=swatches.value(key,Json::object());const auto color=colors.value(row,Json{});ImU32 fill=ImGui::GetColorU32(theme.panelAlt);if(color.is_array()&&color.size()==4)fill=IM_COL32(color[0].get<int>(),color[1].get<int>(),color[2].get<int>(),255);
                draw->AddCircleFilled(center,size*.39f,fill);draw->AddCircle(center,size*.45f,ImGui::GetColorU32(selected||hovered?theme.accentHover:theme.border),0,theme.X(selected?2.f:1.f));if(selected){draw->AddLine({center.x-size*.12f,center.y},{center.x-size*.02f,center.y+size*.1f},IM_COL32(255,255,255,255),2);draw->AddLine({center.x-size*.02f,center.y+size*.1f},{center.x+size*.15f,center.y-size*.13f},IM_COL32(255,255,255,255),2);}
                if(clicked)if(AuthoringClient::Submit("SaveEditorAppearanceSet",{{"Field",key},{"Row",row},{"Revision",StudioAppearance.at("Revision")}}))Invalidate();
                if(hovered){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextWrapped("%s · %s",field.at("Label").get_ref<const std::string&>().c_str(),row.c_str());if(color.is_array())ImGui::TextUnformatted("Game character-creation swatch");else ImGui::TextUnformatted("Native swatch unavailable");HelpyImGuiTheme::EndDetailsTooltip();}ImGui::PopID();++i;if(i%columns&&i<int(field.at("Rows").size()))ImGui::SameLine();
            }
            ImGui::EndTabItem();}
    }ImGui::EndTabBar();}
}
inline void StudioAppearanceControls(){
    if(!StudioAppearance.contains("Appearance")||!StudioAppearance.contains("Choices"))return;
    if(ImGui::BeginTable("##native-appearance-selectors",2,ImGuiTableFlags_SizingStretchSame)){
    for(const auto& field:StudioAppearance["Choices"].at("Fields")){
        const auto key=field.at("Key").get<std::string>();const auto found=StudioAppearance["Appearance"].find(key);
        if(key=="SkinTone"||key=="EyeColor"||key=="HairColor"||key=="EyebrowColor")continue;
        ImGui::TableNextColumn();
        const auto row=found!=StudioAppearance["Appearance"].end()?found->value("rowName",std::string{}):std::string{};
        ImGui::TextUnformatted(field.at("Label").get_ref<const std::string&>().c_str());ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo(("##appearance-"+key).c_str(),row.c_str())){
            const auto body=StudioAppearance["Appearance"].value("BodyType",Json::object()).value("rowName",std::string{});
            for(const auto& value:field.at("Rows")){const auto name=value.get<std::string>();if(key=="Head"&&!body.empty()&&((body.starts_with("female")&&!name.starts_with("female"))||(body.starts_with("male")&&!name.starts_with("male"))))continue;
                if(key=="FacialHairPreset"&&!body.empty()&&((body.starts_with("female")&&name.starts_with("M_"))||(body.starts_with("male")&&name.starts_with("F_"))))continue;
                if(ImGui::Selectable(name.c_str(),row==name))if(AuthoringClient::Submit("SaveEditorAppearanceSet",{{"Field",key},{"Row",name},{"Revision",StudioAppearance.at("Revision")}}))Invalidate();}
            ImGui::EndCombo();}
    }ImGui::EndTable();}
    StudioPaintKit();
}
inline void StudioSkillControls(){
    if(!StudioSkillsRequested&&!AuthoringClient::Busy())StudioSkillsRequested=AuthoringClient::Submit("SaveEditorSkills");
    if(!StudioSkills.contains("Items")){ImGui::TextDisabled("Resolving skills…");if(StudioSkillsRequested&&HelpyImGuiTheme::SoftButton("Retry"))StudioSkillsRequested=false;return;}
    if(ImGui::BeginTable("##saved-skill-levels",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH)){
        ImGui::TableSetupColumn("Skill",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,85);ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,50);
        for(const auto& row:StudioSkills["Items"]){const auto id=row.at("Id").get<std::string>();ImGui::PushID(id.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();const auto at=ImGui::GetCursorScreenPos();const float size=HelpyImGuiTheme::Current.Y(26);const auto icon=row.value("Icon",std::string{});if(!icon.empty())ItemIconCache::Draw(icon,at,{at.x+size,at.y+size},true);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::TextWrapped("%s",row.at("Name").get_ref<const std::string&>().c_str());ImGui::TableNextColumn();
            if(row.value("Resolved",false)){auto [value,inserted]=StudioSkillDrafts.try_emplace(id,row.at("Level").get<int>());ImGui::SetNextItemWidth(-1);ImGui::InputInt("##level",&value->second,0,0);ImGui::TableNextColumn();ImGui::BeginDisabled(value->second<1||value->second>row.at("MaxLevel").get<int>()||AuthoringClient::Busy());if(HelpyImGuiTheme::SoftButton("Stage"))AuthoringClient::Submit("SaveEditorSkillSet",{{"Id",id},{"Level",value->second},{"Revision",StudioAppearance.at("Revision")}});ImGui::EndDisabled();}
            else{ImGui::TextDisabled("Unresolved");ImGui::TableNextColumn();}ImGui::PopID();
        }ImGui::EndTable();
    }ImGui::TextDisabled("Levels use native XP thresholds · save draft only");
}
inline void StudioFit(const Json& row,const std::string& slot){
    if(!LoadoutEligibility::Matches(row,slot)){Error="Native slot compatibility is unavailable or does not match. Use the slot picker to read compatible items.";return;}
    StudioSelected=row;
    if(slot=="Head"||slot=="Body"||slot=="Legs"||slot=="Cape"||slot=="Trinket"){StudioEquipment[slot]=row.at("Path");StudioPreviewMode="Body";}
    else {StudioPreviewMode="Item";}
    StudioRequestedView.clear();
}
inline void StudioSlotPicker(){
    if(!StudioSlotOpen)return;const auto& theme=HelpyImGuiTheme::Current;
    ImGui::SetNextWindowSize({theme.X(620),theme.Y(530)},ImGuiCond_FirstUseEver);
    if(ImGui::Begin(("Fit "+StudioSlot+"###native-slot-picker").c_str(),&StudioSlotOpen)){
        ImGui::TextDisabled("Preview only · saved loadout and live player are unchanged");
        ImGui::SetNextItemWidth(std::max(60.f,ImGui::GetContentRegionAvail().x-theme.X(135)));
        const bool enter=ImGui::InputTextWithHint("##slot-search","Search compatible items…",StudioSlotSearch,sizeof(StudioSlotSearch),ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();ImGui::BeginDisabled(StudioSlotPending);const bool search=HelpyImGuiTheme::SoftButton("Search");ImGui::SameLine();const bool refresh=HelpyImGuiTheme::SoftButton("Refresh");ImGui::EndDisabled();
        if((search||refresh||enter)&&!StudioSlotPending){StudioSlotQuery=StudioSlotSearch;StudioSlotRows=Json::array();StudioSlotOffset=0;StudioSlotMore=true;StudioSlotError.clear();}
        if(!StudioSlotPending&&StudioSlotMore&&StudioSlotRows.size()<48&&!AuthoringClient::Busy())StudioSlotPending=AuthoringClient::Submit("SaveEditorSlotItems",{{"Slot",StudioSlot},{"Query",StudioSlotQuery},{"Offset",StudioSlotOffset}});
        if(!StudioSlotError.empty())ImGui::TextWrapped("%s",StudioSlotError.c_str());
        ImGui::BeginChild("##compatible-slot-items",{0,theme.Y(340)},true);
        const float gap=ImGui::GetStyle().ItemSpacing.x,available=ImGui::GetContentRegionAvail().x;
        const int columns=ItemBayLayout::Columns(available,theme.X(theme.gridMinCardWidth),gap,theme.itemGridColumns);
        const float width=std::max(1.f,(available-gap*(columns-1))/columns),height=theme.Y(theme.gridCardHeight);
        ImGuiListClipper clip;clip.Begin(int((StudioSlotRows.size()+columns-1)/columns),height+ImGui::GetStyle().ItemSpacing.y);
        while(clip.Step())for(int line=clip.DisplayStart;line<clip.DisplayEnd;++line)for(int col=0;col<columns;++col){
            const auto index=size_t(line*columns+col);if(index>=StudioSlotRows.size())break;const auto& row=StudioSlotRows[index];const auto path=row.value("Path",std::string{});
            ImGui::PushID(path.c_str());const auto a=ImGui::GetCursorScreenPos();const bool click=ImGui::InvisibleButton("##slot-item",{width,height}),hovered=ImGui::IsItemHovered();
            ItemGridPicker::DrawPlacard(row,a,{a.x+width,a.y+height},StudioSelected.value("Path",std::string{})==path,hovered);
            if(click)StudioFit(row,StudioSlot);ItemGridPicker::DragItem(row);
            if(HelpyImGuiTheme::TooltipReady("slot:"+path,hovered))ItemGridPicker::VerboseTooltip(row,ItemPresentation::Name(row),path,false,true);
            ImGui::PopID();if(col+1<columns&&index+1<StudioSlotRows.size())ImGui::SameLine();
        }
        if(StudioSlotRows.empty())ImGui::TextWrapped("%s",StudioSlotPending||StudioSlotMore?"Reading compatible native items…":"No compatible loaded items. Load additional content and refresh.");
        ImGui::EndChild();
        ImGui::TextDisabled("%zu compatible items · native slot strategy",StudioSlotRows.size());
        ImGui::BeginDisabled(StudioSlotPending||!StudioSlotMore||StudioSlotRows.size()>=2048);if(HelpyImGuiTheme::SoftButton("More compatible items"))StudioSlotPending=AuthoringClient::Submit("SaveEditorSlotItems",{{"Slot",StudioSlot},{"Query",StudioSlotQuery},{"Offset",StudioSlotOffset}});ImGui::EndDisabled();
        ImGui::TextWrapped("Compatibility is read from loaded native ItemData, not names. Refresh after loading new content. Skill/use requirements are not bypassed.");
    }ImGui::End();
}
inline void StudioGear(){
    ImGui::SeparatorText("Saved loadout");
    const auto& theme=HelpyImGuiTheme::Current;const int columns=ImGui::GetContentRegionAvail().x>450?3:2;
    if(ImGui::BeginTable("##native-preview-slots",columns,ImGuiTableFlags_SizingStretchSame)){
    for(const auto* slot:{"Head","Body","Legs","Cape","Trinket","Trinket2","Ammo","MagicAmmo1","HeldRight","HeldLeft","CrossbowBolts","FishingBait"}){
        ImGui::TableNextColumn();
        const std::string_view location=slot;const auto glyph=location=="Head"?"helmet":location=="Body"||location=="Cape"?"body":location=="Legs"?"legs":location=="Trinket"||location=="Trinket2"?"gem":location=="MagicAmmo1"?"runecrafting":location=="HeldRight"?"sword":location=="HeldLeft"?"shield":location=="FishingBait"?"fish":"bow";
        const auto title=location=="Trinket2"?"Trinket 2":location=="MagicAmmo1"?"Runes":location=="HeldRight"?"Main hand":location=="HeldLeft"?"Off hand":location=="CrossbowBolts"?"Bolts":location=="FishingBait"?"Bait":slot;
        ImGui::PushID(slot);Json item=Json::object();for(const auto& row:StudioSavedLoadout)if(row.value("Location",std::string{})==slot)item=SavedPlacard(row);
        const auto path=StudioEquipment.value(slot,item.value("Path",std::string{}));if(!path.empty()&&item.value("Path",std::string{})!=path)item={{"Path",path},{"Name","Preview fitting"}};
        const auto a=ImGui::GetCursorScreenPos();const ImVec2 b(a.x+ImGui::GetContentRegionAvail().x,a.y+theme.Y(108));const bool clicked=ImGui::InvisibleButton("##loadout-placard",{b.x-a.x,b.y-a.y}),hovered=ImGui::IsItemHovered();
        if(!path.empty()){ItemGridPicker::DrawPlacard(item,a,b,false,hovered);if(clicked){StudioSelected=item;StudioPreviewMode="Item";StudioRequestedView.clear();}}
        else{auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,b,ImGui::GetColorU32(theme.panelAlt),4);draw->AddRect(a,b,ImGui::GetColorU32(theme.border),4);MapLayerIcons::Draw(glyph,{(a.x+b.x)*.5f,a.y+theme.Y(48)},theme.Y(28));}
        auto* draw=ImGui::GetWindowDrawList();const auto extent=ImGui::CalcTextSize(title);draw->AddRectFilled(a,{b.x,a.y+theme.Y(18)},ImGui::GetColorU32(theme.panelAlt),3);MapLayerIcons::Draw(glyph,{a.x+theme.X(10),a.y+theme.Y(9)},theme.Y(13));draw->AddText({std::max(a.x+theme.X(20),(a.x+b.x-extent.x)*.5f),a.y+theme.Y(2)},ImGui::GetColorU32(theme.accent),title);
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right)&&!StudioSlotPending){StudioSlot=slot;StudioSlotQuery.clear();StudioSlotSearch[0]=0;StudioSlotRows=Json::array();StudioSlotOffset=0;StudioSlotMore=true;StudioSlotOpen=true;StudioSlotError.clear();}
        if(ImGui::BeginDragDropTarget()){if(auto row=ItemGridPicker::AcceptItemDrop(Error))StudioFit(*row,slot);ImGui::EndDragDropTarget();}
        if(hovered){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextUnformatted(slot);ImGui::TextWrapped("%s",path.empty()?"Empty saved slot":ItemPresentation::Name(item).c_str());ImGui::TextUnformatted("Drag to preview only. Saved equipment is not changed.");HelpyImGuiTheme::EndDetailsTooltip();}ImGui::PopID();
    }ImGui::EndTable();}
}
inline void Studio(const Json& catalog,uint64_t revision,bool inventoryOnly=false){
    const auto& theme=HelpyImGuiTheme::Current;
    if(!inventoryOnly){
        ImGui::TextColored(theme.accent,"YOUR CHARACTER, ALL IN ONE PLACE");
        ImGui::TextWrapped("%s / Character workspace",StudioAppearance.value("Name",std::string("Choose a character")).c_str());
        ImGui::Separator();
    }
    std::string view=StudioPreviewMode;
    const double now=ImGui::GetTime();if(now-StudioLastDraw>1.5)StudioRequestedView.clear();StudioLastDraw=now;
    if(StudioView!=view){StudioView=view;StudioRequestedView.clear();CharacterPreviewPanel::Yaw=CharacterPreviewPanel::Pitch=0;CharacterPreviewPanel::Zoom=100;}
    if(!Opened){ImGui::TextWrapped("Choose a character save and open a copy above. Appearance, equipment fitting and item previews do not attach to the live player.");return;}
    if(!inventoryOnly){
        if(ImGui::Checkbox("Show character preview",&StudioShowPreview)&&StudioShowPreview)StudioRequestedView.clear();
        for(const auto& option:std::array<std::pair<const char*,const char*>,3>{{{"Full body","Body"},{"Portrait","Head"},{"Item","Item"}}}){ImGui::SameLine();if(ImGui::RadioButton(option.first,StudioPreviewMode==option.second)){StudioPreviewMode=option.second;StudioRequestedView.clear();CharacterPreviewPanel::Yaw=CharacterPreviewPanel::Pitch=0;CharacterPreviewPanel::Zoom=100;}}view=StudioPreviewMode;
        if(HelpyImGuiTheme::SoftButton("Inventory & Item Bay below"))FocusInventoryGrid=true;
    }
    if(!inventoryOnly&&StudioShowPreview&&StudioRequestedView!=view&&!StudioAppearance.empty()&&!AuthoringClient::Busy()){
        Json args={{"View",view},{"Equipment",std::string_view(view)=="Head"?Json::object():StudioEquipment}};if(std::string_view(view)=="Item")args["Path"]=StudioSelected.value("Path",std::string{});
        if(std::string_view(view)!="Item"||!StudioSelected.empty())if(AuthoringClient::Submit("SaveEditorRender",args))StudioRequestedView=view;
    }
    const bool wide=ImGui::GetContentRegionAvail().x>640;const int columns=wide?2:1;
    if(!inventoryOnly&&StudioShowPreview&&ImGui::BeginTable("##native-character-studio",columns,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)){
        if(wide){ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch,1.1f);ImGui::TableSetupColumn("Appearance tools",ImGuiTableColumnFlags_WidthStretch,1.3f);}ImGui::TableNextRow();ImGui::TableNextColumn();
        ImGui::SeparatorText("Preview Window");
        CharacterPreviewPanel::Draw(HelpyImGuiTheme::Current.Y(std::string_view(view)=="Body"?390:280));
        if(!CharacterPreviewPanel::DroppedItem.empty()){StudioSelected=CharacterPreviewPanel::DroppedItem;CharacterPreviewPanel::DroppedItem=Json::object();StudioPreviewMode="Item";StudioRequestedView.clear();}
        if(HelpyImGuiTheme::SoftButton("Refresh native preview"))StudioRequestedView.clear();
        if(wide)ImGui::TableNextColumn();ImGui::BeginDisabled(AuthoringClient::Busy());
        ImGui::TextUnformatted("Character name");ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##character-name","Character name",StudioName,sizeof(StudioName));
        if(StudioAppearance.contains("Revision"))if(Button("Stage character name","SaveEditorRename",{{"Name",StudioName},{"Revision",StudioAppearance.at("Revision")}}))Invalidate();
        if(ImGui::BeginTabBar("##character-tools")){
            if(ImGui::BeginTabItem("Appearance")){StudioAppearanceControls();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Loadout")){StudioGear();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Skills")){StudioSkillControls();ImGui::EndTabItem();}
            ImGui::EndTabBar();
        }
        if(std::string_view(view)=="Item"){ImGui::SeparatorText("Selected Item Bay preview");ImGui::TextWrapped("%s",StudioSelected.empty()?"Choose an item below":ItemPresentation::Name(StudioSelected).c_str());}
        ImGui::EndDisabled();ImGui::EndTable();
    }
    ImGui::SeparatorText("Inventory workbench · Item Bay → saved inventory");
    if(ImGui::BeginTable("##native-inventory-workbench",columns,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::PushID("saved-inventory-pane");ImGui::BeginDisabled(AuthoringClient::Busy());Inventory(true);ImGui::EndDisabled();
        if(!DroppedInventoryItem.empty()){StudioStageAddition(DroppedInventoryItem);DroppedInventoryItem=Json::object();}
        HelpyImGuiTheme::SoftButton("Drop Item Bay item here to stage an addition");if(ImGui::BeginDragDropTarget()){if(auto item=ItemGridPicker::AcceptItemDrop(Error))StudioStageAddition(*item);ImGui::EndDragDropTarget();}
        ImGui::PopID();if(wide)ImGui::TableNextColumn();ImGui::PushID("studio-item-bay-pane");ImGui::SeparatorText("Item Bay");Json bay=catalog;LazyCatalog::Overlay(bay,0);
        StudioBayControls();
        {
            const auto result=ItemGridPicker::Render("character-studio-bay",bay,StudioFilter,sizeof(StudioFilter),StudioSelectedPath,sizeof(StudioSelectedPath),StudioPicker,revision,false,false,true,true,false,420);
            if(result.SelectedRow){const auto previous=StudioSelected.value("Path",std::string{});StudioSelected=*result.SelectedRow;if(std::string_view(view)=="Item"&&previous!=StudioSelected.value("Path",std::string{}))StudioRequestedView.clear();}
        }
        ImGui::BeginDisabled(StudioSelected.empty());if(HelpyImGuiTheme::SoftButton("Add selected to save draft"))StudioStageAddition(StudioSelected);ImGui::EndDisabled();ImGui::PopID();ImGui::EndTable();
    }
    if(!StudioAddition.empty()){
        ImGui::SeparatorText("Review inventory addition");ImGui::TextWrapped("%s",ItemPresentation::Name(StudioAddition).c_str());ImGui::SetNextItemWidth(110);ImGui::InputInt("Quantity",&StudioQuantity,0,0);ImGui::TextWrapped("Destination: Inventory · first empty verified native slot");ImGui::TextWrapped("Special-tab insertion needs verified native eligibility. Personal storage: existing records remain editable; new-slot capacity is not verified.");ImGui::Checkbox("I confirm this addition to the save draft",&StudioConfirm);
        ImGui::BeginDisabled(!StudioConfirm||AuthoringClient::Busy());if(Button("Stage native item record","SaveEditorAddItem",{{"Path",StudioAddition.at("Path")},{"Count",StudioQuantity},{"Group","Inventory"},{"Personal",false},{"Confirm",true},{"Revision",StudioAppearance.value("Revision",uint64_t{})}})){StudioAddition=Json::object();Invalidate();}ImGui::EndDisabled();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel addition"))StudioAddition=Json::object();
    }
    StudioReview();StudioSlotPicker();
}
