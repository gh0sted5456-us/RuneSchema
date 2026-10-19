// Included after SaveAuthoring.inl: uses the bounded encoding-aware reader and
// unique verified-copy writer. Never opens the WGS database for writing.
namespace {
struct CharacterEditorState {
    fs::path Path;fs::file_time_type Time{};uintmax_t Size=0;
    CharacterSaveEditor::Session Session;
    json Choices=json::object();
    json Layout=json::array();
} characterEditor;
json EditorSource(){
    if(!characterEditor.Session.Ready())throw std::runtime_error("Open a character copy first");
    if(fs::last_write_time(characterEditor.Path)!=characterEditor.Time||fs::file_size(characterEditor.Path)!=characterEditor.Size)
        throw std::runtime_error("Source save changed; reopen it before editing/exporting");
    auto source=Read(characterEditor.Path);
    if(source!=characterEditor.Session.Original())throw std::runtime_error("Source save changed; reopen it before editing/exporting");
    return source;
}
json EditorMods(){
    const auto plan=HelpyCacheSources::Snapshot();json rows=json::array();
    const auto root=HostServices::ModDirectory()/"mods";
    for(const auto& [source,owners]:plan.DeclaredOwners)for(const auto& owner:owners)
        rows.push_back({{"Source",source},{"Mod",owner},{"Root",SavePathText(root/owner)},
            {"Registered",plan.Registered.contains(source)},{"Evidence","Accepted registry declaration; shared roots can have multiple providers"}});
    return rows;
}
json EditorItemRecord(const std::string& path,int count){
    auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path));auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
    if(!item||!type||!item->IsA(type)||RC::to_string(item->GetPathName())!=path)throw std::runtime_error("The exact native ItemData must be loaded before adding it to a save");
    const auto identity=StringField(item,TEXT("PersistenceID"));if(identity.size()!=22)throw std::runtime_error("Native ItemData persistence identity is unavailable");
    json matches=json::array({{{"PersistenceID",identity}}});Resolve(matches,TEXT("/Script/Dominion.ItemData"),true);
    if(!matches[0].contains("AssetPath")||!matches[0]["AssetPath"].is_string()||matches[0]["AssetPath"]!=path)throw std::runtime_error("Native persistence identity is ambiguous; no addition staged");
    ActorHelper::FunctionCall stack(item,TEXT("/Script/Dominion.ItemData:GetMaxStackSize"));stack.Invoke();const auto maximum=stack.Result<int32_t>();
    if(count<1||count>maximum||maximum<1||maximum>10000)throw std::runtime_error("Quantity exceeds the verified native stack limit");
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetGuidLibrary"));
    ActorHelper::FunctionCall guid(library,TEXT("/Script/Engine.KismetGuidLibrary:NewGuid"));guid.Invoke();unsigned char bytes[16]{};guid.MoveResult(bytes,sizeof(bytes));
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";std::string unique;uint32_t accumulator=0;int bits=0;for(auto byte:bytes){accumulator=(accumulator<<8)|byte;bits+=8;while(bits>=6){bits-=6;unique+=alphabet[(accumulator>>bits)&63];}}if(bits)unique+=alphabet[(accumulator<<(6-bits))&63];
    json record={{"ItemData",identity},{"GUID",unique}};if(count>1)record["Count"]=count;
    ActorHelper::FunctionCall durability(item,TEXT("/Script/Dominion.ItemData:GetBaseDurability"));durability.Invoke();const auto base=durability.Result<int32_t>();if(base< -1||base>10000000)throw std::runtime_error("Native item durability is outside the supported bound");if(base>0)record["Durability"]=base;
    ActorHelper::FunctionCall itemClass(item,TEXT("/Script/Dominion.ItemData:GetItemClass"));itemClass.Invoke();auto* native=itemClass.Result<UClass*>();
    if(!native)throw std::runtime_error("Native item instance class unavailable");
    if(auto* shield=PropertyHelper::GetPropertyByName(native,TEXT("CurrentVitalShield"));shield)record["VitalShield"]=0;
    if(!RC::to_string(native->GetPathName()).starts_with("/Script/Dominion.")){
        json sample;for(const auto* container:{"Inventory","PersonalInventory"})if(characterEditor.Session.Draft()["GameProgress"].contains(container))for(const auto& value:characterEditor.Session.Draft()["GameProgress"][container])if(value.is_object()&&value.value("ItemData",std::string{})==identity){sample=value;break;}
        if(sample.is_null())throw std::runtime_error("This mod defines a custom item-instance class. A verified native save template is required; no incomplete record was invented.");
        sample["GUID"]=unique;if(count>1||sample.contains("Count"))sample["Count"]=count;record=std::move(sample);
    }
    return record;
}
}
void ResetCharacterEditor(){CharacterPreview::Reset();characterEditor.Session.Reset();characterEditor.Choices=json::object();characterEditor.Layout=json::array();}
json SaveEditorOperation(const std::string& op,const json& args){
    if(op=="SaveEditorList"){
        json files=json::array(),warnings=json::array();
        for(const auto& file:ImportedSaves())files.push_back({{"Name",file.filename().string()},{"Path",SavePathText(file)},{"Storage","Imported JSON payload"}});
        // Steam/Epic JSON is also editable while running Game Pass. This is a
        // source-format decision, not inferred from the active storefront.
        wchar_t local[32768]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,_countof(local));
        if(count&&count<_countof(local)){
            const auto folder=fs::path(local)/"RSDragonwilds/Saved/SaveCharacters";std::error_code ec;
            if(fs::is_directory(folder,ec)){size_t entries=0;for(const auto& entry:fs::directory_iterator(folder)){
                if(++entries>512){warnings.push_back("Local save list limited to 512 entries");break;}
                if(entry.is_regular_file()&&entry.path().extension()==L".json")files.push_back({{"Name",entry.path().filename().string()},{"Path",SavePathText(entry.path())},{"Storage","Steam/Epic JSON"}});
            }}
        }
        return {{"Files",files},{"Mods",EditorMods()},{"Warnings",warnings},{"Runtime",Storefront::Name(Storefront::Current())},
            {"AttachAvailable",false},{"Status","Edit imported character JSON from either runtime. Native Steam/Epic and Game Pass attachment adapters are not yet verified; export copies only."}};
    }
    if(op=="SaveEditorOpen"){
        ResetCharacterEditor();
        if(!args.value("Offline",false))throw std::runtime_error("Confirm this character is not loaded before opening its save");
        const auto path=SaveInput(args);const auto stamp=fs::last_write_time(path);const auto size=fs::file_size(path);auto source=Read(path);
        if(fs::last_write_time(path)!=stamp||fs::file_size(path)!=size)throw std::runtime_error("Save changed during read; retry");
        characterEditor.Session.Open(source);characterEditor.Path=path;characterEditor.Time=stamp;characterEditor.Size=size;
        auto result=characterEditor.Session.Browse();result["Path"]=SavePathText(path);result["Format"]="Native character JSON";result["Status"]="Character copy opened. Unknown fields and mod IDs are preserved.";return result;
    }
    if(op=="SaveEditorClose"){ResetCharacterEditor();return {{"Status","Draft discarded. Original unchanged."}};}
    if(op=="SaveEditorRenderStop"){CharacterPreview::Reset();return {{"Status","Native preview stopped; transient scene released."}};}
    const auto source=EditorSource();
    if(op=="SaveEditorAppearance"||op=="SaveEditorAppearanceSet"||op=="SaveEditorRename"){
        if(characterEditor.Choices.empty())characterEditor.Choices=CharacterPreview::Choices();
        if(op=="SaveEditorAppearanceSet"){
            const auto field=args.at("Field").get<std::string>(),selected=args.at("Row").get<std::string>();std::map<std::string,std::set<std::string>> verified;
            for(const auto& choices:characterEditor.Choices.at("Fields"))for(const auto& row:choices.at("Rows"))verified[choices.at("Key").get<std::string>()].insert(row.get<std::string>());
            std::map<std::string,std::string> changes{{field,selected}};
            if(field=="BodyType"){
                const bool female=selected.starts_with("female"),male=selected.starts_with("male");if(!female&&!male)throw std::runtime_error("This body type has no verified compatible face family");
                const auto& appearance=CharacterWorkbench::Appearance(characterEditor.Session.Draft());const auto head=CharacterWorkbench::Row(appearance,CharacterWorkbench::Find("Head"));
                if(!head.starts_with(female?"female":"male")){if(!verified["Head"].contains(selected))throw std::runtime_error("No matching native face for this body type");changes["Head"]=selected;}
                const auto beard=CharacterWorkbench::Row(appearance,CharacterWorkbench::Find("FacialHairPreset"));if(beard.starts_with(female?"M_":"F_")){const std::string none=female?"F_A_PresetNone":"M_A_PresetNone";if(!verified["FacialHairPreset"].contains(none))throw std::runtime_error("No compatible native beard preset for this body type");changes["FacialHairPreset"]=none;}
            }
            characterEditor.Session.SetAppearanceBatch(changes,args.at("Revision").get<uint64_t>(),verified);
        }
        if(op=="SaveEditorRename")characterEditor.Session.Rename(args.at("Name").get<std::string>(),args.at("Revision").get<uint64_t>());
        auto result=characterEditor.Session.Appearance();result["Choices"]=characterEditor.Choices;result["Status"]="Appearance/name changes affect the draft only; review and export a copy.";return result;
    }
    if(op=="SaveEditorSlotItems"){
        if(!characterEditor.Session.Ready())throw std::runtime_error("Open a character copy first");const auto slot=args.at("Slot").get<std::string>();
        auto page=AssetSearch::LoadoutWindow(slot,args.value("Query",std::string{}),args.value("Offset",size_t{}));
        page["Slot"]=slot;page["Coverage"]="Native loaded ItemData only · refresh after loading additional content. Compatibility is not equip permission.";
        return page;
    }
    if(op=="SaveEditorRender")return CharacterPreview::Render(CharacterWorkbench::Appearance(characterEditor.Session.Draft()),args);
    if(op=="SaveEditorSkills"||op=="SaveEditorSkillSet"){
        auto result=CharacterPreview::Skills(characterEditor.Session.Draft());
        if(op=="SaveEditorSkillSet"){
            const auto id=args.at("Id").get<std::string>();const int level=args.at("Level").get<int>();const json* selected=nullptr;for(const auto& row:result.at("Items"))if(row.at("Id")==id){if(selected)throw std::runtime_error("Duplicate saved skill identity");selected=&row;}
            if(!selected||!selected->value("Resolved",false)||level<1||level>selected->at("MaxLevel").get<int>())throw std::runtime_error("Choose a resolved skill and a level within its native maximum");
            characterEditor.Session.SetSkillLevel(id,level,args.at("Revision").get<uint64_t>(),result.at("Thresholds"),selected->at("MaxLevel"));result=CharacterPreview::Skills(characterEditor.Session.Draft());
        }result["Revision"]=characterEditor.Session.Revision();return result;
    }
    if(op=="SaveEditorAddItem"){
        if(!args.value("Confirm",false))throw std::runtime_error("Confirm the addition before staging");
        if(args.value("Personal",false))throw std::runtime_error("Personal storage insertion requires a separately verified native capacity; existing storage remains editable and removable.");
        if(characterEditor.Layout.empty())characterEditor.Layout=CharacterPreview::InventoryLayout();
        const auto group=args.value("Group",std::string("Inventory"));int destination=-1;const auto& slots=characterEditor.Session.Draft().at("GameProgress").at("Inventory");
        if(group!="Inventory")throw std::runtime_error("New records currently require the general Inventory tab. Special-tab native item eligibility is not verified; existing records remain editable.");
        for(const auto& page:characterEditor.Layout)if(page.at("Group")==group){const int start=page.at("Start"),count=page.at("Count");for(int i=start;i<start+count;++i)if(!slots.contains(std::to_string(i))){destination=i;break;}if(destination>=0)break;}
        if(destination<0)throw std::runtime_error("No empty native slot in the selected inventory tab; no existing item was replaced");
        const auto record=EditorItemRecord(args.at("Path").get<std::string>(),args.at("Count").get<int>());
        characterEditor.Session.AddRecord(record,args.at("Revision").get<uint64_t>(),"Inventory",destination);
    }
    if(op=="SaveEditorStack"){
        const auto pointer=args.at("Pointer").get<std::string>();json rows=json::array({{{"PersistenceID",characterEditor.Session.ItemIdentity(pointer)}}});
        try{Resolve(rows,TEXT("/Script/Dominion.ItemData"),true);}catch(...){} // Unknown metadata can never authorize a stack increase.
        characterEditor.Session.SetCount(pointer,args.at("Count").get<int64_t>(),args.at("Revision").get<uint64_t>(),rows[0].value("MaxStackSize",int64_t{}));
    }
    if(op=="SaveEditorRemoveItems")characterEditor.Session.RemoveItems(args.at("Pointers"),args.at("Revision").get<uint64_t>(),args.value("Confirm",false));
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"||op=="SaveEditorAddItem"){
        auto result=characterEditor.Session.Inventory(args.value("Offset",size_t{}));
        try{Resolve(result["Items"],TEXT("/Script/Dominion.ItemData"),true);}catch(const std::exception& error){result["MetadataWarning"]=error.what();}
        try{if(characterEditor.Layout.empty())characterEditor.Layout=CharacterPreview::InventoryLayout();result["NativeLayout"]=characterEditor.Layout;
            for(auto& row:result["Items"]){const auto slot=std::stoi(row.at("Slot").get<std::string>());row["Group"]=row.at("Container")=="PersonalInventory"?"Personal storage":slot<8?"Quick slots":CharacterWorkbench::InventoryGroup(row);}
        }catch(const std::exception& error){result["LayoutWarning"]=error.what();}
        auto loadout=characterEditor.Session.Loadout();try{Resolve(loadout,TEXT("/Script/Dominion.ItemData"),true);}catch(...){}for(auto& row:loadout)try{row["Location"]=CharacterPreview::LoadoutSlot(row.at("LoadoutSlot"));}catch(...){row["Location"]="Unknown slot";}result["Loadout"]=loadout;
        result["Coverage"]="Saved inventory, not live items. Names/icons/source resolve only against currently loaded native ItemData; missing or ambiguous IDs remain explicit.";
        return result;
    }
    const auto pointer=args.value("Pointer",std::string{});
    if(op=="SaveEditorBrowse")return characterEditor.Session.Browse(pointer,args.value("Offset",size_t{}));
    if(op=="SaveEditorSet"){
        characterEditor.Session.Set(pointer,args.at("Value"),args.at("Revision").get<uint64_t>());
        const auto parent=pointer.substr(0,pointer.find_last_of('/'));auto result=characterEditor.Session.Browse(parent);result["Status"]="Edit staged in memory. Review and export to keep it.";return result;
    }
    if(op=="SaveEditorPreview")return characterEditor.Session.Preview(source);
    if(op=="SaveEditorExport"){
        const auto document=characterEditor.Session.Consume(source,args.at("Token").get<std::string>(),args.value("Confirm",false));
        return {{"File",SavePathText(WriteSaveCopy(HostServices::ExportsDirectory()/"saves","edited",document,characterEditor.Path))},
            {"Status","Verified character copy exported. Original unchanged; no runtime attachment or platform conversion was performed."}};
    }
    throw std::runtime_error("Unknown save-editor operation");
}
