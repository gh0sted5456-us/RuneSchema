namespace {
struct JournalNativeRoutines {
    using Insert = void* (*)(void*, int32*, const void*, bool*);
    using Category = UObject* (*)(UObject*, uint8);
    using FixedCategory = UObject* (*)(UObject*);
    Insert insert{};
    Category category{};
    std::array<FixedCategory, 3> categories{};
    std::string error;
    JournalNativeRoutines() {
        try {
            const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x100000)
                throw std::runtime_error("Invalid game image for journal registration");
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
                || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
                || nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64)
                || !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections > 96)
                throw std::runtime_error("Unsupported journal game image");
            const auto size = nt->OptionalHeader.SizeOfImage;
            const auto* section = IMAGE_FIRST_SECTION(nt);
            if (reinterpret_cast<uintptr_t>(section) - base + sizeof(*section) * nt->FileHeader.NumberOfSections > size)
                throw std::runtime_error("Invalid journal game section table");
            std::vector<PS::AppearanceResolver::Section> sections;
            for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
                if (!(section[i].Characteristics & IMAGE_SCN_MEM_READ) || (section[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE)) continue;
                sections.push_back({section[i].VirtualAddress, section[i].Misc.VirtualSize,
                    (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0});
            }
            std::span<const uint8_t> image(reinterpret_cast<const uint8_t*>(base), size);
            if (PS::Storefront::AllowsGamePassNativeSignatures()) {
                uintptr_t addresses[5]{};
                for (size_t i = 0; i < std::size(addresses); ++i)
                    addresses[i] = base + PS::AppearanceResolver::Resolve(image, sections, JournalWinGDKContract::Definitions[i]);
                insert = reinterpret_cast<Insert>(addresses[0]);
                for (size_t i = 0; i < categories.size(); ++i)
                    categories[i] = reinterpret_cast<FixedCategory>(addresses[i + 1]);
            } else {
                uintptr_t addresses[3]{};
                for (size_t i = 0; i < std::size(addresses); ++i)
                    addresses[i] = base + PS::AppearanceResolver::Resolve(image, sections, JournalNativeContract::Definitions[i]);
                insert = reinterpret_cast<Insert>(addresses[0]);
                category = reinterpret_cast<Category>(addresses[1]);
            }
        } catch (const std::exception& e) { error = e.what(); }
    }
    UObject* ResolveCategory(UObject* root, uint8 categoryId) const {
        if (categoryId >= 1 && categoryId <= categories.size() && categories[categoryId - 1])
            return categories[categoryId - 1](root);
        return category ? category(root, categoryId) : nullptr;
    }
};

struct JournalHierarchyPlacement {
    JournalNativeRoutines::Insert insert{};
    void* map{};
    std::array<uint8, 28> value{};
    void Publish(UObject* entry) const {
        UECustom::FSoftObjectPtr key{UECustom::FSoftObjectPath(entry->GetPathName())};
        const void* pair[]{&key, value.data()};
        int32 index = -1;
        insert(map, &index, pair, nullptr);
        if (index < 0) throw std::runtime_error("Native journal hierarchy insertion failed");
    }
};

JournalHierarchyPlacement PrepareJournalHierarchy(UObject* subsystem, UObject* subcategory,
    FName entryKey, FName groupKey) {
    static const JournalNativeRoutines native;
    if (!native.error.empty()) throw std::runtime_error(native.error);
    if (!subsystem || subsystem->GetClassPrivate()->GetPropertiesSize() != 0x270)
        throw std::runtime_error("Journal subsystem layout does not match the validated hierarchy builder");
    auto* pathField = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), TEXT("JournalDataPath")));
    if (!pathField || pathField->GetOffset_Internal() != 0x1e0 || pathField->GetSize() != 32
        || !pathField->GetStruct() || pathField->GetStruct()->GetPathName() != TEXT("/Script/CoreUObject.SoftObjectPath"))
        throw std::runtime_error("Journal root path contract changed");
    const auto& path = *pathField->ContainerPtrToValuePtr<UECustom::FSoftObjectPath>(subsystem);
    UECustom::TSoftObjectPtr<UObject> rootReference{path};
    auto* root = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(rootReference);
    auto* rootClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, TEXT("/Script/Dominion.JournalData"));
    if (!root || !rootClass || !root->IsA(rootClass)) throw std::runtime_error("Journal root data is unavailable");
    JournalHierarchyPlacement result{native.insert, reinterpret_cast<uint8*>(subsystem) + 0x200};
    unsigned matches = 0;
    for (uint8 categoryId = 1; categoryId <= 3; ++categoryId) {
        auto* category = native.ResolveCategory(root, categoryId);
        if (!category) continue;
        auto scan = [&](FMapProperty* field, void* owner) {
            if (!field || !CastField<FNameProperty>(field->GetKeyProp())
                || !CastField<FObjectPropertyBase>(field->GetValueProp())
                || field->GetKeyProp()->GetSize() != sizeof(FName)
                || field->GetValueProp()->GetSize() != sizeof(UObject*)) return;
            UECustom::FScriptMapHelper map(field, field->ContainerPtrToValuePtr<void>(owner));
            map.ForEachPair([&](void* k, void* v) {
                UObject* object{}; std::memcpy(&object, v, sizeof(object));
                if (object != subcategory) return;
                ++matches;
                result.value[0] = categoryId;
                std::memcpy(result.value.data() + 4, k, sizeof(FName));
            });
        };
        for (auto* field : TFieldRange<FProperty>(category->GetClassPrivate(), EFieldIterationFlags::IncludeSuper)) {
            scan(CastField<FMapProperty>(field), category);
            if (auto* structure = CastField<FStructProperty>(field); structure && structure->GetStruct())
                for (auto* child : TFieldRange<FProperty>(structure->GetStruct().Get(), EFieldIterationFlags::Default))
                    scan(CastField<FMapProperty>(child), structure->ContainerPtrToValuePtr<void>(category));
        }
    }
    if (matches != 1) throw std::runtime_error("Journal subcategory must occur exactly once in the native journal root");
    std::memcpy(result.value.data() + 12, &entryKey, sizeof(entryKey));
    std::memcpy(result.value.data() + 20, &groupKey, sizeof(groupKey));
    return result;
}
}
