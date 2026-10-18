// Included in SaveViewer.cpp; all entry points run on the authoring game thread.
namespace {
struct DesktopSaveState {
    fs::path Path;fs::file_time_type Time{};uintmax_t Size=0;
    json Source,Npc;
    SaveCleanup::AuthoringPreview Preview;
    std::shared_ptr<const SaveCleanup::RegistrySnapshot> Registry;

} desktopSave;
std::string SavePathText(const fs::path& path){const auto text=path.u8string();return {text.begin(),text.end()};}
fs::path SaveInput(const json& args) {
    const auto text=args.at("Path").get<std::string>();
    const auto path=fs::canonical(fs::path(std::u8string(text.begin(),text.end())));
    if(path.extension()!=L".json")throw std::runtime_error("Only native character JSON saves are supported; no world/SPUD or WGS cleanup.");
    return path;
}
fs::path WriteSaveCopy(const fs::path& folder,const std::string& prefix,const json& value,const fs::path& original={}) {
    fs::create_directories(folder);auto text=value.dump(2);
    if(!original.empty()){
        std::ifstream encoding(original,std::ios::binary);char bom[2]{};encoding.read(bom,2);
        if(static_cast<unsigned char>(bom[0])==0xff&&static_cast<unsigned char>(bom[1])==0xfe){
            const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
            if(count<=0)throw std::runtime_error("Export encoding conversion failed");std::wstring wide(count,L'\0');
            if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),wide.data(),count)!=count)throw std::runtime_error("Export encoding conversion failed");
            text.assign("\xff\xfe",2);text.append(reinterpret_cast<const char*>(wide.data()),wide.size()*sizeof(wchar_t));
        }
    }
    const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto output=folder/(prefix+"_"+std::to_string(stamp)+".json");
    const auto handle=CreateFileW(output.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create a unique export; original untouched");
    DWORD written=0;const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();
    const bool flushed=ok&&FlushFileBuffers(handle);CloseHandle(handle);
    if(!flushed||Read(output)!=value)throw std::runtime_error("Export verification failed; do not use the incomplete copy. Original untouched.");
    return output;
}
}
void ResetCharacterEditor();
void ResetAuthoring(){desktopSave.Source=nullptr;desktopSave.Npc=nullptr;desktopSave.Registry.reset();desktopSave.Preview.Reset();ResetCharacterEditor();}
json Authoring(const std::string& operation,const json& args) {
    if(operation=="Imports") {json files=json::array();for(const auto& file:ImportedSaves())files.push_back({{"Name",file.filename().string()},{"Path",SavePathText(file)}});return {{"Files",files},{"Folder",SavePathText(ImportFolder())}};}
    if(operation=="NpcPreview") {desktopSave.Npc=nullptr;desktopSave.Npc=ResolveNpcPreview(NpcSaveExport::Preview(Read(SaveInput(args))));return desktopSave.Npc;}
    if(operation=="NpcExport") {
        if(!desktopSave.Npc.is_object()||!desktopSave.Npc.contains("NPC"))throw std::runtime_error("Read and review an NPC preview first");
        const auto id=args.at("Id").get<std::string>();
        if(id.empty()||id.size()>128||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=id.npos)throw std::runtime_error("NPC Id must use letters, numbers, underscores or hyphens");
        const auto location=args.at("Location");if(!location.is_array()||location.size()!=3)throw std::runtime_error("NPC Location requires X, Y and Z");
        for(const auto& component:location)if(!component.is_number()||!std::isfinite(component.get<double>()))throw std::runtime_error("NPC coordinates must be finite");
        const auto yaw=args.value("Yaw",0.0);if(!std::isfinite(yaw))throw std::runtime_error("NPC yaw must be finite");
        auto npc=desktopSave.Npc.at("NPC");npc["Id"]=id;npc["Enabled"]=false;npc["Location"]=location;npc["Rotation"]={{"Pitch",0},{"Yaw",yaw},{"Roll",0}};
        DragonWilds::HumanNpc::Validate(npc);
        return {{"File",SavePathText(WriteSaveCopy(HostServices::ExportsDirectory()/"npcs",id,npc))},{"Document",npc},{"Status","Disabled NPC definition exported. Original save untouched."}};
    }
    if(operation=="CleanupInspect") {
        ResetAuthoring();if(!args.value("Offline",false))throw std::runtime_error("Confirm this character save is offline before inspection");
        auto path=SaveInput(args);const auto stamp=fs::last_write_time(path);const auto size=fs::file_size(path);auto source=Read(path);
        const auto plan=SaveCleanup::Plan(source,{});const auto mods=HostServices::ModDirectory()/"mods";
        if(!fs::is_directory(mods))throw std::runtime_error("Mod directory unavailable; absent-mod classification refused");
        std::set<std::string> installed;for(const auto& entry:fs::directory_iterator(mods))if(entry.is_directory())installed.insert(entry.path().filename().string());
        if(fs::last_write_time(path)!=stamp||fs::file_size(path)!=size)throw std::runtime_error("Save changed during inspection");
        desktopSave.Path=path;desktopSave.Time=stamp;desktopSave.Size=size;desktopSave.Source=std::move(source);desktopSave.Preview.Inspect(desktopSave.Source);desktopSave.Registry=SaveCleanup::ReadRegistry();
        json owners=json::array();for(const auto& [owner,count]:plan.Owners)owners.push_back({{"Owner",owner},{"Count",count},{"Installed",installed.contains(owner)}});
        return {{"Owners",owners},{"RegistryReady",desktopSave.Registry&&desktopSave.Registry->Ready()&&desktopSave.Registry->QuestsComplete&&desktopSave.Registry->JournalsComplete},{"Path",SavePathText(path)},{"Status","Choose owners, preview removals, then confirm export. Original save stays untouched."}};
    }
    if(desktopSave.Source.is_null())throw std::runtime_error("Inspect an offline character save first");
    if(fs::last_write_time(desktopSave.Path)!=desktopSave.Time||fs::file_size(desktopSave.Path)!=desktopSave.Size||Read(desktopSave.Path)!=desktopSave.Source){desktopSave.Preview.Reset();throw std::runtime_error("Source save changed; inspect and preview again");}
    if(operation=="CleanupPreview")return desktopSave.Preview.Preview(desktopSave.Source,args,SaveCleanup::ReadRegistry());
    if(operation=="CleanupExport") {
        const auto plan=desktopSave.Preview.Consume(desktopSave.Source,args.value("Token",std::string{}),args.value("Confirm",false),SaveCleanup::ReadRegistry());
        return {{"File",SavePathText(WriteSaveCopy(HostServices::ExportsDirectory()/"saves","cleaned",plan.Save,desktopSave.Path))},{"Removed",plan.Removed},{"Status","Verified cleaned copy exported. Original untouched; selected mods may recreate their content."}};
    }
    throw std::runtime_error("Unsupported save-authoring operation");
}
