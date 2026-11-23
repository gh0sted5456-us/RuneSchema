// Character sheet: one saved character copy, organised as Inventory & loadout, Appearance,
// Skills and Save data. Edits stage on an isolated copy; one draft bar above the pages shows
// whether the draft has changes and runs Review & export. The live character and native save
// slots are never written here.
enum class StudioPage{Inventory,Appearance,Skills,SaveData};
inline StudioPage StudioTab=StudioPage::Inventory,StudioShown=StudioPage::Inventory;
inline Json StudioAppearance=Json::object(),StudioSelected=Json::object(),StudioAddition=Json::object();
inline Json StudioSkills=Json::object(),StudioSavedLoadout=Json::array(),StudioSections=Json::object(),StudioTransferResult=Json::object();
inline bool StudioSkillsRequested=false,StudioSectionsRequested=false;
inline std::map<std::string,int> StudioSkillDrafts;
inline std::set<std::string> StudioSectionPicks;
inline std::string StudioBackupPick,StudioTransferTarget;
inline bool StudioSectionStore=false,StudioCleanConfirm=false,StudioTransferReplace=false;
inline uint64_t StudioReceipt=0,StudioGeneration=0,StudioExportedRevision=0;
inline char StudioName[129]{},StudioFilter[257]{},StudioSelectedPath[2049]{};
inline ItemGridPicker::State StudioPicker;
inline std::string StudioPaletteField="HairColor";
inline int StudioQuantity=1;inline bool StudioPersonal=false,StudioConfirm=false,StudioListRequested=false,StudioAppearanceRequested=false;
inline char StudioSaveSearch[129]{};
inline std::string StudioSaveName;
inline std::string StudioPendingSave,StudioPendingName,StudioPendingGuid;
inline bool StudioSwitchSave=false,StudioOpenPending=false;
inline std::string StudioLiveSeen,StudioLiveAttempt;
inline double StudioNextSaveRefresh=0;
inline const Json* StudioCurrentSave(const Json& list,const std::string& guid,const std::string& platform){
    const auto id=HelpyPersonalization::Guid(guid);if(id.empty()||!list.contains("Files")||!list["Files"].is_array())return nullptr;
    const bool xbox=platform=="Game Pass"||platform=="GamePass";
    const bool steam=platform=="Steam"||platform=="Epic"||platform=="Steam/Epic";
    const Json* match=nullptr;
    for(const auto& row:list["Files"]){
        if(HelpyPersonalization::Guid(row.value("CharacterGuid",std::string{}))!=id)continue;
        const auto provider=row.value("Provider",std::string{});
        if(provider=="Imported"||(xbox&&provider!="GamePass")||(steam&&provider!="SteamEpic"&&provider!=platform))continue;
        if(match)return nullptr;match=&row;
    }return match;
}
inline void StudioSelectSave(const std::string& path,const std::string& name,const std::string& guid={}){
    if(Opened&&path==File&&(guid.empty()||HelpyPersonalization::Guid(guid)==HelpyPersonalization::Guid(Browser.value("CharacterGuid",std::string{}))))return;
    if(Opened){StudioPendingSave=path;StudioPendingName=name;StudioPendingGuid=guid;StudioSwitchSave=true;return;}
    AuthoringPanel::Copy(File,sizeof(File),path);StudioSaveName=name;Invalidate();
    if(!AuthoringClient::Busy()){Json args={{"Path",File},{"CopyOnly",true}};if(!guid.empty())args["CharacterGuid"]=guid;AuthoringClient::Submit("SaveEditorOpen",args);}
}
inline void StudioForget(){
    StudioAppearance=Json::object();StudioAppearanceRequested=false;StudioSkillsRequested=false;StudioSkills=Json::object();StudioSkillDrafts.clear();
    StudioSavedLoadout=Json::array();StudioSections=Json::object();StudioSectionsRequested=false;StudioSectionPicks.clear();StudioBackupPick.clear();StudioCleanConfirm=false;
    StudioTransferResult=Json::object();StudioTransferReplace=false;StudioAddition=Json::object();StudioSelected=Json::object();
}
// The draft's revision rises with every staged edit (the core counts them). The newest
// revision any save-editor reply reported, against the one the copy opened at, tells whether
// the draft has changes; replies from an earlier copy are always older than its open.
inline uint64_t StudioDraftRevision(){
    uint64_t latest=0;
    for(const auto* op:{"SaveEditorOpen","SaveEditorBrowse","SaveEditorSet","SaveEditorAppearance","SaveEditorAppearanceSet","SaveEditorRename",
        "SaveEditorInventory","SaveEditorStack","SaveEditorRemoveItems","SaveEditorAddItem","SaveEditorSkills","SaveEditorSkillSet",
        "SaveEditorSections","SaveEditorSectionRestore","SaveEditorSectionClean"}){
        const auto& result=AuthoringClient::Result(op);const auto found=result.find("Revision");
        if(found!=result.end()&&found->is_number_unsigned())latest=std::max(latest,found->get<uint64_t>());
    }return latest;
}
inline uint64_t StudioOpenRevision(){
    const auto& opened=AuthoringClient::Result("SaveEditorOpen");const auto found=opened.find("Revision");
    return found!=opened.end()&&found->is_number_unsigned()?found->get<uint64_t>():0;
}
inline void StudioApply(){
    const auto generation=AuthoringClient::Snapshot.value("Generation",uint64_t{});
    if(generation!=StudioGeneration){StudioGeneration=generation;StudioReceipt=0;StudioForget();StudioListRequested=false;StudioLiveSeen.clear();StudioLiveAttempt.clear();StudioOpenPending=StudioSwitchSave=false;}
    const auto id=AuthoringClient::Snapshot.value("RequestId",uint64_t{});if(!id||id==StudioReceipt)return;StudioReceipt=id;
    const auto op=AuthoringClient::Snapshot.value("Operation",std::string{});const auto& result=AuthoringClient::Result(op.c_str());
    if(result.contains("Error")){if(op=="SaveEditorTransfer")StudioTransferResult=result;return;}
    if(op=="SaveEditorOpen"||op=="SaveEditorClose")StudioForget();
    if(op=="SaveEditorAppearance"||op=="SaveEditorAppearanceSet"||op=="SaveEditorRename"){
        StudioAppearance=result;AuthoringPanel::Copy(StudioName,sizeof(StudioName),result.value("Name",std::string{}));
        if(result.contains("Revision"))Browser["Revision"]=result["Revision"];
        if(op!="SaveEditorAppearance"){ResetInventory();Invalidate();}
    }
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"||op=="SaveEditorAddItem")if(result.contains("Revision"))StudioAppearance["Revision"]=result["Revision"];
    if(op=="SaveEditorSkills"||op=="SaveEditorSkillSet"){StudioSkills=result;StudioSkillDrafts.clear();if(result.contains("Revision"))StudioAppearance["Revision"]=result["Revision"];if(op=="SaveEditorSkillSet"){ResetInventory();Invalidate();}}
    if(op=="SaveEditorInventory"&&result.contains("Loadout"))StudioSavedLoadout=result["Loadout"];
    if(op.starts_with("SaveEditorSection")){
        StudioSections=result;if(result.contains("Revision"))StudioAppearance["Revision"]=result["Revision"];
        if(op=="SaveEditorSectionRestore"||op=="SaveEditorSectionClean"){ResetInventory();Invalidate();StudioCleanConfirm=false;StudioSectionPicks.clear();}
    }
    if(op=="SaveEditorTransfer"){StudioTransferResult=result;if(!result.value("NeedsConfirm",false))StudioTransferReplace=false;}
    if(op=="SaveEditorExport"&&result.contains("File"))StudioExportedRevision=StudioDraftRevision();
}
inline void StudioHeader(){
    const auto live=FollowCurrentCharacter?LiveCharacterGuid:std::string{};
    if(live!=StudioLiveSeen){StudioLiveSeen=live;StudioLiveAttempt.clear();StudioListRequested=false;StudioNextSaveRefresh=0;}
    const double now=ImGui::GetTime();
    const bool missing=!live.empty()&&StudioLiveAttempt!=live&&!StudioCurrentSave(AuthoringClient::Result("SaveEditorList"),live,LiveCharacterPlatform);
    // Xbox can publish metadata before its first gameplay save. Retry only an unmatched visible character.
    if((!StudioListRequested||missing)&&now>=StudioNextSaveRefresh&&!AuthoringClient::Busy()){
        StudioNextSaveRefresh=now+10;
        StudioListRequested=AuthoringClient::Submit("SaveEditorList");
    }
    const auto& list=AuthoringClient::Result("SaveEditorList");
    if(StudioOpenPending&&!Opened&&!AuthoringClient::Busy()){
        Json args={{"Path",File},{"CopyOnly",true}};if(!StudioPendingGuid.empty())args["CharacterGuid"]=StudioPendingGuid;
        if(AuthoringClient::Submit("SaveEditorOpen",args))StudioOpenPending=false;
    }
    if(!live.empty()&&StudioLiveAttempt!=live&&!AuthoringClient::Busy()){
        if(const auto* match=StudioCurrentSave(list,live,LiveCharacterPlatform)){
            const auto path=match->at("Path").get<std::string>(),name=match->at("Name").get<std::string>();
            if(Opened){StudioLiveAttempt=live;if(HelpyPersonalization::Guid(Browser.value("CharacterGuid",std::string{}))!=HelpyPersonalization::Guid(live))StudioSelectSave(path,name,live);}
            else if(AuthoringClient::Submit("SaveEditorOpen",{{"Path",path},{"CopyOnly",true},{"CharacterGuid",live}})){
                AuthoringPanel::Copy(File,sizeof(File),path);StudioSaveName=name;StudioLiveAttempt=live;Invalidate();
            }
        }
    }
    if(ImGui::Checkbox("Follow current character",&FollowCurrentCharacter)){StudioLiveSeen.clear();StudioLiveAttempt.clear();}
    if(FollowCurrentCharacter&&!live.empty()&&!Opened&&!AuthoringClient::Busy())ImGui::TextDisabled("Waiting for a matching gameplay save · checking automatically");
    ImGui::SetNextItemWidth(std::max(200.f,ImGui::GetContentRegionAvail().x*.6f));
    const auto current=File[0]?(StudioSaveName.empty()?std::filesystem::path(File).filename().string():StudioSaveName):std::string("Choose a character save");
    if(ImGui::BeginCombo("##character-studio-save",current.c_str())){
        HelpyTextInput::InputTextWithHint("##filter-saves","Find a character…",StudioSaveSearch,sizeof(StudioSaveSearch));
        if(list.contains("Files"))for(const auto& file:list["Files"]){const auto path=file.at("Path").get<std::string>(),name=file.at("Name").get<std::string>();
            const auto text=HelpyCacheSources::Lower(name+" "+file.value("Storage",std::string{})+" "+file.value("FileName",std::string{}));if(text.find(HelpyCacheSources::Lower(StudioSaveSearch))==text.npos)continue;
            ImGui::PushID(path.c_str());ProviderIcon(file);if(ImGui::Selectable(name.c_str(),path==File)){FollowCurrentCharacter=false;StudioSelectSave(path,name);}
            if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("%s\n%s",file.value("Storage",std::string{}).c_str(),file.value("FileName",name).c_str());ImGui::PopID();}
        if(!list.contains("Files")||list["Files"].empty())ImGui::TextDisabled("No character saves found. Refresh or import a JSON copy.");ImGui::EndCombo();}
    if(ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(250))ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Import JSON...")){const std::string before=File;Choose();const std::string picked=File;AuthoringPanel::Copy(File,sizeof(File),before);if(picked!=before){FollowCurrentCharacter=false;StudioSelectSave(picked,std::filesystem::path(picked).stem().string());}}ImGui::SameLine();if(Button("Refresh saves","SaveEditorList")){StudioListRequested=true;StudioLiveAttempt.clear();StudioNextSaveRefresh=now+10;}
    if(StudioSwitchSave){ImGui::OpenPopup("Switch character?");StudioSwitchSave=false;}
    if(ImGui::BeginPopupModal("Switch character?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Discard the current draft and select %s? Original saves stay untouched.",StudioPendingName.c_str());
        ImGui::BeginDisabled(AuthoringClient::Busy());if(ImGui::Button("Discard draft and switch")){
            if(AuthoringClient::Submit("SaveEditorClose")){AuthoringPanel::Copy(File,sizeof(File),StudioPendingSave);StudioSaveName=StudioPendingName;StudioOpenPending=true;Invalidate();ImGui::CloseCurrentPopup();}
        }ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Keep current draft"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::BeginDisabled(!File[0]||AuthoringClient::Busy());if(HelpyImGuiTheme::SoftButton(Opened?"Reload saved copy":"Open saved copy")){
        if(Opened){StudioPendingSave=File;StudioPendingName=StudioSaveName;StudioPendingGuid=Browser.value("CharacterGuid",std::string{});StudioSwitchSave=true;}
        else if(AuthoringClient::Submit("SaveEditorOpen",{{"Path",File},{"CopyOnly",true}}))Invalidate();
    }ImGui::EndDisabled();
    if(list.contains("Files"))for(const auto& file:list["Files"])if(file.value("Path",std::string{})==File){ProviderIcon(file);ImGui::TextDisabled("%s",file.value("Storage",std::string{}).c_str());break;}
    ImGui::TextDisabled("Saved snapshot · edits export a copy, not the live character");
    if(Opened&&!StudioAppearanceRequested&&!AuthoringClient::Busy())StudioAppearanceRequested=AuthoringClient::Submit("SaveEditorAppearance");
    if(Opened&&StudioAppearance.empty()&&StudioAppearanceRequested&&HelpyImGuiTheme::SoftButton("Retry native appearance choices"))StudioAppearanceRequested=false;
}
// One draft bar for every page: whether the draft has changes, Review & export, and Discard
// (which reopens the saved copy). The review appears under the bar until the draft changes.
inline void StudioReview();
inline void StudioDraftBar(){
    const auto& theme=HelpyImGuiTheme::Current;const auto latest=StudioDraftRevision();
    const bool changed=latest>StudioOpenRevision(),exported=changed&&StudioExportedRevision==latest;
    if(HelpyImGuiTheme::BeginPanel("##character-draft-bar",ImVec2(0,ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().WindowPadding.y*2),true,ImGuiWindowFlags_AlwaysUseWindowPadding)){
        ImGui::AlignTextToFramePadding();
        if(!changed)ImGui::TextDisabled("No changes in this draft yet");
        else if(exported)ImGui::TextColored(theme.success,"Draft exported · further edits need another export");
        else ImGui::TextColored(theme.warning,"This draft has changes that are not exported yet");
        const float buttons=ImGui::CalcTextSize("Review & exportDiscard changes").x+ImGui::GetStyle().FramePadding.x*4+ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX()+ImGui::GetStyle().ItemSpacing.x,ImGui::GetWindowContentRegionMax().x-buttons));
        ImGui::BeginDisabled(!changed||AuthoringClient::Busy());
        if(Button("Review & export","SaveEditorPreview"))Invalidate();
        ImGui::SameLine();
        // Discard closes the draft and reopens the same saved copy; the original never changed.
        if(Button("Discard changes","SaveEditorClose")){StudioPendingGuid=Browser.value("CharacterGuid",std::string{});StudioOpenPending=true;Invalidate();}
        ImGui::EndDisabled();
    }HelpyImGuiTheme::EndPanel();
    if(!Token.empty())StudioReview();
}
inline void StudioReview(){
    if(!Token.empty()){
        ImGui::SeparatorText("Review edited copy");if(Review.contains("Summary"))for(const auto& row:Review["Summary"]){ImGui::TextWrapped("%s",row.at("Label").get_ref<const std::string&>().c_str());ImGui::TextDisabled("%s",row.at("Field").get_ref<const std::string&>().c_str());if(row.contains("Before")||row.contains("After"))ImGui::TextWrapped("%s → %s",row.value("Before",Json{}).dump().c_str(),row.value("After",Json{}).dump().c_str());}
        ImGui::Checkbox("I reviewed these changes; export a new copy",&Confirmed);ImGui::BeginDisabled(!Confirmed);Button("Export edited character copy","SaveEditorExport",{{"Token",Token},{"Confirm",Confirmed}});ImGui::EndDisabled();
    }
    const auto& exported=AuthoringClient::Result("SaveEditorExport");if(exported.contains("File"))ImGui::TextWrapped("Export: %s",exported.at("File").get_ref<const std::string&>().c_str());
}
inline void StudioStageAddition(const Json& item){StudioAddition=item;StudioQuantity=1;StudioConfirm=false;}
inline void StudioBayControls(const Json& catalog,uint64_t revision){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BaySearchId=ImGui::GetID("Search");HelpyPreviewHooks::BayRefreshId=ImGui::GetID("Refresh");
#endif
    ItemBayBrowser::Controls(StudioPicker,catalog,revision,StudioFilter,sizeof(StudioFilter));
}
inline void StudioPaintKit(){
    const auto& theme=HelpyImGuiTheme::Current;ImGui::SeparatorText("Painter's kit · native palette");
    if(ImGui::BeginTabBar("##native-paint-fields")){for(const auto& field:StudioAppearance["Choices"].at("Fields")){
        const auto key=field.at("Key").get<std::string>();if(key!="SkinTone"&&key!="EyeColor"&&key!="HairColor"&&key!="EyebrowColor")continue;
        const auto title=key=="SkinTone"?"Skin":key=="EyeColor"?"Eyes":key=="HairColor"?"Hair":"Eyebrows";
        if(ImGui::BeginTabItem(title)){StudioPaletteField=key;const auto current=StudioAppearance["Appearance"].at(key).at("rowName").get<std::string>();ImGui::TextWrapped("%s · %s",field.at("Label").get_ref<const std::string&>().c_str(),current.c_str());
            const float size=theme.Y(38),gap=ImGui::GetStyle().ItemSpacing.x,available=ImGui::GetContentRegionAvail().x;const int columns=std::max(1,int((available+gap)/(size+gap)));int i=0;
            for(const auto& value:field.at("Rows")){const auto row=value.get<std::string>();ImGui::PushID(row.c_str());const auto a=ImGui::GetCursorScreenPos();const bool clicked=ImGui::InvisibleButton("##paint-well",{size,size},ImGuiButtonFlags_EnableNav);const bool selected=row==current,hovered=ImGui::IsItemHovered();const ImVec2 center(a.x+size*.5f,a.y+size*.5f);auto* draw=ImGui::GetWindowDrawList();
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
// Saved loadout, read-only: equipment changes belong to the live player's gear menu.
// Compact: a short strip above the inventory, read with the inventory page.
inline void StudioGear(bool compact=false){
    const auto& theme=HelpyImGuiTheme::Current;const float avail=ImGui::GetContentRegionAvail().x;
    if(compact&&!ImGui::CollapsingHeader("Saved loadout · read-only",ImGuiTreeNodeFlags_DefaultOpen))return;
    if(!compact)ImGui::SeparatorText("Saved loadout");
    const int columns=compact?(avail>theme.X(900)?6:avail>theme.X(450)?4:3):(avail>theme.X(450)?3:2);
    const float placard=theme.Y(compact?72:108),glyphOffset=theme.Y(compact?40:48),glyphSize=theme.Y(compact?22:28);
    if(ImGui::BeginTable("##saved-loadout-slots",columns,ImGuiTableFlags_SizingStretchSame)){
    for(const auto* slot:{"Head","Body","Legs","Cape","Trinket","Trinket2","Ammo","MagicAmmo1","HeldRight","HeldLeft","CrossbowBolts","FishingBait"}){
        ImGui::TableNextColumn();
        const std::string_view location=slot;const auto glyph=location=="Head"?"helmet":location=="Body"||location=="Cape"?"body":location=="Legs"?"legs":location=="Trinket"||location=="Trinket2"?"gem":location=="MagicAmmo1"?"runecrafting":location=="HeldRight"?"sword":location=="HeldLeft"?"shield":location=="FishingBait"?"fish":"bow";
        const auto title=location=="Trinket2"?"Trinket 2":location=="MagicAmmo1"?"Runes":location=="HeldRight"?"Main hand":location=="HeldLeft"?"Off hand":location=="CrossbowBolts"?"Bolts":location=="FishingBait"?"Bait":slot;
        ImGui::PushID(slot);Json item=Json::object();for(const auto& row:StudioSavedLoadout)if(row.value("Location",std::string{})==slot)item=SavedPlacard(row);
        const auto path=item.value("Path",std::string{});
        const auto a=ImGui::GetCursorScreenPos();const ImVec2 b(a.x+ImGui::GetContentRegionAvail().x,a.y+placard);ImGui::Dummy({b.x-a.x,b.y-a.y});const bool hovered=ImGui::IsItemHovered();
        if(!path.empty())ItemGridPicker::DrawPlacard(item,a,b,false,hovered);
        else{auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,b,ImGui::GetColorU32(theme.panelAlt),4);draw->AddRect(a,b,ImGui::GetColorU32(theme.border),4);MapLayerIcons::Draw(glyph,{(a.x+b.x)*.5f,a.y+glyphOffset},glyphSize);}
        auto* draw=ImGui::GetWindowDrawList();const auto extent=ImGui::CalcTextSize(title);draw->AddRectFilled(a,{b.x,a.y+theme.Y(18)},ImGui::GetColorU32(theme.panelAlt),3);MapLayerIcons::Draw(glyph,{a.x+theme.X(10),a.y+theme.Y(9)},theme.Y(13));draw->AddText({std::max(a.x+theme.X(20),(a.x+b.x-extent.x)*.5f),a.y+theme.Y(2)},ImGui::GetColorU32(theme.accent),title);
        if(hovered&&HelpyImGuiTheme::TooltipReady(std::string("loadout:")+slot,hovered)){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextUnformatted(title);ImGui::TextWrapped("%s",path.empty()?"Empty saved slot":ItemPresentation::Name(item).c_str());HelpyImGuiTheme::EndDetailsTooltip();}
        ImGui::PopID();
    }ImGui::EndTable();}
    if(StudioSavedLoadout.empty())ImGui::TextDisabled("Reading the saved loadout with the inventory…");
}
inline void StudioInventory(const Json& catalog,uint64_t revision){
    const bool wide=ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(640);const int columns=wide?2:1;
    if(ImGui::BeginTable("##native-inventory-workbench",columns,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::PushID("saved-inventory-pane");ImGui::SeparatorText("Saved inventory");Inventory(true);
        if(!DroppedInventoryItem.empty()){StudioStageAddition(DroppedInventoryItem);DroppedInventoryItem=Json::object();}
        HelpyImGuiTheme::SoftButton("Drop an Item Bay item here to add it");if(ImGui::BeginDragDropTarget()){if(auto item=ItemGridPicker::AcceptItemDrop(Error))StudioStageAddition(*item);ImGui::EndDragDropTarget();}
        ImGui::PopID();if(wide)ImGui::TableNextColumn();ImGui::PushID("studio-item-bay-pane");ImGui::SeparatorText("Item Bay");Json bay=catalog;LazyCatalog::Overlay(bay,0);
        const auto bayRevision=revision+LazyCatalog::For(0).RowsRevision;
        StudioBayControls(bay,bayRevision);
        {
            const auto result=ItemGridPicker::Render("character-studio-bay",bay,StudioFilter,sizeof(StudioFilter),StudioSelectedPath,sizeof(StudioSelectedPath),StudioPicker,bayRevision,false,false,true,true,false,420);
            // Picking an item makes it the one to add; the row below adds it in one click.
            if(result.SelectedRow&&*result.SelectedRow!=StudioSelected){StudioSelected=*result.SelectedRow;StudioStageAddition(StudioSelected);}
        }
        ImGui::PopID();ImGui::EndTable();
    }
    // Add an item in one step: pick it (or drop it), set the quantity, Add. It lands in the draft;
    // Review & export is where the whole draft is confirmed.
    if(!StudioAddition.empty()){
        ImGui::Separator();ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(("Add "+ItemPresentation::Name(StudioAddition)).c_str());ImGui::SameLine();
        StudioQuantity=std::clamp(StudioQuantity,1,9999);ImGui::SetNextItemWidth(HelpyImGuiTheme::Current.X(110));ImGui::InputInt("##add-quantity",&StudioQuantity,1,10);StudioQuantity=std::clamp(StudioQuantity,1,9999);
        ImGui::SameLine();ImGui::BeginDisabled(AuthoringClient::Busy()||!StudioAppearance.contains("Revision"));
        if(Button("Add to draft","SaveEditorAddItem",{{"Path",StudioAddition.at("Path")},{"Count",StudioQuantity},{"Group","Inventory"},{"Personal",false},{"Confirm",true},{"Revision",StudioAppearance.value("Revision",uint64_t{})}})){StudioAddition=Json::object();StudioSelected=Json::object();Invalidate();}
        ImGui::EndDisabled();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))StudioAddition=Json::object();
        ImGui::TextDisabled("Goes to the first empty inventory slot. Personal storage and special tabs keep their existing records.");
    }
}
inline void StudioAppearancePage(){
    // The name stages like every other appearance choice: when you finish editing it (Enter,
    // clicking away, or the controller keyboard's OK), not through a separate button.
    static bool editingName=false;
    ImGui::TextUnformatted("Character name");ImGui::SetNextItemWidth(-1);
    const ImGuiID nameId=ImGui::GetID("##character-name");
    HelpyTextInput::InputTextWithHint("##character-name","Character name",StudioName,sizeof(StudioName));
    const bool editing=ImGui::GetCurrentContext()->ActiveId==nameId||HelpyTextInput::Owner==nameId;
    const auto savedName=StudioAppearance.value("Name",std::string{});
    if(editingName&&!editing&&StudioName[0]&&savedName!=StudioName&&StudioAppearance.contains("Revision")&&!AuthoringClient::Busy())
        if(AuthoringClient::Submit("SaveEditorRename",{{"Name",StudioName},{"Revision",StudioAppearance.at("Revision")}}))Invalidate();
    editingName=editing;
    if(!editing&&StudioName[0]&&savedName!=StudioName&&!savedName.empty())ImGui::TextDisabled("Not staged yet: click the name and press Enter.");
    else ImGui::TextDisabled("Edit the name and press Enter to stage it.");
    if(StudioAppearance.empty()){ImGui::TextDisabled("Reading native appearance choices…");return;}
    StudioAppearanceControls();
}
inline std::string StudioGuid(){return DeliveryFlagsPanel::F::NormalizeGuid(Browser.value("CharacterGuid",std::string{}));}
// Deliveries for this character: the player's own switch, and every mod pause that applies
// to it or its world. The core enforces these; the page explains and records choices.
inline void StudioDeliveries(){
    namespace F=DeliveryFlagsPanel::F;DeliveryFlagsPanel::Refresh();const auto& snapshot=DeliveryFlagsPanel::Snapshot;
    const auto guid=StudioGuid();if(guid.empty()){ImGui::TextDisabled("This save has no character identity.");return;}
    const auto name=StudioAppearance.value("Name",StudioSaveName);
    ImGui::SeparatorText("Helpy item delivery");
    bool allowed=F::UserAllows(snapshot.State,guid);
    if(HelpyImGuiTheme::Toggle("Allow Helpy deliveries to this character",&allowed))
        DeliveryFlagsPanel::Apply([&]{F::SetUserDeliveries(DeliveryFlagsPanel::Folder(),guid,allowed,DeliveryFlagsPanel::Now(),name);},allowed?"Deliveries are on for this character.":"Deliveries are off for this character.");
    ImGui::TextWrapped("%s",allowed?"Items you send to yourself from Helpy arrive normally.":"Helpy refuses deliveries you make to this character. A server host can still give you items.");
    ImGui::SeparatorText("Mods that pause Helpy for this character");
    size_t shown=0;
    for(const auto* list:{&snapshot.Active,&snapshot.Declined})for(const auto& flag:*list){
        if(flag.Target=="character"&&F::NormalizeGuid(flag.Id)!=guid)continue;
        ++shown;const auto mod=F::ModName(snapshot,flag.Mod);
        ImGui::BulletText("%s · %s",mod.c_str(),flag.Target=="world"?("world "+DeliveryFlagsPanel::Short(flag.Id)).c_str():"this character");
        ImGui::Indent();ImGui::TextWrapped("%s",flag.Message.empty()?"Deliveries paused.":flag.Message.c_str());
        if(list==&snapshot.Declined)ImGui::TextDisabled("You stopped honouring this mod's pauses, so it does not block deliveries.");
        ImGui::Unindent();
    }
    if(!shown)ImGui::TextDisabled("No mod pauses deliveries to this character.");
    ImGui::TextDisabled("Manage which mods may pause deliveries in Settings › Deliveries.");
    if(!DeliveryFlagsPanel::Status.empty())ImGui::TextWrapped("%s",DeliveryFlagsPanel::Status.c_str());
}
// Deliveries, folded into the inventory page: the header says whether Helpy can deliver to
// this character; opening it shows the switch and any mod pauses.
inline void StudioDeliverySummary(){
    namespace F=DeliveryFlagsPanel::F;DeliveryFlagsPanel::Refresh();const auto guid=StudioGuid();if(guid.empty())return;
    const auto block=F::Check(DeliveryFlagsPanel::Snapshot,guid,{});
    const bool worldPauses=std::any_of(DeliveryFlagsPanel::Snapshot.Active.begin(),DeliveryFlagsPanel::Snapshot.Active.end(),[](const F::Flag& flag){return flag.Target=="world";});
    std::string label="Helpy deliveries: ";
    if(block)label+=block->User?"off for this character":"paused by "+block->ModName;
    else label+=worldPauses?"on (paused in some worlds)":"on";
    label+="###studio-deliveries";
    if(block)ImGui::PushStyleColor(ImGuiCol_Text,HelpyImGuiTheme::Current.warning);
    const bool open=ImGui::CollapsingHeader(label.c_str());
    if(block)ImGui::PopStyleColor();
    if(open){ImGui::Indent();StudioDeliveries();ImGui::Unindent();}
}
inline std::string StudioBytes(uint64_t bytes){char text[32]{};if(bytes<1024)std::snprintf(text,sizeof(text),"%llu B",static_cast<unsigned long long>(bytes));else std::snprintf(text,sizeof(text),"%.1f KiB",bytes/1024.0);return text;}
inline Json StudioPickedOwners(){Json owners=Json::array();for(const auto& owner:StudioSectionPicks)owners.push_back(owner);return owners;}
// J4SSaves sections: what mods keep with this character, where each copy lives, and backup,
// restore or clean. Restores and cleans stage on the copy like any other edit.
inline void StudioSectionsPage(){
    if(!StudioSectionsRequested&&!AuthoringClient::Busy())StudioSectionsRequested=AuthoringClient::Submit("SaveEditorSections");
    if(!StudioSections.contains("Sections")){ImGui::TextDisabled("Reading save sections…");if(StudioSectionsRequested&&HelpyImGuiTheme::SoftButton("Retry"))StudioSectionsRequested=false;return;}
    const bool store=StudioSections.value("StoreAvailable",false);
    ImGui::TextWrapped("J4SSaves lets mods keep their own data with a character. Sections travel inside the save, and J4SSaves also keeps a local copy.");
    ImGui::TextDisabled("J4SSaves store: %s",StudioSections.value("Store",std::string("unavailable")).c_str());
    const auto& rows=StudioSections["Sections"];
    if(rows.empty())ImGui::TextDisabled("This character has no J4SSaves sections.");
    else if(ImGui::BeginTable("##j4s-sections",5,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg)){
        ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,HelpyImGuiTheme::Current.X(28));ImGui::TableSetupColumn("Mod section",ImGuiTableColumnFlags_WidthStretch,2.f);
        ImGui::TableSetupColumn("Size");ImGui::TableSetupColumn("Schema · revision");ImGui::TableSetupColumn("Kept in");ImGui::TableHeadersRow();
        for(const auto& row:rows){
            const auto owner=row.value("Owner",std::string{});ImGui::PushID(owner.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();
            bool picked=StudioSectionPicks.contains(owner);const bool valid=row.value("Valid",false);
            ImGui::BeginDisabled(!valid);if(ImGui::Checkbox("##pick",&picked)){if(picked)StudioSectionPicks.insert(owner);else StudioSectionPicks.erase(owner);StudioCleanConfirm=false;}ImGui::EndDisabled();
            ImGui::TableNextColumn();ImGui::TextUnformatted(owner.c_str());if(!valid){ImGui::SameLine();ImGui::TextDisabled("(damaged)");}
            ImGui::TableNextColumn();ImGui::TextUnformatted(StudioBytes(row.value("Bytes",uint64_t{})).c_str());
            ImGui::TableNextColumn();ImGui::Text("%lld · %lld",static_cast<long long>(row.value("Schema",int64_t{})),static_cast<long long>(row.value("Revision",int64_t{})));
            ImGui::TableNextColumn();const bool inSave=row.value("InSave",false);const auto storedRevision=row.value("StoredRevision",int64_t{});
            ImGui::TextUnformatted(inSave&&storedRevision?"Save + J4SSaves":inSave?"Save only":"J4SSaves only");
            if(inSave&&storedRevision&&storedRevision!=row.value("Revision",int64_t{})&&ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("J4SSaves holds revision %lld; the newer copy wins on the next load.",static_cast<long long>(storedRevision));
            ImGui::PopID();
        }ImGui::EndTable();
    }
    if(store)ImGui::Checkbox("Also apply to the J4SSaves store",&StudioSectionStore);else StudioSectionStore=false;
    if(store&&ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Without this, only the save copy changes. J4SSaves keeps its own copy and may put it back on the next save.");
    const auto revision=StudioAppearance.value("Revision",StudioSections.value("Revision",uint64_t{}));
    ImGui::BeginDisabled(StudioSectionPicks.empty()||AuthoringClient::Busy());
    Button("Back up selected","SaveEditorSectionBackup",{{"Owners",StudioPickedOwners()}});ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Clean selected…"))StudioCleanConfirm=true;
    ImGui::EndDisabled();
    if(StudioCleanConfirm&&!StudioSectionPicks.empty()){
        ImGui::TextWrapped("Remove %zu section(s) from the save copy%s? The mods lose this data unless you restore a backup.",StudioSectionPicks.size(),StudioSectionStore?" and the J4SSaves store":"");
        ImGui::BeginDisabled(AuthoringClient::Busy());if(Button("Remove sections","SaveEditorSectionClean",{{"Owners",StudioPickedOwners()},{"Store",StudioSectionStore},{"Revision",revision}}))StudioCleanConfirm=false;ImGui::EndDisabled();
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Keep them"))StudioCleanConfirm=false;
    }
    ImGui::SeparatorText("Backups");
    const auto backups=StudioSections.value("Backups",Json::array());
    if(backups.empty())ImGui::TextDisabled("No backups yet. Backups are kept with this character's profile files.");
    for(const auto& backup:backups){
        const auto file=backup.value("File",std::string{});ImGui::PushID(file.c_str());
        if(backup.value("Damaged",false)){ImGui::TextDisabled("%s · damaged",file.c_str());ImGui::PopID();continue;}
        std::string owners;for(const auto& section:backup.value("Sections",Json::array())){if(!owners.empty())owners+=", ";owners+=section.value("Owner",std::string{});}
        if(ImGui::Selectable((backup.value("CreatedAt",file)+"  ·  "+owners).c_str(),StudioBackupPick==file))StudioBackupPick=file;
        ImGui::PopID();
    }
    for(const auto& backup:backups)if(backup.value("File",std::string{})==StudioBackupPick&&!StudioBackupPick.empty()){
        Json owners=Json::array();for(const auto& section:backup.value("Sections",Json::array()))owners.push_back(section.value("Owner",std::string{}));
        ImGui::BeginDisabled(AuthoringClient::Busy()||owners.empty());
        Button("Restore this backup","SaveEditorSectionRestore",{{"File",StudioBackupPick},{"Owners",owners},{"Store",StudioSectionStore},{"Revision",revision}});
        ImGui::EndDisabled();ImGui::TextDisabled("Restored sections get a new revision so they win over older copies.");
    }
    const auto status=StudioSections.value("Status",std::string{});if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
}
// Steam/Epic and Game Pass use the same character JSON. Steam gets a plain save file; Game
// Pass gets the copy through the J4SSaves importer, which creates a new Xbox slot.
inline void StudioTransfer(){
    ImGui::SeparatorText("Move this character between Steam and Game Pass");
    ImGui::TextWrapped("Both versions store the same character data. Use the same gameplay mods on both, and transfer the saved copy, not a draft with unexported edits.");
    const auto submit=[](const char* target,bool replace){StudioTransferTarget=target;Json args={{"Target",target},{"Replace",replace}};if(!LiveCharacterGuid.empty())args["LiveCharacterGuid"]=LiveCharacterGuid;AuthoringClient::Submit("SaveEditorTransfer",args);};
    ImGui::BeginDisabled(AuthoringClient::Busy()||!Opened);
    if(HelpyImGuiTheme::SoftButton("Send to Steam / Epic"))submit("Steam",false);
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Send to Game Pass (J4SSaves)"))submit("Xbox",false);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Steam: writes a save file in Steam's character folder; Steam Cloud uploads it.");
    ImGui::TextDisabled("Game Pass: queues the copy; start Game Pass with J4SSaves and open character selection.");
    if(StudioTransferResult.value("NeedsConfirm",false)){
        ImGui::TextWrapped("%s",StudioTransferResult.value("Status",std::string{}).c_str());
        ImGui::BeginDisabled(AuthoringClient::Busy());if(HelpyImGuiTheme::SoftButton("Replace the Steam save"))submit("Steam",true);ImGui::EndDisabled();
    }else if(StudioTransferResult.contains("Error"))ImGui::TextWrapped("%s",StudioTransferResult.at("Error").get_ref<const std::string&>().c_str());
    else if(StudioTransferResult.contains("Status"))ImGui::TextWrapped("%s",StudioTransferResult.value("Status",std::string{}).c_str());
    ImGui::TextDisabled("Use Review & export above to save edits as a new copy before transferring it.");
}
inline bool StudioTabItem(const char* label,StudioPage page){
    const bool pending=StudioTab!=StudioShown;
    if(!ImGui::BeginTabItem(label,nullptr,pending&&page==StudioTab?ImGuiTabItemFlags_SetSelected:0))return false;
    if(!pending||page==StudioTab)StudioTab=page;
    StudioShown=page;return true;
}
// The sheet itself. Pages that stage edits end with the shared review row.
inline void Studio(const Json& catalog,uint64_t revision){
    if(!Opened){ImGui::TextWrapped("Choose a saved character above.");return;}
    StudioDraftBar();
    if(ImGui::BeginTabBar("##character-sheet")){
        if(StudioTabItem("Inventory & loadout",StudioPage::Inventory)){StudioDeliverySummary();StudioGear(true);StudioInventory(catalog,revision);ImGui::EndTabItem();}
        if(StudioTabItem("Appearance",StudioPage::Appearance)){StudioAppearancePage();ImGui::EndTabItem();}
        if(StudioTabItem("Skills",StudioPage::Skills)){StudioSkillControls();ImGui::EndTabItem();}
        if(StudioTabItem("Save data",StudioPage::SaveData)){
            ImGui::SeparatorText("Mod save sections");StudioSectionsPage();
            StudioTransfer();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
