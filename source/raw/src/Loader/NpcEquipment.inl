// Authority owns the durable latch. Clients receive only authored rule IDs;
// no client-supplied equipment path or completion can mutate this ledger.
void DragonWildsNpcLoader::LoadNpcEquipmentWorld(UObject* context) {
    if(!context || !context->GetWorld())throw std::runtime_error("NPC equipment world is unavailable");
    ActorHelper::FunctionCall authority(context,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())throw std::runtime_error("Only world authority may access NPC equipment progress");
    auto* instance=SpawnRuntime::CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),TEXT("/Script/Engine.Default__GameplayStatics"),context);
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.PersistenceSubsystem"));
    if(!instance || !type)throw std::runtime_error("NPC equipment persistence subsystem unavailable");
    TArray<UObject*> candidates;UECustom::UObjectGlobals::GetObjectsOfClass(type,candidates,true);
    if(candidates.Num()<0 || candidates.Num()>64)throw std::runtime_error("NPC equipment persistence roster is invalid");
    UObject* subsystem=nullptr;
    for(auto* candidate:candidates)if(candidate && candidate->GetOuterPrivate()==instance
        && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
        if(subsystem)throw std::runtime_error("NPC equipment persistence subsystem is ambiguous");
        subsystem=candidate;
    }
    auto* settings=subsystem?CastField<FStructProperty>(PropertyHelper::GetPropertyByName(type,TEXT("WorldSaveSettings"))):nullptr;
    auto* schema=settings?settings->GetStruct().Get():nullptr;
    auto* guid=schema?CastField<FStructProperty>(PropertyHelper::GetPropertyByName(schema,TEXT("WorldSaveGuid"))):nullptr;
    if(!schema || schema->GetPathName()!=TEXT("/Script/Dominion.DomGameWorldSettings")
        || !ReflectedContract::Fits(settings,type->GetPropertiesSize()) || settings->GetArrayDim()!=1
        || settings->GetElementSize()!=schema->GetStructureSize() || !guid || guid->GetArrayDim()!=1
        || !guid->GetStruct().Get() || guid->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Guid")
        || guid->GetElementSize()!=16 || !ReflectedContract::Fits(guid,schema->GetStructureSize()))
        throw std::runtime_error("NPC equipment world-save GUID contract changed");
    uint32_t words[4]{};
    std::memcpy(words,guid->ContainerPtrToValuePtr<void>(settings->ContainerPtrToValuePtr<void>(subsystem)),sizeof(words));
    const auto world=PS::SaveReport::Guid(std::format("{:08X}{:08X}{:08X}{:08X}",words[0],words[1],words[2],words[3]));
    if(world.empty())throw std::runtime_error("NPC equipment is waiting for the loaded world-save GUID");
    if(world==m_equipmentWorldGuid)return;
    auto data=NpcEquipment::Read(PS::HostServices::ProgressDirectory()/"worlds"/world/"npc-equipment.json",world);
    m_equipmentWorldGuid=world;m_equipmentWorld=std::move(data);
}

std::vector<std::string> DragonWildsNpcLoader::ActiveEquipmentRules(UObject* context,const VendorDefinition& definition) {
    std::vector<std::string> active;
    if(definition.EquipmentWhen.empty())return active;
    LoadNpcEquipmentWorld(context);
    for(const auto& rule:definition.EquipmentWhen)
        if(NpcEquipment::Latched(m_equipmentWorld,definition.ModName+":"+definition.Id,rule))active.push_back(rule.Id);
    return active;
}

nlohmann::json DragonWildsNpcLoader::EffectiveNpcEquipment(AActor* actor,const VendorDefinition& definition) {
    if(definition.EquipmentWhen.empty())return definition.Equipment;
    ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    std::vector<std::string> active;
    if(authority.Result<bool>())active=ActiveEquipmentRules(actor,definition);
    else {
        const auto components=actor->GetComponentsByClass(RequireIdentityClass());
        if(components.Num()!=1)throw std::runtime_error("NPC equipment requires one replicated identity component");
        const auto identity=NpcIdentity::Decode(ReadIdentity(components[0]));
        if(identity.Mod!=definition.ModName || identity.Npc!=definition.Id
            || identity.Fingerprint!=NetworkDefinitionFingerprint(actor,definition))throw std::runtime_error("NPC equipment definition fingerprint mismatch");
        active=identity.EquipmentRules;
    }
    return NpcEquipment::Overlay(definition.Equipment,definition.EquipmentWhen,active);
}

