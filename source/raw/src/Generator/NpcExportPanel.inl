void PumpNpcExport() {
    NpcExportRequests::Available=true;
    auto request=NpcExportRequests::Take();if(!request)return;
    try {
        auto preview=std::move(*request);
        Resolve(preview["Rows"],TEXT("/Script/Dominion.ItemData"));
        for(auto& row:preview["Rows"])if(row["AssetPath"].is_string()) {
            auto* item=ActorHelper::ResolveObject(to_generic_string(row["AssetPath"].get<std::string>()));
            auto* property=item?TextField(item,TEXT("Slot")):nullptr;
            UEnum* enumeration=nullptr;int64 number=0;
            if(auto* field=CastField<FEnumProperty>(property);field && field->GetUnderlyingProperty()) {
                enumeration=field->GetEnum();number=field->GetUnderlyingProperty()->GetSignedIntPropertyValue(field->ContainerPtrToValuePtr<void>(item));
            }else if(auto* field=CastField<FByteProperty>(property)) {
                enumeration=field->GetEnum().Get();number=field->GetUnsignedIntPropertyValue(field->ContainerPtrToValuePtr<void>(item));
            }
            if(!enumeration)continue;
            auto name=to_string(enumeration->GetNameByValue(number).ToString());
            if(const auto colon=name.rfind(':');colon!=name.npos)name=name.substr(colon+1);
            row["Slot"]=name;
        }
        NpcSaveExport::ApplyResolved(preview);
        preview["Status"]="Preview ready. Unresolved equipment is omitted with warnings. Export is disabled until you enable and place the NPC.";
        NpcExportRequests::Publish(preview);
    }catch(const std::exception& e){NpcExportRequests::Publish({{"Status",e.what()}});}
}
void RenderNpcExport() {
    static char path[2048]{},id[129]="imported_human";
    static float position[3]{},yaw=0;
    static std::string message;
    ImGui::TextWrapped("Read a native character JSON save and export appearance plus equipped item paths. Load a world with the item's mods enabled first. The original save is never modified.");
    try {if(ImportSelector("Imported character save##npc",path,sizeof(path))) {NpcExportRequests::Clear();message.clear();}}
    catch(const std::exception& e){message=e.what();}
    if(ImGui::InputText("Character save JSON path##npc",path,sizeof(path))) {NpcExportRequests::Clear();message.clear();}
    if(ImGui::Button("Read save and resolve equipment"))try {
        const std::string utf8=path;
        auto file=fs::canonical(fs::path(std::u8string(utf8.begin(),utf8.end())));
        if(file.extension()!=L".json")throw std::runtime_error("Only character .json saves are supported");
        NpcExportRequests::Submit(NpcSaveExport::Preview(Read(file)));message.clear();
    }catch(const std::exception& e){NpcExportRequests::Publish({{"Status",e.what()}});}
    const auto preview=NpcExportRequests::Read();
    ImGui::TextWrapped("%s",preview.value("Status",std::string{}).c_str());
    if(preview.contains("NPC")) {
        ImGui::Text("Character: %s",preview["NPC"]["DisplayName"].get_ref<const std::string&>().c_str());
        for(const auto& warning:preview["Warnings"])ImGui::TextWrapped("Warning: %s",warning.get_ref<const std::string&>().c_str());
        if(ImGui::TreeNode("Appearance and equipment preview")){ImGui::TextWrapped("%s",preview["NPC"].dump(2).c_str());ImGui::TreePop();}
        ImGui::InputText("NPC ID",id,sizeof(id));ImGui::InputFloat3("World position (cm)",position);ImGui::InputFloat("Yaw (degrees)",&yaw);
        if(ImGui::Button("Export disabled NPC definition"))try {
            const std::string key=id;
            if(key.empty() || key.size()>128 || key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=key.npos)
                throw std::runtime_error("NPC ID must contain only letters, numbers, underscores or hyphens");
            for(float value:position)if(!std::isfinite(value))throw std::runtime_error("Position must be finite");
            if(!std::isfinite(yaw))throw std::runtime_error("Yaw must be finite");
            auto npc=preview["NPC"];npc["Id"]=key;npc["Location"]={position[0],position[1],position[2]};
            npc["Rotation"]={{"Pitch",0},{"Yaw",yaw},{"Roll",0}};
            const auto directory=HostServices::ExportsDirectory()/"npcs";fs::create_directories(directory);
            const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto output=directory/(key+"_"+std::to_string(stamp)+".json");
            const auto text=npc.dump(2);
            HANDLE file=CreateFileW(output.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create unique NPC export");
            DWORD bytes=0;const bool ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&bytes,nullptr) && bytes==text.size();CloseHandle(file);
            if(!ok || Read(output)!=npc)throw std::runtime_error("NPC export verification failed");
            message="Exported: "+output.string();
        }catch(const std::exception& e){message=e.what();}
    }
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
}
