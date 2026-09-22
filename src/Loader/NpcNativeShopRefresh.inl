namespace {
    inline constexpr std::array<const char*,2> CraftTabIcons{
        "/Game/Art/UI/Craft/T_Icon_Craft_Normal.T_Icon_Craft_Normal",
        "/Game/Art/UI/Craft/T_Icon_Craft_Highlight.T_Icon_Craft_Highlight"};
    inline constexpr std::array<const char*,2> RepairTabIcons{
        "/Game/Art/UI/Craft/T_Icon_Repair_Normal.T_Icon_Repair_Normal",
        "/Game/Art/UI/Craft/T_Icon_Repair_Highlight.T_Icon_Repair_Highlight"};
    inline constexpr std::array<const char*,2> MasterworkTabIcons{
        "/Game/Art/UI/Craft/T_Icon_Ascend_Normal.T_Icon_Ascend_Normal",
        "/Game/Art/UI/Craft/T_Icon_Ascend_Highlight.T_Icon_Ascend_Highlight"};
    void PrepareClientShopTabAssets(UObject* station) {
        auto* repair=station?CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(station->GetClassPrivate(),TEXT("bHaveRepairOption"))):nullptr;
        auto* masterwork=station?CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(station->GetClassPrivate(),TEXT("bHaveMasterworkOption"))):nullptr;
        if(!repair || repair->GetArrayDim()!=1 || !masterwork || masterwork->GetArrayDim()!=1)
            throw std::runtime_error("Client merchant crafting tab option fields are unavailable");
        auto* textureClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.Texture2D"));
        if(!textureClass)throw std::runtime_error("Client merchant crafting tab texture class is unavailable");
        std::vector<const char*> paths(CraftTabIcons.begin(),CraftTabIcons.end());
        if(repair->GetPropertyValue(repair->ContainerPtrToValuePtr<void>(station)))
            paths.insert(paths.end(),RepairTabIcons.begin(),RepairTabIcons.end());
        if(masterwork->GetPropertyValue(masterwork->ContainerPtrToValuePtr<void>(station)))
            paths.insert(paths.end(),MasterworkTabIcons.begin(),MasterworkTabIcons.end());
        for(const auto* path:paths) {
            auto* texture=ActorHelper::ResolveObject(RC::to_generic_string(path));
            if(!texture || !texture->IsA(textureClass)
                || texture->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed)))
                throw std::runtime_error(std::string("Client merchant crafting tab icon is not ready: ")+path);
        }
        PS::Log<LogLevel::Verbose>(STR("Client merchant crafting tab assets ready: repair={}, masterwork={}.\n"),
            repair->GetPropertyValue(repair->ContainerPtrToValuePtr<void>(station)),
            masterwork->GetPropertyValue(masterwork->ContainerPtrToValuePtr<void>(station)));
    }
    void PrepareClientShopBanner(UObject* station,const std::string& path) {
        if(path.empty())return;
        auto* field=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(
            station->GetClassPrivate(),TEXT("VendorHeaderImage")));
        auto* textureClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr,nullptr,TEXT("/Script/Engine.Texture2D"));
        if(!field || field->GetArrayDim()!=1 || field->GetElementSize()!=sizeof(UObject*)
            || field->GetOffset_Internal()!=0x210 || !textureClass
            || field->GetPropertyClass().Get()!=textureClass)
            throw std::runtime_error("Client merchant banner: unsupported texture field layout");
        auto* texture=ActorHelper::ResolveObject(RC::to_generic_string(path));
        if(!texture || !texture->IsA(textureClass)
            || texture->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed)))
            throw std::runtime_error("Client merchant banner: texture is not ready: "+path);
        auto* address=field->ContainerPtrToValuePtr<void>(station);
        UObject* previous=nullptr;
        std::memcpy(&previous,address,sizeof(previous));
        if(previous==texture)return;
        // Match the project's validated TObjectPtr storage path; the UE4SS
        // virtual object setter is unavailable on this engine build.
        std::memcpy(address,&texture,sizeof(texture));
        if(field->GetObjectPropertyValue(address)!=texture)
            throw std::runtime_error("Client merchant banner: texture assignment verification failed");
        PS::Log<LogLevel::Verbose>(STR("Client merchant banner ready before menu entry: {}\n"),texture->GetPathName());
    }

    using NativeShopRefresh=void(*)(UObject*);
    NativeShopRefresh RequireNativeShopRefresh() {
        static const auto resolved=[] {
        struct Resolution {NativeShopRefresh function=nullptr;std::string error;};
        Resolution result;
        try {
        const auto* image=reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
        if(!image)throw std::runtime_error("Native shop refresh: executable unavailable");
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000)
            throw std::runtime_error("Native shop refresh: invalid executable header");
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64
            || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)
            || !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96)
            throw std::runtime_error("Native shop refresh: unsupported PE layout");
        const auto size=nt->OptionalHeader.SizeOfImage;
        const auto* section=IMAGE_FIRST_SECTION(nt);
        if(reinterpret_cast<const uint8_t*>(section)-image+sizeof(*section)*nt->FileHeader.NumberOfSections>size)
            throw std::runtime_error("Native shop refresh: invalid section table");
        std::vector<PS::AppearanceResolver::Section> sections;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            if(!(section[i].Characteristics&IMAGE_SCN_MEM_READ) || (section[i].Characteristics&IMAGE_SCN_MEM_DISCARDABLE))continue;
            sections.push_back({section[i].VirtualAddress,section[i].Misc.VirtualSize,
                (section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0});
        }
        const auto rva=PS::AppearanceResolver::Resolve({image,size},sections,NativeShopContract::Definition);
        const auto length=NativeShopContract::Definition.code.size()/2;
        const auto* code=image+rva;
        DWORD64 unwindBase=0;
        const auto* unwind=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(code),&unwindBase,nullptr);
        if(!unwind || unwindBase!=reinterpret_cast<DWORD64>(image)
            || unwind->BeginAddress!=rva || unwind->EndAddress-rva!=length)
            throw std::runtime_error("Native shop refresh: function boundary mismatch");
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(code,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT
            || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))
            || !(memory.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))
            || reinterpret_cast<uintptr_t>(code)+length>
                reinterpret_cast<uintptr_t>(memory.BaseAddress)+memory.RegionSize)
            throw std::runtime_error("Native shop refresh: function fingerprint mismatch");
        result.function=reinterpret_cast<NativeShopRefresh>(const_cast<uint8_t*>(code));
        PS::Log<LogLevel::Verbose>(STR("Client merchant cache routine validated at RVA 0x{:X}.\n"),rva);
        }catch(const std::exception& error){result.error=error.what();}
        return result;
        }();
        if(!resolved.function)throw std::runtime_error("Native shop refresh unavailable: "+resolved.error);
        return resolved.function;
    }

    template<class T> FProperty* ShopField(T* type,const TCHAR* name,int offset,int size) {
        if(!type)throw std::runtime_error("Native shop refresh: missing reflected type");
        auto* field=PropertyHelper::GetPropertyByName(type,name);
        if(!field || field->GetOffset_Internal()!=offset || field->GetElementSize()!=size || field->GetArrayDim()!=1)
            throw std::runtime_error("Native shop refresh: property layout mismatch: "+RC::to_string(name));
        return field;
    }

    bool RefreshClientShop(UObject* station) {
        auto native=RequireNativeShopRefresh();
        auto* expected=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.CraftingStationComponent"));
        if(!station || station->GetClassPrivate()!=expected || !expected || expected->GetPropertiesSize()!=0x2e8
            || station->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))
            || PS::Network::Detect(station).Mode!=PS::Network::Role::Client)
            throw std::runtime_error("Native shop refresh: requires a live native client station");
        auto* handle=CastField<FStructProperty>(ShopField(expected,TEXT("CraftingDataRowHandle"),0xf0,16));
        ShopField(expected,TEXT("CraftingDataRowCached"),0x100,232);
        auto* cached=CastField<FArrayProperty>(ShopField(expected,TEXT("CachedLabeledRecipes"),0x1e8,16));
        auto* vendor=CastField<FBoolProperty>(ShopField(expected,TEXT("bIsVendor"),0x20a,1));
        auto* valid=CastField<FArrayProperty>(ShopField(expected,TEXT("ValidRecipes"),0x2d0,16));
        auto* repair=CastField<FBoolProperty>(ShopField(expected,TEXT("bHaveRepairOption"),0x208,1));
        auto* masterwork=CastField<FBoolProperty>(ShopField(expected,TEXT("bHaveMasterworkOption"),0x209,1));
        ShopField(expected,TEXT("bHasVendorLevelReputationThresholds"),0x20b,1);
        ShopField(expected,TEXT("VendorHeaderImage"),0x210,8);
        if(!handle || !cached || !vendor || !valid)throw std::runtime_error("Native shop refresh: station property type mismatch");
        auto* ht=handle->GetStruct().Get();
        auto* tableField=CastField<FObjectPropertyBase>(ShopField(ht,TEXT("DataTable"),0,8));
        auto* nameField=CastField<FNameProperty>(ShopField(ht,TEXT("RowName"),8,8));
        if(!tableField || !nameField)throw std::runtime_error("Native shop refresh: invalid row handle");
        auto* hp=handle->ContainerPtrToValuePtr<void>(station);
        UObject* tableObject=nullptr;
        std::memcpy(&tableObject,tableField->ContainerPtrToValuePtr<void>(hp),sizeof(tableObject));
        auto* tableClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.DataTable"));
        if(!tableObject || !tableClass || !tableObject->IsA(tableClass))throw std::runtime_error("Native shop refresh: invalid table");
        auto* table=static_cast<UDataTable*>(tableObject);
        const auto name=*nameField->ContainerPtrToValuePtr<FName>(hp);
        auto* row=const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(table->FindRowUnchecked(name)));
        auto* rowType=table->GetRowStruct().Get();
        if(!row || !rowType || rowType->GetPropertiesSize()!=224 || rowType->GetPathName()!=TEXT("/Script/Dominion.CraftingStationDataTableRow"))
            throw std::runtime_error("Native shop refresh: unsupported row type");
        auto* groups=CastField<FArrayProperty>(ShopField(rowType,TEXT("LabeledRecipes"),24,16));
        auto* rowVendor=CastField<FBoolProperty>(ShopField(rowType,TEXT("bIsVendor"),162,1));
        auto* rowRepair=CastField<FBoolProperty>(ShopField(rowType,TEXT("bHaveRepairOption"),160,1));
        auto* rowMasterwork=CastField<FBoolProperty>(ShopField(rowType,TEXT("bHaveMasterworkOption"),161,1));
        ShopField(rowType,TEXT("VendorLevelReputationThresholds"),64,16);
        ShopField(rowType,TEXT("VendorHeaderImage"),168,40);
        auto* group=groups?CastField<FStructProperty>(groups->GetInner()):nullptr;
        auto* cachedGroup=CastField<FStructProperty>(cached->GetInner());
        auto* object=CastField<FObjectPropertyBase>(valid->GetInner());
        if(!group || !cachedGroup || group->GetStruct().Get()!=cachedGroup->GetStruct().Get()
            || group->GetElementSize()!=32 || !object || object->GetElementSize()!=8
            || !rowVendor || !repair || !rowRepair || !masterwork || !rowMasterwork
            || !rowVendor->GetPropertyValue(rowVendor->ContainerPtrToValuePtr<void>(row)))
            throw std::runtime_error("Native shop refresh: invalid vendor recipe layout");
        auto* collection=CastField<FArrayProperty>(ShopField(group->GetStruct().Get(),TEXT("Collection"),16,16));
        auto* soft=collection?CastField<FSoftObjectProperty>(collection->GetInner()):nullptr;
        if(!soft || soft->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))
            throw std::runtime_error("Native shop refresh: unsupported soft recipe reference");
        auto* recipeClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.RecipeData"));
        if(!recipeClass || object->GetPropertyClass().Get()!=recipeClass || soft->GetPropertyClass().Get()!=recipeClass)
            throw std::runtime_error("Native shop refresh: recipe class mismatch");
        const auto bounded=[](FScriptArray* array,int limit) {
            if(array->Num()<0 || array->Num()>limit || (array->Num() && !array->GetData()))
                throw std::runtime_error("Native shop refresh: invalid recipe array");
        };
        auto* source=groups->ContainerPtrToValuePtr<FScriptArray>(row);
        bounded(source,128);
        std::vector<UObject*> recipes;
        for(int i=0;i<source->Num();++i) {
            auto* entry=static_cast<uint8_t*>(source->GetData())+i*32;
            auto* items=collection->ContainerPtrToValuePtr<FScriptArray>(entry);
            bounded(items,128);
            for(int j=0;j<items->Num();++j) {
                const auto& path=reinterpret_cast<UECustom::FSoftObjectPtr*>(items->GetData())[j].ObjectID;
                if(path.SubPathString.GetCharArray().Num()>1)
                    throw std::runtime_error("Native shop refresh: recipe subobject path unsupported");
                auto* recipe=ActorHelper::ResolveObject(path.GetLongPackageFName().ToString()+TEXT(".")+path.GetAssetFName().ToString());
                if(!recipe || !recipe->IsA(recipeClass))throw std::runtime_error("Native shop refresh: recipe unavailable");
                if(std::find(recipes.begin(),recipes.end(),recipe)==recipes.end())recipes.push_back(recipe);
                if(recipes.size()>128)throw std::runtime_error("Native shop refresh: too many recipes");
            }
        }
        auto* target=valid->ContainerPtrToValuePtr<FScriptArray>(station);
        auto* labels=cached->ContainerPtrToValuePtr<FScriptArray>(station);
        bounded(target,128); bounded(labels,128);
        bool same=target->Num()==static_cast<int>(recipes.size());
        for(int i=0;same && i<target->Num();++i)same=static_cast<UObject**>(target->GetData())[i]==recipes[i];
        const auto* cachedRow=reinterpret_cast<const uint8_t*>(station)+0x100;
        bool rowMatches=cachedRow[224]==1;
        for(auto* field:TFieldRange<FProperty>(rowType,EFieldIterationFlags::Default)) {
            if(!rowMatches)break;
            rowMatches=field->Identical(field->ContainerPtrToValuePtr<void>(cachedRow),field->ContainerPtrToValuePtr<void>(row));
        }
        const auto repairMatches=repair->GetPropertyValue(repair->ContainerPtrToValuePtr<void>(station))
            ==rowRepair->GetPropertyValue(rowRepair->ContainerPtrToValuePtr<void>(row));
        const auto masterworkMatches=masterwork->GetPropertyValue(masterwork->ContainerPtrToValuePtr<void>(station))
            ==rowMasterwork->GetPropertyValue(rowMasterwork->ContainerPtrToValuePtr<void>(row));
        if(same && rowMatches && repairMatches && masterworkMatches
            && vendor->GetPropertyValue(vendor->ContainerPtrToValuePtr<void>(station))
            && groups->Identical(source,labels))return false;
        native(station);
        if(!vendor->GetPropertyValue(vendor->ContainerPtrToValuePtr<void>(station))
            || repair->GetPropertyValue(repair->ContainerPtrToValuePtr<void>(station))
                !=rowRepair->GetPropertyValue(rowRepair->ContainerPtrToValuePtr<void>(row))
            || masterwork->GetPropertyValue(masterwork->ContainerPtrToValuePtr<void>(station))
                !=rowMasterwork->GetPropertyValue(rowMasterwork->ContainerPtrToValuePtr<void>(row))
            || !groups->Identical(source,labels))
            throw std::runtime_error("Native shop refresh: native row rebuild verification failed");
        UECustom::FScriptArrayHelper array(target,valid);
        array.Empty();
        for(auto* recipe:recipes)array.Add(&recipe);
        if(target->Num()!=static_cast<int>(recipes.size()))throw std::runtime_error("Native shop refresh: valid recipe count mismatch");
        PS::Log<LogLevel::Verbose>(STR("Client merchant cache refreshed: repair={}, masterwork={}, recipes={}.\n"),
            rowRepair->GetPropertyValue(rowRepair->ContainerPtrToValuePtr<void>(row)),
            rowMasterwork->GetPropertyValue(rowMasterwork->ContainerPtrToValuePtr<void>(row)),recipes.size());
        return true;
    }
}
