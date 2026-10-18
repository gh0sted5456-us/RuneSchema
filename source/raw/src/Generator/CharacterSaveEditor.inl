// Included after SaveAuthoring.inl: uses the bounded encoding-aware reader and
// unique verified-copy writer. Never opens the WGS database for writing.
namespace {
struct CharacterEditorState {
    fs::path Path;fs::file_time_type Time{};uintmax_t Size=0;
    CharacterSaveEditor::Session Session;
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
}
void ResetCharacterEditor(){characterEditor.Session.Reset();}
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
        characterEditor.Session.Reset();
        if(!args.value("Offline",false))throw std::runtime_error("Confirm this character is not loaded before opening its save");
        const auto path=SaveInput(args);const auto stamp=fs::last_write_time(path);const auto size=fs::file_size(path);auto source=Read(path);
        if(fs::last_write_time(path)!=stamp||fs::file_size(path)!=size)throw std::runtime_error("Save changed during read; retry");
        characterEditor.Session.Open(source);characterEditor.Path=path;characterEditor.Time=stamp;characterEditor.Size=size;
        auto result=characterEditor.Session.Browse();result["Path"]=SavePathText(path);result["Format"]="Native character JSON";result["Status"]="Character copy opened. Unknown fields and mod IDs are preserved.";return result;
    }
    if(op=="SaveEditorClose"){characterEditor.Session.Reset();return {{"Status","Draft discarded. Original unchanged."}};}
    const auto source=EditorSource();
    if(op=="SaveEditorStack"){
        const auto pointer=args.at("Pointer").get<std::string>();json rows=json::array({{{"PersistenceID",characterEditor.Session.ItemIdentity(pointer)}}});
        try{Resolve(rows,TEXT("/Script/Dominion.ItemData"),true);}catch(...){} // Unknown metadata can never authorize a stack increase.
        characterEditor.Session.SetCount(pointer,args.at("Count").get<int64_t>(),args.at("Revision").get<uint64_t>(),rows[0].value("MaxStackSize",int64_t{}));
    }
    if(op=="SaveEditorRemoveItems")characterEditor.Session.RemoveItems(args.at("Pointers"),args.at("Revision").get<uint64_t>(),args.value("Confirm",false));
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"){
        auto result=characterEditor.Session.Inventory(args.value("Offset",size_t{}));
        try{Resolve(result["Items"],TEXT("/Script/Dominion.ItemData"),true);}catch(const std::exception& error){result["MetadataWarning"]=error.what();}
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