void DragonWildsNpcLoader::RecordQuestEquipment(UObject* controller,const std::string& quest) {
    // Called only after confirmed native completion in the reward receipt, or
    // by the bounded server reconciliation of a saved completed receipt.
    bool needed=false;
    for(const auto& definition:m_definitions)if(definition.Enabled)
        for(const auto& rule:definition.EquipmentWhen)needed|=rule.Quest==quest;
    if(!needed)return;
    const QuestNative::Adapter native(controller,m_quests.Asset(quest));
    auto state=RC::to_string(native.StateName());if(const auto colon=state.rfind("::");colon!=state.npos)state=state.substr(colon+2);
    if(!native.IsInitialized() || state!="Complete")throw std::runtime_error("NPC equipment requires confirmed native quest completion");
    LoadNpcEquipmentWorld(controller);
    // Character quest progress can travel to another world. Persist proof of
    // the completion's world before the sidecar write, so recovery never adopts
    // an unrelated world's completed character quest merely on connection.
    for(size_t i=0;i<4;++i) {
        const auto word=static_cast<uint32_t>(std::stoul(m_equipmentWorldGuid.substr(i*8,8),nullptr,16));
        const FName key(RC::to_generic_string("RuneSchema.EquipmentWorld"+std::to_string(i)),FNAME_Add);
        native.SetInt(key,std::bit_cast<int32_t>(word));
        if(native.GetInt(key)!=std::bit_cast<int32_t>(word))throw std::runtime_error("NPC equipment completion world proof was not confirmed");
    }
    for(const auto& definition:m_definitions)if(definition.Enabled)for(const auto& rule:definition.EquipmentWhen) {
        if(rule.Quest!=quest || NpcEquipment::Latched(m_equipmentWorld,definition.ModName+":"+definition.Id,rule))continue;
        m_equipmentWorld=NpcEquipment::Latch(PS::HostServices::ProgressDirectory()/"worlds"/m_equipmentWorldGuid/"npc-equipment.json",
            m_equipmentWorldGuid,definition.ModName+":"+definition.Id,rule);
    }
    m_npcEquipmentElapsed=1; // Refresh on the next game tick, outside inventory callbacks.
}

void DragonWildsNpcLoader::RefreshNpcEquipment(AActor* actor,const VendorDefinition& definition) {
    if(!actor || definition.EquipmentWhen.empty())return;
    const auto equipment=EffectiveNpcEquipment(actor,definition);
    const auto key=RC::to_string(actor->GetPathName());
    if(m_appliedNpcEquipment.contains(key) && m_appliedNpcEquipment.at(key)==equipment.dump())return;
    auto* childType=ActorHelper::ResolveClass(TEXT("/Script/Engine.ChildActorComponent"));
    auto* previewType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.PlayerCharacterPreview"));
    UObject* preview=nullptr;
    if(childType && previewType)for(auto* child:actor->GetComponentsByClass(childType)) {
        auto* candidate=ActorHelper::GetObjectRef(child,TEXT("ChildActor"));
        if(!candidate || !candidate->IsA(previewType))continue;
        if(preview)throw std::runtime_error("NPC equipment preview is ambiguous");
        preview=candidate;
    }
    if(!preview || preview->GetWorld()!=actor->GetWorld())throw std::runtime_error("NPC equipment preview is not ready");
    nlohmann::json loadout=nlohmann::json::array();
    auto* wearable=ActorHelper::ResolveClass(TEXT("/Script/Dominion.WearableEquipmentData"));
    for(const auto& [slot,path]:equipment.items()) {
        if(slot=="MainHand" || slot=="OffHand")continue;
        auto* item=HumanEquipment(path.get<std::string>());
        if(!item || !wearable || !item->IsA(wearable) || !HumanNpc::MatchesWearableSlot(slot,HumanEnum(item,TEXT("Slot"))))
            throw std::runtime_error("NPC consequence equipment has the wrong native wearable slot: "+slot);
        loadout.push_back(RC::to_string(item->GetPathName()));
    }
    HumanCall(preview,TEXT("/Script/Dominion.PlayerCharacterPreview:HandleCharacterLoadoutChanged"),
        {{"CustomizationLoadout",loadout},{"bInIsNewCharacter",false}});
    m_appliedNpcEquipment[key]=equipment.dump();
}

