void RenderCleanup() {
    static char input[2048]{};
    static fs::path sourcePath;
    static fs::file_time_type sourceTime{};
    static uintmax_t sourceSize=0;
    static json source,removed;
    static std::map<std::string,size_t> owners;
    static std::set<std::string> installed,selected;
    static std::string message;
    static bool confirmed=false;
    static bool eraseProgress=false;
    static bool removeUnknownItems=false;
    static std::shared_ptr<const SaveCleanup::RegistrySnapshot> registry;
    ImGui::TextWrapped("Export a cleaned character save. The original and running world are never modified.");
    ImGui::TextWrapped("Supported: owned quest/dialogue/journal/lore progress, and registry-unknown inventory/equipment. World/NPC/building records and installed mods' known items are not removed here. Existing automatic startup cleanup is unchanged.");
    ImGui::TextWrapped("Do not clean the character currently loaded in game. Put offline character JSON files in the shared input folder, then select one below.");
    try {if(ImportSelector("Imported character save##cleanup",input,sizeof(input))) {source=nullptr;owners.clear();selected.clear();removed=nullptr;confirmed=false;}}
    catch(const std::exception& e){message=e.what();}
    if(ImGui::InputText("Character save JSON path",input,sizeof(input))) {
        source=nullptr;owners.clear();selected.clear();removed=nullptr;confirmed=false;
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
            sourcePath=path;sourceTime=before;sourceSize=size;source=std::move(data);owners=plan.Owners;installed=std::move(found);registry=SaveCleanup::ReadRegistry();
            message="Select owners below, preview removals, then export. Unknown means the saved owner is absent from this mod directory.";
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
        ImGui::BeginDisabled(!registry);
        if(ImGui::Checkbox("Remove inventory/equipment IDs absent from the loaded registry",&removeUnknownItems)){removed=nullptr;confirmed=false;}
        ImGui::EndDisabled();
        if(!registry)ImGui::TextWrapped("A complete registered item/recipe snapshot is unavailable. Load a world, then inspect this save again for unknown-item cleanup.");
        if(ImGui::Checkbox("Also erase progress for selected mods / selected unknown-ID cleanup",&eraseProgress)){removed=nullptr;confirmed=false;}
        ImGui::BeginDisabled(!registry);
        if(ImGui::Button("Nuclear cleanup: select all absent content")) {
            for(const auto& [owner,count]:owners)if(!installed.contains(owner))selected.insert(owner);
            removeUnknownItems=true;eraseProgress=true;removed=nullptr;confirmed=false;
            message="Nuclear cleanup selected: absent RuneSchema owners plus every registry-unknown inventory/equipment and discovery ID. Preview before exporting.";
        }
        ImGui::EndDisabled();
        if(!eraseProgress)ImGui::TextWrapped("Progress preservation is on: quest credit, dialogue flags, journal/lore unlocks and item/recipe discovery history are retained.");
        else ImGui::TextWrapped("Progress erasure removes selected owners' saved progress. With unknown-ID cleanup selected, it also removes registry-unknown item discovery and recipe unlock IDs.");
        const auto checkRegistry=[&]{if(removeUnknownItems && (!registry || SaveCleanup::ReadRegistry()!=registry))throw std::runtime_error("Registry snapshot changed or is unavailable; inspect the save again");};
        ImGui::BeginDisabled(selected.empty() && !removeUnknownItems);
        if(ImGui::Button("Preview removals")) {
            removed=nullptr;confirmed=false;
            try {checkRegistry();removed=SaveCleanup::Plan(source,selected,eraseProgress,removeUnknownItems?registry.get():nullptr).Removed;message="Preview ready. Unknown item IDs have no reliable mod owner; this option checks all inventory/equipment IDs. Content outside the supported scope is preserved.";}
            catch(const std::exception& e){message=e.what();}
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
                checkRegistry();
                const auto plan=SaveCleanup::Plan(source,selected,eraseProgress,removeUnknownItems?registry.get():nullptr);
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
