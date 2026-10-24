namespace {
    namespace BuildingOwnership=PS::BuildingPropOwnership;
    FBoolProperty* ManagedBuildingSkipResult(UFunction* function) noexcept {try{
        auto* result=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
        if(!function||function->HasAnyFunctionFlags(FUNC_Net)||!ReflectedContract::Output(result,function)
            ||result->GetByteOffset()>=result->GetElementSize()||!result->GetFieldMask()||ReflectedContract::Count(function)!=1)return nullptr;
        return result;
        }catch(...){return nullptr;}
    }
    UFunction* ManagedBuildingSkipFunction(UClass* type){
        auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.BaseBuildingActor"));
        if(!type||!base||!type->IsChildOf(base))return nullptr;
        auto* defaults=type->GetClassDefaultObject().Get();auto* function=defaults?defaults->GetFunctionByNameInChain(TEXT("ShouldSkip")):nullptr;
        return ManagedBuildingSkipResult(function)?function:nullptr;
    }
    BuildingOwnership::Guid ManagedBuildingGuid(UObject* object){
        auto* property=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("SpudGuid")));
        if(!property||!property->GetStruct()||property->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Guid")
            ||property->GetArrayDim()!=1||property->GetElementSize()!=16
            ||!ReflectedContract::Fits(property,object->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Managed building SpudGuid layout is unavailable");
        BuildingOwnership::Guid result{};std::memcpy(result.data(),property->ContainerPtrToValuePtr<void>(object),16);return result;
    }
    bool ManagedBuildingDestroying(UObject* object){
        if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return true;
        auto* flag=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("bActorIsBeingDestroyed")));
        if(!flag||flag->GetArrayDim()!=1||flag->GetElementSize()!=1||!ReflectedContract::Fits(flag,object->GetClassPrivate()->GetPropertiesSize()))
            throw std::runtime_error("Managed building destruction-state contract is unavailable");
        return flag->GetPropertyValue(flag->ContainerPtrToValuePtr<void>(object));
    }
    bool ManagedBuildingExcluded(UObject* object){
        auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("bSkipSpudStore")));
        if(skip)return skip->GetArrayDim()==1&&skip->GetElementSize()==1&&ReflectedContract::Fits(skip,object->GetClassPrivate()->GetPropertiesSize())
            &&skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(object));
        auto* function=ManagedBuildingSkipFunction(object->GetClassPrivate());if(!function)return false;
        std::vector<uint8_t> frame(function->GetParmsSize(),0);object->ProcessEvent(function,frame.data());
        auto* result=ManagedBuildingSkipResult(function);return result&&result->GetPropertyValue(result->ContainerPtrToValuePtr<void>(frame.data()));
    }
    bool ManagedBuildingBinding(UObject* actor,const BuildingOwnership::Record& record){
        if(record.Binding.empty())return false; // No saved numeric index is ownership proof across registry rebuilds.
        auto* property=PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),RC::to_generic_string(record.Binding));
        if(!property||property->GetArrayDim()!=1||!ReflectedContract::Fits(property,actor->GetClassPrivate()->GetPropertiesSize()))return false;
        if(auto* hard=CastField<FObjectProperty>(property)){
            if(hard->GetElementSize()!=sizeof(UObject*))return false;
            auto* value=hard->GetObjectPropertyValue(hard->ContainerPtrToValuePtr<void>(actor));
            return value&&!value->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))&&RC::to_string(value->GetPathName())==record.Building;
        }
        if(auto* soft=CastField<FSoftObjectProperty>(property)){
            if(soft->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))return false;
            const auto& path=soft->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(actor)->ObjectID;
            const UECustom::FSoftObjectPath expected(RC::to_generic_string(record.Building));
            return path.SubPathString.GetCharArray().Num()<=1&&path.AssetPath.GetPackageName()==expected.AssetPath.GetPackageName()
                &&path.AssetPath.GetAssetName()==expected.AssetPath.GetAssetName();
        }
        return false;
    }
}
PS::BuildingPropOwnership::Record DragonWildsSpawnLoader::BuildingOwnershipRecord(const SpawnInfo& spawn) const {
    BuildingOwnership::Record record{RC::to_string(spawn.ModName),RC::to_string(spawn.EntryId),RC::to_string(spawn.ClassPath),spawn.BuildingDataPath,spawn.BuildingObjectProperty};
    BuildingOwnership::Validate(record);return record;
}
void DragonWildsSpawnLoader::RememberBuildingOwnership(){
    std::vector<BuildingOwnership::Record> current;
    for(const auto& spawn:m_spawns)if(spawn.bBuildingProp){const auto record=BuildingOwnershipRecord(spawn);if(std::find(current.begin(),current.end(),record)==current.end())current.push_back(record);}
    const auto next=BuildingOwnership::Remember(m_buildingOwnership,current,m_buildingOwnershipScanHealthy);
    if(next!=m_buildingOwnership){
        const auto document=BuildingOwnership::Encode(next);PS::CacheBundleIO::Publish(m_buildingOwnershipFile,document.dump());
        if(BuildingOwnership::Decode(nlohmann::json::parse(PS::CacheBundleIO::Read(m_buildingOwnershipFile,2*1024*1024)))!=next)
            throw std::runtime_error("Managed building ownership history did not round-trip");
        m_buildingOwnership=next;
    }
}
void DragonWildsSpawnLoader::CompleteBuildingOwnershipSnapshot(bool complete){
    m_buildingOwnershipReady=false;
    if(!complete||!m_buildingOwnershipScanHealthy||!HasInitialized())return;
    try{
        auto platform=std::string(PS::Storefront::MappingCachePlatform(PS::Storefront::CurrentDetection()));
        std::ranges::transform(platform,platform.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        m_buildingOwnershipFile=PS::RuneSchemaData::PlatformDirectory(platform)/"ownership"/"building-props.json";
        PS::CacheBundleIO::Safe(m_buildingOwnershipFile);fs::create_directories(m_buildingOwnershipFile.parent_path());
        if(!m_buildingOwnershipLock){
            const auto path=m_buildingOwnershipFile.wstring()+L".lock";PS::CacheBundleIO::Safe(path);
            const auto handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Another runtime owns this platform's building history; cleanup deferred");
            m_buildingOwnershipLock=reinterpret_cast<std::uintptr_t>(handle);
        }
        if(fs::exists(m_buildingOwnershipFile))m_buildingOwnership=BuildingOwnership::Decode(nlohmann::json::parse(PS::CacheBundleIO::Read(m_buildingOwnershipFile,2*1024*1024)));
        RememberBuildingOwnership();m_buildingOwnershipReady=true;
        PS::Log<LogLevel::Normal>(STR("[BUILDING-PROPS][OWNERSHIP][READY] {} historical definitions; only exact owned instances can be migrated or retired.\n"),m_buildingOwnership.size());
    }catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[BUILDING-PROPS][OWNERSHIP][DEFERRED] {}. Existing world actors retained.\n"),PS::ToWideSafe(error.what()));}
}
void DragonWildsSpawnLoader::TrackManagedBuildingSaveExclusion(AActor* actor){
    auto* function=ManagedBuildingSkipFunction(actor->GetClassPrivate());if(!function)return;
    if(m_protectedBuildingDestroyGuard==Hook::ERROR_ID)throw std::runtime_error("Managed building SPUD save-exclusion guard is unavailable");
    std::erase_if(m_managedBuildingSaveTokens,[](const auto& token){return !token.Actor.Get()||!token.Function.Get();});
    for(const auto& token:m_managedBuildingSaveTokens)if(token.Actor.Get()==actor&&token.Function.Get()==function)return;
    if(m_managedBuildingSaveTokens.size()>=4096)throw std::runtime_error("Managed building save-exclusion instance limit exceeded");
    ManagedBuildingSaveToken token{PS::WeakObjectHandle(actor),PS::WeakObjectHandle(function)};
    if(token.Actor.Get()!=actor||token.Function.Get()!=function)throw std::runtime_error("Managed building save-exclusion weak identity unavailable");
    m_managedBuildingSaveTokens.push_back(std::move(token));
}
void DragonWildsSpawnLoader::MarkManagedBuilding(AActor* actor,const SpawnInfo& spawn){
    if(!spawn.bBuildingProp)return;
    auto* property=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),TEXT("Tags")));
    auto* inner=property?CastField<FNameProperty>(property->GetInner()):nullptr;
    if(!property||!inner||property->GetArrayDim()!=1||property->GetElementSize()!=sizeof(FScriptArray)||inner->GetElementSize()!=sizeof(FName)
        ||!ReflectedContract::Fits(property,actor->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Managed building Tags contract changed");
    auto* array=property->ContainerPtrToValuePtr<FScriptArray>(actor);
    if(array->Num()<0||array->Num()>254||(array->Num()&&!array->GetData()))throw std::runtime_error("Managed building Tags exceeds its bound");
    const auto record=BuildingOwnershipRecord(spawn);const auto guid=BuildingOwnership::ActorGuid(record.Identity());
    const std::array tags={"RuneSchema.Owner:"+record.Owner,std::format("RuneSchema.BuildingPlacement:v1:{:08X}{:08X}{:08X}{:08X}",guid[0],guid[1],guid[2],guid[3])};
    UECustom::FScriptArrayHelper helper(array,property);
    for(const auto& tag:tags){
        FName name(RC::to_generic_string(tag),FNAME_Add);bool found=false;
        for(int i=0;i<array->Num();++i){const auto value=*reinterpret_cast<FName*>(static_cast<uint8_t*>(array->GetData())+i*sizeof(FName));
            const auto text=RC::to_string(value.ToString());
            if(text.starts_with("RuneSchema.Owner:")&&text!=tags[0])throw std::runtime_error("Managed building owner tag conflicts");
            if(text.starts_with("RuneSchema.BuildingPlacement:")&&text!=tags[1])throw std::runtime_error("Managed building placement tag conflicts");
            found|=value==name;
        }
        if(!found)helper.Add(&name);
    }
    if(ManagedBuildingGuid(actor)!=guid||!actor->HasAnyFlags(RF_Transient))
        throw std::runtime_error("Managed building identity or transient flag did not round-trip");
    TrackManagedBuildingSaveExclusion(actor);
    if(!ManagedBuildingExcluded(actor))
        throw std::runtime_error("Managed building identity or save exclusion did not round-trip");
}
void DragonWildsSpawnLoader::ReconcileBuildingOwnership(float deltaSeconds){
    if(!m_buildingOwnershipReady||!m_readyWorld||!GetGameMode(m_readyWorld))return;
    m_buildingOwnershipElapsed+=deltaSeconds;if(m_buildingOwnershipElapsed<5.0)return;m_buildingOwnershipElapsed=0;
    try{
        if(!PS::RegistryEnrollment::Healthy()||PS::RegistryEnrollment::PendingCount()!=0
            ||!BuildingOwnershipRegistryReady||!BuildingOwnershipRegistryReady())return;
        RememberBuildingOwnership();if(m_buildingOwnership.empty())return;
        std::map<BuildingOwnership::Guid,std::vector<const BuildingOwnership::Record*>> identities;
        for(const auto& record:m_buildingOwnership)for(bool legacy:{false,true})identities[BuildingOwnership::ActorGuid(record.Identity(),legacy)].push_back(&record);
        std::vector<BuildingOwnership::Record> active;for(const auto& spawn:m_spawns)if(spawn.bBuildingProp)active.push_back(BuildingOwnershipRecord(spawn));
        auto* type=ResolveClass(TEXT("/Script/Engine.Actor"));if(!type)throw std::runtime_error("Managed building actor class unavailable");
        TArray<UObject*> actors;UECustom::UObjectGlobals::GetObjectsOfClass(type,actors,true);
        if(actors.Num()<0||actors.Num()>262144)throw std::runtime_error("Managed building actor roster exceeds its bound");
        std::set<std::string> kept;size_t retired=0;
        for(auto* object:actors){
            if(!object||object->GetWorld()!=m_readyWorld||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization)))continue;
            auto* property=PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("SpudGuid"));if(!property||property->GetSize()!=16)continue;
            const auto guid=ManagedBuildingGuid(object);const auto found=identities.find(guid);if(found==identities.end()||ManagedBuildingDestroying(object))continue;
            const BuildingOwnership::Record* matched=nullptr;bool enabled=false;
            for(const auto* record:found->second)if(RC::to_string(object->GetClassPrivate()->GetPathName())==record->Class&&ManagedBuildingBinding(object,*record)){
                if(matched&&matched->Identity()!=record->Identity())throw std::runtime_error("Ambiguous managed building owner; cleanup refused");matched=record;
                enabled=enabled||std::find(active.begin(),active.end(),*record)!=active.end();
            }
            if(!matched)continue;
            const auto identity=matched->Identity();
            const auto action=BuildingOwnership::Decide({true,true,true,true,true,true},enabled&&!kept.contains(identity),guid==BuildingOwnership::ActorGuid(identity),object->HasAnyFlags(RF_Transient),ManagedBuildingExcluded(object));
            if(action==BuildingOwnership::Action::Retire){RetireTimedActor(static_cast<AActor*>(object));if(!ManagedBuildingDestroying(object))throw std::runtime_error("Owned building retirement was not confirmed; recreation deferred");++retired;}
            else if(action==BuildingOwnership::Action::Keep)kept.insert(identity);
        }
        if(retired)PS::Log<LogLevel::Normal>(STR("[BUILDING-PROPS][OWNERSHIP][RETIRED] {} verified legacy/duplicate/inactive instances in the current world. Player-built buildings were not swept.\n"),retired);
    }catch(const std::exception& error){if(m_buildingOwnershipNotices.size()<32&&m_buildingOwnershipNotices.insert(error.what()).second)
        PS::Log<LogLevel::Warning>(STR("[BUILDING-PROPS][OWNERSHIP][DEFERRED] {}\n"),PS::ToWideSafe(error.what()));}
}