void DragonWildsNpcLoader::ReconcileNpcEquipment(float deltaSeconds) {
    bool enabled=false;for(const auto& definition:m_definitions)enabled|=definition.Enabled && !definition.EquipmentWhen.empty();
    if(!enabled)return;
    m_npcEquipmentElapsed+=deltaSeconds;if(m_npcEquipmentElapsed<1)return;m_npcEquipmentElapsed=0;
    try {
        auto* world=FindLoadedWorld();if(!world)return;
        auto* state=ActorHelper::GetObjectRef(world,TEXT("GameState"));if(!state)return;
        ActorHelper::FunctionCall authority(state,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())return;
        LoadNpcEquipmentWorld(state);
        std::set<std::string> pending;
        for(const auto& definition:m_definitions)if(definition.Enabled)for(const auto& rule:definition.EquipmentWhen)
            if(!NpcEquipment::Latched(m_equipmentWorld,definition.ModName+":"+definition.Id,rule))pending.insert(rule.Quest);
        for(const auto& quest:pending)if(m_quests.HasAsset(quest)) {
            const auto& spec=m_quests.Find("_",quest); // Validate enabled definition, never create a quest here.
            (void)spec;
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
            TArray<UObject*> controllers;if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
            if(controllers.Num()<0 || controllers.Num()>256)throw std::runtime_error("NPC equipment player roster exceeds safe bounds");
            for(auto* controller:controllers) {
                if(!controller || controller->GetWorld()!=world || !IsGameplayQuestController(controller))continue;
                const QuestNative::Adapter native(controller,m_quests.Asset(quest));
                if(!native.IsInitialized() || native.GetInt(FName(TEXT("RuneSchema.Phase"),FNAME_Add))!=4)continue;
                auto name=RC::to_string(native.StateName());if(const auto colon=name.rfind("::");colon!=name.npos)name=name.substr(colon+2);
                if(name!="Complete")continue;
                bool sameWorld=true;
                for(size_t i=0;i<4;++i) {
                    const auto word=static_cast<uint32_t>(std::stoul(m_equipmentWorldGuid.substr(i*8,8),nullptr,16));
                    sameWorld&=native.GetInt(FName(RC::to_generic_string("RuneSchema.EquipmentWorld"+std::to_string(i)),FNAME_Add))==std::bit_cast<int32_t>(word);
                }
                if(sameWorld){RecordQuestEquipment(controller,quest);break;}
            }
        }
        for(auto& definition:m_definitions)if(definition.Enabled && !definition.EquipmentWhen.empty()) {
            if(auto* actor=FindSpawnedVendor(world,definition)) {
                // Publish before presentation. Dedicated servers never construct GUI previews.
                const auto components=actor->GetComponentsByClass(RequireIdentityClass());
                if(components.Num()!=1)continue; // Initial spawn owns component setup.
                const auto identity=NpcIdentity::Decode(ReadIdentity(components[0]));
                if(identity.EquipmentRules!=ActiveEquipmentRules(actor,definition))ApplyNetworkIdentity(actor,definition);
                if(PS::Network::Detect(actor).Mode!=PS::Network::Role::DedicatedServer)RefreshNpcEquipment(actor,definition);
            }
        }
    }catch(const std::exception& error){ErrorOnceDiagnostic("npc-equipment-world",PS::ToWideSafe(error.what()),
        "RS-NPC-026","NPC equipment world state could not be refreshed","Error","Partial",{},"EquipmentWorld","Application",true);}
}
