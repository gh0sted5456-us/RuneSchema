namespace {
struct JournalPersistence {
    using Owners=JournalSave::Owners;
    using Shared=JournalJsonBridge::Shared;
    inline static std::unique_ptr<JournalJsonBridge> Api;
    inline static SafetyHookInline Reader,Writer;
    inline static UClass* ComponentClass=nullptr;
    inline static std::mutex Mutex;
    inline static std::map<const void*,Owners> Sources;
    inline static std::set<std::string> Errors;
    static void Report(const char* phase,const char* message) noexcept {
        try {
            std::lock_guard lock(Mutex);
            if(Errors.insert(std::string(phase)+":"+message).second)
                PS::Log<LogLevel::Error>(STR("Journal persistence {}: {}. Save data was not swept.\n"),PS::ToWideSafe(phase),PS::ToWideSafe(message));
        }catch(...) {}
    }
    static Owners CurrentOwners() {
        std::lock_guard lock(Mutex);Owners result;
        for(const auto& [source,entries]:Sources)for(const auto& [id,mod]:entries) {
            JournalSave::ValidateId(id);Quests::ValidateOwner(mod);
            const auto [found,added]=result.emplace(id,mod);
            if(!added && found->second!=mod)throw std::runtime_error("Conflicting journal persistence ownership");
        }
        return result;
    }
    static void ValidateInstance(void* persistence) {
        if(!persistence || !ComponentClass)throw std::runtime_error("Journal persistence context missing");
        auto* component=reinterpret_cast<UObject*>(static_cast<uint8_t*>(persistence)-0xc0);
        const auto index=component->GetInternalIndex();
        auto* slot=index>=0?FUObjectArray::IndexToObject(index):nullptr;
        if(!slot || slot->GetUObject()!=component || !component->IsA(ComponentClass)
            || component->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))
            throw std::runtime_error("Journal persistence component identity invalid");
    }
    static JournalSave::Json ReadPayload(void* json) {
        JournalSave::Json result=JournalSave::Json::object();
        for(const auto& [key,native]:{std::pair{"UnlockedEntries",TEXT("UnlockedEntries")},std::pair{"UnreadEntries",TEXT("UnreadEntries")}}) {
            const auto entries=Api->ReadArray(json,native);
            if(!entries)throw std::runtime_error("Native journal entry list missing");
            result[key]=*entries;
        }
        const auto metadata=Api->ReadArray(json,TEXT("RuneSchemaOwnership"));
        if(metadata)JournalSave::StoreOwnership(result,JournalSave::DecodeNative(*metadata));
        return result;
    }
    static std::set<std::string> ConfirmAbsent(const Owners& owners) {
        const auto mods=PS::HostServices::ModDirectory()/"mods";
        const auto active=DragonWilds::ModLoadOrder::ActiveOwners(mods);
        std::set<std::string> absent;
        for(const auto& [id,mod]:owners) {
            Quests::ValidateOwner(mod);
            if(!active.contains(mod))absent.insert(mod);
        }
        return absent;
    }
    static bool Read(void* persistence,Shared* json,int32_t version) {
        if(version<8)return Reader.call<bool>(persistence,json,version);
        try {
            ValidateInstance(persistence);
            if(!json || !json->Object || !json->Controller)throw std::runtime_error("Native journal JSON missing");
            auto payload=ReadPayload(json->Object);
            const auto saved=JournalSave::ReadOwnership(payload);
            const auto current=CurrentOwners();
            for(const auto& [id,owner]:saved) {
                const auto found=current.find(id);
                if(found!=current.end() && found->second!=owner)throw std::runtime_error("Saved journal ownership conflicts with loaded mod");
            }
            if(!saved.empty()) {
                const auto cleaned=JournalSave::RemoveAbsent(payload,ConfirmAbsent(saved),true);
                if(!cleaned.Removed.empty()) {
                    const auto fields=[](const auto& value) -> JournalSave::NativeFields {
                        return {value.at("UnlockedEntries").template get<std::vector<std::string>>(),
                            value.at("UnreadEntries").template get<std::vector<std::string>>(),
                            JournalSave::EncodeNative(JournalSave::ReadOwnership(value))};
                    };
                    const std::array names={TEXT("UnlockedEntries"),TEXT("UnreadEntries"),TEXT("RuneSchemaOwnership")};
                    JournalSave::ReplaceNativeFields(fields(payload),fields(cleaned.Journal),[&](size_t index,const auto& value) {
                        Api->WriteArray(json->Object,names[index],value);
                    });
                }
            }
        }catch(const std::exception& error) {
            Report("load",error.what());
            // This ABI consumes the incoming native shared reference on every
            // return path, including a rejected load. Do not leak or double-call.
            if(json)Api->release(json,1);
            return false;
        }catch(...) {Report("load","Unknown adapter failure");if(json)Api->release(json,1);return false;}
        return Reader.call<bool>(persistence,json,version);
    }
    static bool Write(void* persistence,Shared* json) {
        bool called=false;
        try {
            ValidateInstance(persistence);
            if(!json)throw std::runtime_error("Native journal JSON missing");
            JournalJsonBridge::Hold held(*Api,*json);
            called=true;
            if(!Writer.call<bool>(persistence,json))return false;
            const auto payload=ReadPayload(held.Object());
            const auto current=CurrentOwners();
            Owners included;
            for(const auto* name:{"UnlockedEntries","UnreadEntries"})for(const auto& entry:payload.at(name)) {
                const auto id=entry.get<std::string>();
                if(const auto found=current.find(id);found!=current.end())included.emplace(*found);
            }
            const auto previous=Api->ReadArray(held.Object(),TEXT("RuneSchemaOwnership"));
            if(!included.empty() || previous)
                Api->WriteArray(held.Object(),TEXT("RuneSchemaOwnership"),JournalSave::EncodeNative(included));
            return true;
        }catch(const std::exception& error) {
            Report("save",error.what());
        }catch(...) {Report("save","Unknown adapter failure");}
        if(!called && json)Api->release(json,1);
        return false;
    }
    static void Install(const void* source,Owners owners,UClass* type) {
        // Snapshot provenance, not callbacks into mutable loader containers.
        {std::lock_guard lock(Mutex);Sources[source]=std::move(owners);}
        if(Reader && Writer)return;
        auto api=std::make_unique<JournalJsonBridge>();
        for(const auto& [name,offset]:{std::pair{TEXT("UnlockedJournalEntries"),0xc8},std::pair{TEXT("UnreadJournalEntries"),0xd8}}) {
            auto* property=type?CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type,name)):nullptr;
            auto* inner=property?CastField<FObjectPropertyBase>(property->GetInner()):nullptr;
            if(!property || property->GetOffset_Internal()!=offset || property->GetElementSize()!=16 || property->GetArrayDim()!=1
                || !inner || inner->GetElementSize()!=8 || inner->GetArrayDim()!=1)
                throw std::runtime_error("Journal persistence component layout changed");
        }
        auto* defaults=type->GetClassDefaultObject().Get();
        if(!defaults)throw std::runtime_error("Journal persistence defaults missing");
        auto** table=*reinterpret_cast<void***>(reinterpret_cast<uint8_t*>(defaults)+0xc0);
        if(!table || reinterpret_cast<uintptr_t>(table[1])!=api->Writer || reinterpret_cast<uintptr_t>(table[2])!=api->Reader)
            throw std::runtime_error("Journal persistence interface does not match validated routines");
        ComponentClass=type;Api=std::move(api);
        auto read=safetyhook::InlineHook::create(reinterpret_cast<void*>(Api->Reader),reinterpret_cast<void*>(&Read),safetyhook::InlineHook::StartDisabled);
        auto write=safetyhook::InlineHook::create(reinterpret_cast<void*>(Api->Writer),reinterpret_cast<void*>(&Write),safetyhook::InlineHook::StartDisabled);
        if(!read || !write)throw std::runtime_error("Journal persistence detours could not be prepared");
        Reader=std::move(*read);Writer=std::move(*write);
        if(!Reader.enable() || !Writer.enable()) {
            Reader={};Writer={};throw std::runtime_error("Journal persistence detours could not be enabled");
        }
    }
};
}
