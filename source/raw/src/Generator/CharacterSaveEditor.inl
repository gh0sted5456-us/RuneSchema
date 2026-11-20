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
namespace {
// J4SSaves sections. The save copy is edited through the session like any other change;
// the J4SSaves store is only touched when the player asks for it and the J4SSaves DLL the
// game loaded offers list/delete (1.1+). Backups live with the character's profile files.
fs::path SectionBackupFolder(const std::string& guid){return RuneSchemaData::CharacterCopiesDirectory()/guid/"sections";}
std::string UtcStamp(){
    SYSTEMTIME now{};GetSystemTime(&now);char text[32]{};
    std::snprintf(text,sizeof(text),"%04u-%02u-%02uT%02u:%02u:%02uZ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);return text;
}
std::vector<std::string> SectionOwners(const json& args){
    std::vector<std::string> owners;const auto& list=args.at("Owners");
    if(!list.is_array()||list.empty()||list.size()>J4SSections::MaxSections)throw std::runtime_error("Choose at least one save section");
    for(const auto& owner:list){if(!owner.is_string()||!J4SSections::ValidOwner(owner.get<std::string>()))throw std::runtime_error("Invalid save section owner");owners.push_back(owner);}
    return owners;
}
json SectionBackups(const std::string& guid){
    json rows=json::array();const auto folder=SectionBackupFolder(guid);std::error_code ec;
    if(!fs::is_directory(folder,ec))return rows;
    std::vector<fs::path> files;size_t seen=0;
    for(const auto& entry:fs::directory_iterator(folder,ec)){if(++seen>512)break;if(entry.is_regular_file()&&entry.path().extension()==L".json")files.push_back(entry.path());}
    std::sort(files.begin(),files.end(),std::greater<>());
    for(const auto& file:files){if(rows.size()>=64)break;
        try{std::error_code sizeError;if(fs::file_size(file,sizeError)>16*1024*1024)continue;const auto backup=Read(file);const auto sections=J4SSections::ReadBackup(backup,guid);
            json owners=json::array();for(const auto& [owner,entry]:sections.items())owners.push_back({{"Owner",owner},{"Schema",entry[0]},{"Revision",entry[1]},{"Bytes",entry[2].get_ref<const std::string&>().size()/2}});
            rows.push_back({{"File",file.filename().string()},{"CreatedAt",backup.value("createdAt",std::string{})},{"Sections",owners}});
        }catch(...){rows.push_back({{"File",file.filename().string()},{"Damaged",true}});}
    }
    return rows;
}
json EditorSections(const std::string& op,const json& args){
    if(!characterEditor.Session.Ready())throw std::runtime_error("Open a character copy first");
    const auto guid=characterEditor.Session.CharacterGuid();if(guid.empty())throw std::runtime_error("Character identity is unavailable");
    const auto& draft=characterEditor.Session.Draft();
    const auto api=J4SSavesBridge::Find(HostServices::WorkingDirectory()/"Mods");
    std::vector<J4SSavesBridge::Stored> stored;int storeStatus=J4SSavesBridge::Unavailable;
    if(api.Complete())try{storeStatus=J4SSavesBridge::List(api,guid,stored);}catch(...){storeStatus=J4SSavesBridge::StorageError;}
    const auto storedRevision=[&](const std::string& owner){for(const auto& row:stored)if(row.Owner==owner)return row.Revision;return int64_t{};};
    const bool useStore=args.value("Store",false);
    if(useStore&&storeStatus!=J4SSavesBridge::Ok)throw std::runtime_error(std::string("The J4SSaves store is ")+J4SSavesBridge::Name(storeStatus)+"; only the save copy can change");
    json done=json::array();std::string status;
    if(op=="SaveEditorSectionBackup"){
        const auto owners=SectionOwners(args);auto source=draft;
        // Sections kept only in the store (companion mode) are backed up from the store.
        for(const auto& owner:owners)if(!J4SSections::Entry(source,owner)&&storeStatus==J4SSavesBridge::Ok){json entry;if(J4SSavesBridge::Read(api,guid,owner,entry)==J4SSavesBridge::Ok)source=J4SSections::WithSection(std::move(source),owner,entry);}
        const auto name=draft.contains("meta_data")&&draft["meta_data"].is_object()?draft["meta_data"].value("char_name",std::string{}):std::string{};
        const auto stamp=UtcStamp();const auto backup=J4SSections::Backup(source,guid,name,owners,stamp);
        const auto folder=SectionBackupFolder(guid);RuneSchemaData::Safe(folder);fs::create_directories(folder);
        auto file=folder/J4SSections::BackupName(stamp,owners.size());for(int i=2;fs::exists(file)&&i<100;++i)file=folder/(file.stem().string()+"-"+std::to_string(i)+".json");
        const auto written=WriteSaveCopy(folder,file.stem().string(),backup);
        status="Backed up "+std::to_string(owners.size())+" section(s) to "+written.filename().string()+".";
    }
    if(op=="SaveEditorSectionRestore"){
        const auto fileName=args.at("File").get<std::string>();
        if(fileName.empty()||fileName.find_first_of("/\\:")!=std::string::npos||!fileName.ends_with(".json"))throw std::runtime_error("Choose a listed backup");
        const auto path=SectionBackupFolder(guid)/fs::path(std::u8string(fileName.begin(),fileName.end()));RuneSchemaData::Safe(path);
        const auto sections=J4SSections::ReadBackup(Read(path),guid);const auto owners=SectionOwners(args);
        json changes=json::object();
        for(const auto& owner:owners){
            if(!sections.contains(owner))throw std::runtime_error("The backup does not contain "+owner);
            auto entry=sections.at(owner);
            // The restored copy must outrank both the copy in this save and the store's copy,
            // or J4SSaves would keep the newer one on the next load.
            int64_t floor=entry[1];if(const auto current=J4SSections::Entry(draft,owner))floor=(std::max)(floor,(*current)[1].get<int64_t>());floor=(std::max)(floor,storedRevision(owner));
            entry[1]=floor+1;
            if(useStore){int64_t revision=0;const auto result=J4SSavesBridge::Write(api,guid,owner,storedRevision(owner),entry,revision);
                if(result!=J4SSavesBridge::Ok)throw std::runtime_error("J4SSaves did not accept "+owner+": "+J4SSavesBridge::Name(result));entry[1]=revision;}
            changes[owner]=entry;done.push_back(owner);
        }
        characterEditor.Session.SetSections(changes,args.at("Revision").get<uint64_t>());
        status="Restored "+std::to_string(owners.size())+" section(s) into the save copy"+(useStore?" and the J4SSaves store.":". Export the copy to keep it.");
    }
    if(op=="SaveEditorSectionClean"){
        const auto owners=SectionOwners(args);json changes=json::object();
        for(const auto& owner:owners){
            int64_t newest=storedRevision(owner);if(const auto current=J4SSections::Entry(draft,owner))newest=(std::max)(newest,(*current)[1].get<int64_t>());
            if(useStore&&newest>0){const auto result=api.Delete(guid.c_str(),owner.c_str(),newest);
                if(result!=J4SSavesBridge::Ok&&result!=J4SSavesBridge::Missing)throw std::runtime_error("J4SSaves did not remove "+owner+": "+J4SSavesBridge::Name(result));}
            changes[owner]=nullptr;done.push_back(owner);
        }
        characterEditor.Session.SetSections(changes,args.at("Revision").get<uint64_t>());
        status="Removed "+std::to_string(owners.size())+" section(s) from the save copy"+(useStore?" and the J4SSaves store.":". The J4SSaves store still has its copy until you clean it there too.");
    }
    const auto& current=characterEditor.Session.Draft();json rows=json::array();
    for(const auto& row:J4SSections::List(current))rows.push_back({{"Owner",row.Owner},{"Schema",row.Schema},{"Revision",row.Revision},{"Bytes",row.Bytes},{"Valid",row.Valid},{"InSave",true},{"StoredRevision",storedRevision(row.Owner)}});
    for(const auto& row:stored)if(!J4SSections::Entry(current,row.Owner)&&std::none_of(rows.begin(),rows.end(),[&](const json& r){return r.value("Owner",std::string{})==row.Owner;}))
        rows.push_back({{"Owner",row.Owner},{"Schema",row.Schema},{"Revision",row.Revision},{"Bytes",row.Size},{"Valid",true},{"InSave",false},{"StoredRevision",row.Revision}});
    std::sort(rows.begin(),rows.end(),[](const json& a,const json& b){return a.value("Owner",std::string{})<b.value("Owner",std::string{});});
    const auto envelope=current.contains(J4SSections::Envelope)&&current[std::string(J4SSections::Envelope)].is_object()?current[std::string(J4SSections::Envelope)]:json::object();
    return {{"Sections",rows},{"Backups",SectionBackups(guid)},{"Store",J4SSavesBridge::Name(storeStatus)},{"StoreAvailable",storeStatus==J4SSavesBridge::Ok},
        {"Mods",envelope.value("mods",json::object())},{"CharacterGuid",guid},{"Revision",characterEditor.Session.Revision()},{"Changed",done},{"Status",status}};
}
// Steam/Epic and Game Pass keep the same character JSON; Game Pass stores it as a provider
// blob. Steam gets a plain file in SaveCharacters. Game Pass is never written here: the copy
// is queued for the J4SSaves importer, which creates the slot through the Xbox provider.
std::wstring LocalAppData(){
    wchar_t local[32768]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,_countof(local));
    if(!count||count>=_countof(local))throw std::runtime_error("LOCALAPPDATA is unavailable");return local;
}
std::string TransferFileName(const json& save){
    std::string name=save.contains("meta_data")&&save["meta_data"].is_object()?save["meta_data"].value("char_name",std::string{}):std::string{};
    std::string safe;for(const unsigned char c:name)if(c>=0x20&&c!='/'&&c!='\\'&&c!=':'&&c!='*'&&c!='?'&&c!='"'&&c!='<'&&c!='>'&&c!='|')safe+=static_cast<char>(c);
    while(!safe.empty()&&(safe.back()==' '||safe.back()=='.'))safe.pop_back();
    if(safe.empty())safe="Character";if(safe.size()>64)safe.resize(64);return safe;
}
json EditorTransfer(const json& args){
    if(!characterEditor.Session.Ready())throw std::runtime_error("Open a character copy first");
    const auto target=args.at("Target").get<std::string>();const auto guid=characterEditor.Session.CharacterGuid();
    // Transfers use the opened snapshot; staged edits go through review and export first.
    if(characterEditor.Session.Draft()!=characterEditor.Session.Original())throw std::runtime_error("Export or discard the staged edits before transferring this character");
    const auto source=EditorSource();
    if(!source.contains("Version")||!source["Version"].is_number_integer())throw std::runtime_error("This save has no native version; it cannot be transferred");
    const auto local=fs::path(LocalAppData());
    if(target=="Steam"){
        const auto folder=local/"RSDragonwilds/Saved/SaveCharacters";
        if(!fs::is_directory(folder))throw std::runtime_error("No Steam/Epic character folder was found on this PC");
        const auto live=CharacterSaveEditor::Guid(args.value("LiveCharacterGuid",std::string{}));
        fs::path existing;size_t seen=0;
        for(const auto& entry:fs::directory_iterator(folder)){if(++seen>1024)break;if(!entry.is_regular_file()||entry.path().extension()!=L".json")continue;
            std::error_code ec;if(fs::file_size(entry.path(),ec)>8*1024*1024)continue;
            try{if(CharacterSaveEditor::Identity(Read(entry.path()))==guid){existing=entry.path();break;}}catch(...){}}
        if(!existing.empty()){
            if(!args.value("Replace",false))return {{"NeedsConfirm",true},{"Existing",existing.filename().string()},{"Status","Steam already has this character as "+existing.filename().string()+". Confirm to replace it; the current file is backed up first."}};
            if(Storefront::Current()==Storefront::Kind::SteamEpic&&live==guid)throw std::runtime_error("This character is loaded in the running Steam game. Return to the main menu and close the game before replacing it");
            WriteSaveCopy(RuneSchemaData::CharacterCopiesDirectory()/guid/"backups","steam-before-transfer",Read(existing),existing);
        }
        auto output=existing;
        if(output.empty()){const auto base=TransferFileName(source);output=folder/(base+".json");for(int i=2;fs::exists(output)&&i<100;++i)output=folder/(base+" "+std::to_string(i)+".json");if(fs::exists(output))throw std::runtime_error("No free Steam save name");}
        const auto staged=WriteSaveCopy(RuneSchemaData::CharacterCopiesDirectory()/guid/"exports","steam-transfer",source);
        if(!CopyFileW(staged.c_str(),output.c_str(),existing.empty()))throw std::runtime_error("Could not write the Steam save; nothing was replaced");
        if(Read(output)!=source)throw std::runtime_error("The Steam save did not read back intact; restore it from the profile backups");
        return {{"File",SavePathText(output)},{"Status","Saved for Steam/Epic as "+output.filename().string()+". Steam Cloud uploads it the next time Steam syncs."}};
    }
    if(target=="Xbox"){
        const auto folder=local/"J4SSaves";
        if(!fs::is_directory(folder))throw std::runtime_error("J4SSaves is not set up on this PC. Install J4SSaves on Game Pass and start the game once");
        const auto request=folder/"import-character.json";
        if(fs::exists(request))throw std::runtime_error("A Game Pass import is already queued. Open character selection on Game Pass, or delete import-character.json with the game closed to cancel it");
        const auto identity=[](const fs::path& file){try{return CharacterSaveEditor::Identity(Read(file));}catch(...){return std::string{};}};
        for(const auto& file:XboxCharacterSaves::Candidates(HostServices::XboxSaveRoot()))
            if(identity(file)==guid)throw std::runtime_error("Game Pass already has this character. J4SSaves imports into a new slot only; delete that slot first");
        const auto staged=WriteSaveCopy(RuneSchemaData::CharacterCopiesDirectory()/guid/"exports","xbox-transfer",source);
        if(!CopyFileW(staged.c_str(),request.c_str(),TRUE))throw std::runtime_error("Could not queue the Game Pass import");
        return {{"File",SavePathText(request)},{"Status","Queued for Game Pass. Start the Game Pass game with J4SSaves, open character selection and wait for the import to confirm. Use the same gameplay mods on both."}};
    }
    throw std::runtime_error("Choose Steam or Xbox");
}
}
void ResetCharacterEditor(){characterEditor.Session.Reset();characterEditor.Choices=json::object();characterEditor.Layout=json::array();}
json SaveEditorOperation(const std::string& op,const json& args){
    if(op=="SaveEditorList"){
        json files=json::array(),warnings=json::array();
        size_t metadataBudget=16*1024*1024;
        const auto append=[&](const fs::path& file,const char* storage,const char* provider){
            std::string name=file.stem().string();std::error_code ec;const auto size=fs::file_size(file,ec);
            std::string guid;const bool xbox=std::string_view(provider)=="GamePass";bool character=false;
            if(!ec&&size<=1024*1024&&size<=metadataBudget){metadataBudget-=size;try{const auto save=Read(file);guid=CharacterSaveEditor::Identity(save);character=XboxCharacterSaves::Character(save);if(save.contains("meta_data")&&save["meta_data"].is_object()){const auto label=save["meta_data"].value("char_name",std::string{});if(!label.empty()&&label.size()<=128)name=label;}}catch(...){}}
            if(xbox&&!character)return;
            if(xbox&&std::any_of(files.begin(),files.end(),[&](const auto& row){return row.value("Provider",std::string{})=="GamePass"&&row.value("CharacterGuid",std::string{})==guid;}))return;
            files.push_back({{"Name",name},{"FileName",file.filename().string()},{"Path",SavePathText(file)},{"Storage",storage},{"Provider",provider},{"CharacterGuid",guid}});
        };
        std::vector<std::pair<fs::file_time_type,fs::path>> xboxFiles;
        for(const auto& file:XboxCharacterSaves::Candidates(HostServices::XboxSaveRoot())){
            std::error_code ec;const auto time=fs::last_write_time(file,ec);if(!ec)xboxFiles.emplace_back(time,file);
        }
        std::sort(xboxFiles.begin(),xboxFiles.end(),std::greater<>());
        for(const auto& [time,file]:xboxFiles){
            if(files.size()>=512){warnings.push_back("Save list limited to 512 entries");break;}
            append(file,"Xbox / Game Pass · read-only source","GamePass");
        }
        for(const auto& file:ImportedSaves()){if(files.size()>=512){warnings.push_back("Save list limited to 512 entries");break;}append(file,"Imported JSON payload","Imported");}
        // Steam/Epic JSON is also editable while running Game Pass. This is a
        // source-format decision, not inferred from the active storefront.
        wchar_t local[32768]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,_countof(local));
        if(count&&count<_countof(local)){
            const auto folder=fs::path(local)/"RSDragonwilds/Saved/SaveCharacters";std::error_code ec;
            if(fs::is_directory(folder,ec)){size_t entries=0;for(const auto& entry:fs::directory_iterator(folder)){
                if(++entries>512){warnings.push_back("Local save list limited to 512 entries");break;}
                if(files.size()>=512)break;
                if(entry.is_regular_file()&&entry.path().extension()==L".json")append(entry.path(),"Steam/Epic JSON","SteamEpic");
            }}
        }
        try{const auto profileCharacters=RuneSchemaData::CharacterCopiesDirectory();std::error_code profileError;
        if(fs::is_directory(profileCharacters,profileError)){
            size_t folders=0,profileFiles=0;for(const auto& character:fs::directory_iterator(profileCharacters)){
                if(++folders>256||files.size()>=512||profileFiles>=1024)break;
                RuneSchemaData::Safe(character.path());if(!character.is_directory()||CharacterSaveEditor::Guid(character.path().filename().string()).empty())continue;
                for(const auto* kind:{"snapshots","exports"}){
                    const auto folder=character.path()/kind;RuneSchemaData::Safe(folder);if(!fs::is_directory(folder,profileError))continue;
                    std::vector<std::pair<fs::file_time_type,fs::path>> copies;size_t inspected=0;
                    for(const auto& copy:fs::directory_iterator(folder)){if(++inspected>256||++profileFiles>1024)break;RuneSchemaData::Safe(copy.path());if(copy.is_regular_file()&&copy.path().extension()==L".json")copies.emplace_back(copy.last_write_time(),copy.path());}
                    std::sort(copies.begin(),copies.end(),std::greater<>());size_t shown=0;
                    for(const auto& [time,path]:copies){if(++shown>3||files.size()>=512)break;append(path,"RuneSchema profile copy","Profile");}
                }
            }
        }
        }catch(const std::exception& error){warnings.push_back(std::string("Profile copies unavailable: ")+error.what());}
        std::stable_sort(files.begin(),files.end(),[](const auto& a,const auto& b){return std::pair{a.at("Name").template get<std::string>(),a.at("Path").template get<std::string>()}<std::pair{b.at("Name").template get<std::string>(),b.at("Path").template get<std::string>()};});
        return {{"Files",files},{"Mods",EditorMods()},{"Warnings",warnings},{"Runtime",Storefront::Name(Storefront::Current())},
            {"AttachAvailable",false},{"Status","Steam/Epic and Xbox character saves · copy-only editing. Xbox provider containers are never modified."}};
    }
    if(op=="SaveEditorOpen"){
        ResetCharacterEditor();
        const bool copyOnly=args.value("CopyOnly",false);
        if(!args.value("Offline",false)&&!copyOnly)throw std::runtime_error("Open an isolated copy or confirm this character is offline");
        const auto text=args.at("Path").get<std::string>();auto path=fs::canonical(fs::path(std::u8string(text.begin(),text.end())));
        const bool xbox=path.extension()!=L".json";
        if(xbox&&!XboxCharacterSaves::Candidate(HostServices::XboxSaveRoot(),path))throw std::runtime_error("Choose a discovered Xbox character payload, not a provider index or world container");
        const auto stamp=fs::last_write_time(path);const auto size=fs::file_size(path);auto source=Read(path);
        if(fs::last_write_time(path)!=stamp||fs::file_size(path)!=size)throw std::runtime_error("Save changed during read; retry");
        if(xbox&&!XboxCharacterSaves::Character(source))throw std::runtime_error("This Xbox blob is not a supported character JSON payload");
        CharacterSaveEditor::RequireCharacter(source);
        if(args.contains("CharacterGuid")){
            const auto expected=CharacterSaveEditor::Guid(args.at("CharacterGuid"));
            if(expected.empty()||CharacterSaveEditor::Identity(source)!=expected)throw std::runtime_error("Selected save no longer matches the current character; refresh saves");
        }
        const auto originalPath=SavePathText(path);
        if((xbox||copyOnly)&&CharacterSaveEditor::Identity(source).empty())throw std::runtime_error("Character identity is unavailable; no profile copy was created");
        if(xbox||copyOnly)path=WriteSaveCopy(RuneSchemaData::CharacterCopiesDirectory()/CharacterSaveEditor::Identity(source)/"snapshots",xbox?"xbox-character":"character",source,path);
        characterEditor.Session.Open(source);characterEditor.Path=path;characterEditor.Time=fs::last_write_time(path);characterEditor.Size=fs::file_size(path);
        auto result=characterEditor.Session.Browse();result["Path"]=SavePathText(path);result["OriginalPath"]=originalPath;result["CopyOnly"]=xbox||copyOnly;result["Format"]="Native character JSON";result["Status"]="Saved snapshot opened. Edits export a separate copy, not the live character.";return result;
    }
    if(op=="SaveEditorClose"){ResetCharacterEditor();return {{"Status","Draft discarded. Original unchanged."}};}
    if(op=="SaveEditorSlotItems"){
        if(args.value("CacheFirst",false)){
            const auto slot=args.at("Slot").get<std::string>();auto page=AssetSearch::AlphabeticalItemWindow("__helpyslot:"+slot+" "+args.value("Query",std::string{}),args.value("Cursor",std::string{}),48,"BasicRow");
            page["Slot"]=slot;page["NextOffset"]=args.value("Offset",size_t{})+1;return page;
        }
        const auto slot=args.at("Slot").get<std::string>();auto page=AssetSearch::LoadoutWindow(slot,args.value("Query",std::string{}),args.value("Offset",size_t{}));page["Slot"]=slot;page["Coverage"]="Mounted and loaded native ItemData; slot metadata verified before inclusion.";return page;
    }
    // Item inspection is independent of an offline character draft.
    const auto source=EditorSource();
    if(op=="SaveEditorAppearance"||op=="SaveEditorAppearanceSet"||op=="SaveEditorRename"){
        if(characterEditor.Choices.empty())characterEditor.Choices=CharacterNativeData::Choices();
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
    if(op=="SaveEditorSkills"||op=="SaveEditorSkillSet"){
        auto result=CharacterNativeData::Skills(characterEditor.Session.Draft());
        if(op=="SaveEditorSkillSet"){
            const auto id=args.at("Id").get<std::string>();const int level=args.at("Level").get<int>();const json* selected=nullptr;for(const auto& row:result.at("Items"))if(row.at("Id")==id){if(selected)throw std::runtime_error("Duplicate saved skill identity");selected=&row;}
            if(!selected||!selected->value("Resolved",false)||level<1||level>selected->at("MaxLevel").get<int>())throw std::runtime_error("Choose a resolved skill and a level within its native maximum");
            characterEditor.Session.SetSkillLevel(id,level,args.at("Revision").get<uint64_t>(),result.at("Thresholds"),selected->at("MaxLevel"));result=CharacterNativeData::Skills(characterEditor.Session.Draft());
        }result["Revision"]=characterEditor.Session.Revision();return result;
    }
    if(op=="SaveEditorAddItem"){
        if(!args.value("Confirm",false))throw std::runtime_error("Confirm the addition before staging");
        HelpyDeliveryGate::Require(characterEditor.Session.CharacterGuid(),""); // Adding items to a paused character is a delivery too.
        if(args.value("Personal",false))throw std::runtime_error("Personal storage insertion requires a separately verified native capacity; existing storage remains editable and removable.");
        if(characterEditor.Layout.empty())characterEditor.Layout=CharacterNativeData::InventoryLayout();
        const auto group=args.value("Group",std::string("Inventory"));int destination=-1;const auto& slots=characterEditor.Session.Draft().at("GameProgress").at("Inventory");
        if(group!="Inventory")throw std::runtime_error("New records currently require the general Inventory tab. Special-tab native item eligibility is not verified; existing records remain editable.");
        for(const auto& page:characterEditor.Layout)if(page.at("Group")==group){const int start=page.at("Start"),count=page.at("Count");for(int i=start;i<start+count;++i)if(!slots.contains(std::to_string(i))){destination=i;break;}if(destination>=0)break;}
        if(destination<0)throw std::runtime_error("No empty native slot in the selected inventory tab; no existing item was replaced");
        const auto record=EditorItemRecord(args.at("Path").get<std::string>(),args.at("Count").get<int>());
        characterEditor.Session.AddRecord(record,args.at("Revision").get<uint64_t>(),"Inventory",destination);
    }
    if(op=="SaveEditorStack"){
        const auto pointer=args.at("Pointer").get<std::string>();json rows=json::array({{{"PersistenceID",characterEditor.Session.ItemIdentity(pointer)}}});
        if(const auto& item=characterEditor.Session.Draft().at(json::json_pointer(pointer));item.is_object()&&args.at("Count").get<int64_t>()>item.value("Count",int64_t{}))HelpyDeliveryGate::Require(characterEditor.Session.CharacterGuid(),"");
        try{Resolve(rows,TEXT("/Script/Dominion.ItemData"),true);}catch(...){} // Unknown metadata can never authorize a stack increase.
        characterEditor.Session.SetCount(pointer,args.at("Count").get<int64_t>(),args.at("Revision").get<uint64_t>(),rows[0].value("MaxStackSize",int64_t{}));
    }
    if(op=="SaveEditorRemoveItems")characterEditor.Session.RemoveItems(args.at("Pointers"),args.at("Revision").get<uint64_t>(),args.value("Confirm",false));
    if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"||op=="SaveEditorAddItem"){
        auto result=characterEditor.Session.Inventory(args.value("Offset",size_t{}));
        try{Resolve(result["Items"],TEXT("/Script/Dominion.ItemData"),true);}catch(const std::exception& error){result["MetadataWarning"]=error.what();}
        try{if(characterEditor.Layout.empty())characterEditor.Layout=CharacterNativeData::InventoryLayout();result["NativeLayout"]=characterEditor.Layout;
            for(auto& row:result["Items"]){const auto slot=std::stoi(row.at("Slot").get<std::string>());row["Group"]=row.at("Container")=="PersonalInventory"?"Personal storage":slot<8?"Quick slots":CharacterWorkbench::InventoryGroup(row);}
        }catch(const std::exception& error){result["LayoutWarning"]=error.what();}
        auto loadout=characterEditor.Session.Loadout();try{Resolve(loadout,TEXT("/Script/Dominion.ItemData"),true);}catch(...){}for(auto& row:loadout)try{row["Location"]=CharacterNativeData::LoadoutSlot(row.at("LoadoutSlot"));}catch(...){row["Location"]="Unknown slot";}result["Loadout"]=loadout;
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
    if(op=="SaveEditorSections"||op=="SaveEditorSectionBackup"||op=="SaveEditorSectionRestore"||op=="SaveEditorSectionClean")return EditorSections(op,args);
    if(op=="SaveEditorTransfer")return EditorTransfer(args);
    if(op=="SaveEditorExport"){
        const auto document=characterEditor.Session.Consume(source,args.at("Token").get<std::string>(),args.value("Confirm",false));
        return {{"File",SavePathText(WriteSaveCopy(RuneSchemaData::CharacterCopiesDirectory()/characterEditor.Session.CharacterGuid()/"exports","edited",document,characterEditor.Path))},
            {"Status","Verified character copy exported. Original unchanged; no runtime attachment or platform conversion was performed."}};
    }
    throw std::runtime_error("Unknown save-editor operation");
}
