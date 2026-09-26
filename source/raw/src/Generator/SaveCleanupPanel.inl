void RenderCleanup() {
    static char input[2048]{};
    static fs::path sourcePath;
    static fs::file_time_type sourceTime{};
    static uintmax_t sourceSize=0;
    static json source,removed;
    static std::map<std::string,size_t> owners;
    static std::set<std::string> installed,selected;
    static std::shared_ptr<const SaveCleanup::RegistrySnapshot> registry;
    static std::string message;
    static bool confirmed=false;
    static bool eraseProgress=false;
    static bool pruneInvalidPersistence=false;
    ImGui::TextWrapped("Export a cleaned character save. The original and running world are never modified.");
    ImGui::TextWrapped("Owned RuneSchema quest, dialogue, location, journal and lore records can be removed by owner. Safe Clean can also compare saved item, recipe and quest PersistenceIDs against the fully loaded native registries and remove identities that no longer exist.");
    ImGui::TextWrapped("Do not clean the character currently loaded in game. Put offline character JSON files in the shared input folder, then select one below.");
    try {if(ImportSelector("Imported character save##cleanup",input,sizeof(input))) {source=nullptr;owners.clear();selected.clear();removed=nullptr;registry.reset();pruneInvalidPersistence=false;confirmed=false;}}
    catch(const std::exception& e){message=e.what();}
    if(ImGui::InputText("Character save JSON path",input,sizeof(input))) {
        source=nullptr;owners.clear();selected.clear();removed=nullptr;registry.reset();pruneInvalidPersistence=false;confirmed=false;
    }
    if(ImGui::Button("Inspect save and scan mods")) {
        source=nullptr;owners.clear();selected.clear();removed=nullptr;confirmed=false;
        try {
            const std::string utf8=input;
            const auto path=fs::canonical(fs::path(std::u8string(utf8.begin(),utf8.end())));
            if(path.extension()!=L".json")throw std::runtime_error("Select a character .json save; binary world saves are unsupported");
            const auto before=fs::last_write_time(path);const auto size=fs::file_size(path);
            auto data=Read(path);const auto plan=SaveCleanup::Plan(data,{});
            const auto root=HostServices::ModDirectory()/"mods";
            if(!fs::is_directory(root))throw std::runtime_error("Mod directory unavailable; absent-mod classification refused");
            std::set<std::string> found;
            for(const auto& entry:fs::directory_iterator(root))if(entry.is_directory()) {
                const auto name=entry.path().filename().u8string();found.emplace(name.begin(),name.end());
            }
            if(fs::last_write_time(path)!=before || fs::file_size(path)!=size)throw std::runtime_error("Save changed during inspection; inspect again");
            sourcePath=path;sourceTime=before;sourceSize=size;source=std::move(data);owners=plan.Owners;installed=std::move(found);
            registry=SaveCleanup::ReadRegistry();
            pruneInvalidPersistence=false;
            message=registry && registry->Ready()
                ?"Select owners and/or enable invalid PersistenceID cleanup, preview removals, then export. Unknown means the saved owner is absent from this mod directory."
                :"Select owners, preview removals, then export. Live item/recipe registry validation is unavailable until a world has fully loaded.";
        }catch(const std::exception& e){message=e.what();}
    }
    if(!source.is_null()) {
        for(const bool present:{true,false}) {
            ImGui::SeparatorText(present?"Installed mods (including disabled folders)":"Unknown / absent mods");
            ImGui::PushID(present?1:0);
            if(ImGui::SmallButton("Select group")) {for(const auto& [owner,count]:owners)if(installed.contains(owner)==present)selected.insert(owner);removed=nullptr;confirmed=false;}
            ImGui::SameLine();
            if(ImGui::SmallButton("Clear group")) {for(const auto& [owner,count]:owners)if(installed.contains(owner)==present)selected.erase(owner);removed=nullptr;confirmed=false;}
            bool any=false;
            for(const auto& [owner,count]:owners)if(installed.contains(owner)==present) {
                any=true;bool checked=selected.contains(owner);
                if(ImGui::Checkbox(owner.c_str(),&checked)) {if(checked)selected.insert(owner);else selected.erase(owner);removed=nullptr;confirmed=false;}
                ImGui::SameLine();ImGui::TextDisabled("(%zu owned records)",count);
            }
            if(!any)ImGui::TextDisabled("No saved owners in this group.");
            ImGui::PopID();
        }
        if(ImGui::Checkbox("Erase progress for selected RuneSchema owners",&eraseProgress)){removed=nullptr;confirmed=false;}
        if(!eraseProgress)ImGui::TextWrapped("Progress preservation is on: quest credit, dialogue flags, journal/lore unlocks and item/recipe discovery history are retained unless invalid PersistenceID cleanup is enabled below.");
        else ImGui::TextWrapped("Progress erasure removes saved quest, dialogue, location, journal and lore records for selected RuneSchema owners.");

        const bool registryReady=registry && registry->Ready();
        ImGui::BeginDisabled(!registryReady);
        if(ImGui::Checkbox("Remove invalid item/recipe/quest PersistenceIDs",&pruneInvalidPersistence)){removed=nullptr;confirmed=false;}
        ImGui::EndDisabled();
        if(!registryReady) {
            pruneInvalidPersistence=false;
            ImGui::TextDisabled("Live registry unavailable. Enter a fully loaded world, then inspect the save again.");
        } else if(pruneInvalidPersistence) {
            ImGui::TextWrapped("Registry repair removes saved inventory/loadout item IDs, discovery/unlock IDs, and (when the quest registry is complete) quest IDs that are absent from the current native registries. Disabled or missing mods therefore count as absent.");
        }

        ImGui::BeginDisabled(selected.empty() && !pruneInvalidPersistence);
        if(ImGui::Button("Preview removals")) {
            removed=nullptr;confirmed=false;
            try {
                const auto* validation=pruneInvalidPersistence?registry.get():nullptr;
                removed=SaveCleanup::Plan(source,selected,eraseProgress,validation,false,pruneInvalidPersistence).Removed;
                message=pruneInvalidPersistence
                    ?"Preview ready. Owned records and registry-invalid PersistenceIDs are listed below."
                    :"Preview ready. Only explicitly owned RuneSchema records are eligible.";
            } catch(const std::exception& e){message=e.what();}
        }
        ImGui::EndDisabled();
        if(removed.is_array()) {
            ImGui::Text("%zu records will be removed from the copy.",removed.size());
            if(ImGui::TreeNode("Removal details")) {
                for(const auto& row:removed)ImGui::TextWrapped("%s | %s",row.at("Kind").get_ref<const std::string&>().c_str(),row.at("Id").get_ref<const std::string&>().c_str());
                ImGui::TreePop();
            }
            ImGui::Checkbox("I understand installed mods may recreate this content",&confirmed);
            ImGui::BeginDisabled(!confirmed || removed.empty());
            if(ImGui::Button("Export cleaned copy"))try {
                if(fs::last_write_time(sourcePath)!=sourceTime || fs::file_size(sourcePath)!=sourceSize || Read(sourcePath)!=source)
                    throw std::runtime_error("Source save changed; inspect and preview it again before export");
                const auto* validation=pruneInvalidPersistence?registry.get():nullptr;
                const auto plan=SaveCleanup::Plan(source,selected,eraseProgress,validation,false,pruneInvalidPersistence);
                if(plan.Removed!=removed)throw std::runtime_error("Cleanup preview changed; inspect again");
                auto text=plan.Save.dump(1,'\t');
                std::ifstream encoding(sourcePath,std::ios::binary);char bom[2]{};encoding.read(bom,2);
                if(static_cast<unsigned char>(bom[0])==0xff && static_cast<unsigned char>(bom[1])==0xfe) {
                    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
                    if(count<=0)throw std::runtime_error("Export encoding conversion failed");
                    std::wstring wide(count,L'\0');
                    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),wide.data(),count)!=count)throw std::runtime_error("Export encoding conversion failed");
                    text.assign("\xff\xfe",2);text.append(reinterpret_cast<const char*>(wide.data()),wide.size()*sizeof(wchar_t));
                }
                const auto directory=HostServices::ExportsDirectory()/"saves";fs::create_directories(directory);
                const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                const auto output=directory/(L"cleaned_"+std::to_wstring(stamp)+L".json");
                const auto handle=CreateFileW(output.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create unique cleaned-save output");
                DWORD written=0;const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) && written==text.size();
                const bool flushed=ok && FlushFileBuffers(handle);CloseHandle(handle);
                if(!flushed)throw std::runtime_error("Export write failed; original untouched, do not use the incomplete output");
                if(Read(output)!=plan.Save)throw std::runtime_error("Export verification failed; do not use this output");
                message="Verified cleaned copy: "+output.string()+". Original untouched. Disable/remove selected mods before using the copy.";
                confirmed=false;
            }catch(const std::exception& e){message=e.what();}
            ImGui::EndDisabled();
        }
    }
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
}
