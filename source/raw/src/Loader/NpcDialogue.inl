namespace {
    bool DialogueIsActive(UObject* participant) {
        const auto* path=TEXT("/Script/CommonConversationRuntime.ConversationParticipantComponent:IsInActiveConversation");
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        auto* result=fn?CastField<FBoolProperty>(fn->GetReturnProperty()):nullptr;
        size_t count=0;if(fn)for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++count;
        if(!participant || !result || count!=1 || fn->GetParmsSize()!=1 || result->GetOffset_Internal()!=0
            || result->GetElementSize()!=1 || result->GetArrayDim()!=1 || !result->IsNativeBool() || result->GetByteOffset()!=0)
            throw std::runtime_error("Conversation active-state contract changed");
        ActorHelper::FunctionCall call(participant,path);call.Invoke();return call.Result<bool>();
    }
    std::string DialogueCharacter(UObject* controller) {
        if(!controller)throw std::runtime_error("Dialogue player controller is unavailable");
        ActorHelper::FunctionCall identity(controller,TEXT("/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid"));
        identity.Invoke();uint32_t words[4]{};identity.MoveResult(words,sizeof(words));
        const auto result=PS::SaveReport::Guid(std::format("{:08X}{:08X}{:08X}{:08X}",words[0],words[1],words[2],words[3]));
        if(result.empty())throw std::runtime_error("Dialogue character identity is unavailable");
        return result;
    }
    std::filesystem::path DialogueProgressPath(const std::string& character) {
        if(PS::SaveReport::Guid(character)!=character)throw std::runtime_error("Invalid dialogue character identity");
        return PS::HostServices::ProgressDirectory()/"dialogue"/(character+".json");
    }
    DialogueSave::State SavedDialogue(Quests::Service& quests,UObject* controller,const std::string& character,const std::string& flag) {
        const auto mod=flag.substr(0,flag.find(':'));
        const auto key=DialogueSave::Key(mod);
        if(!quests.HasAsset(key))quests.Prepare(controller);
        return DialogueSave::State(controller,quests.Asset(key),mod,DialogueProgressPath(character),character);
    }
    struct DialogueInventory {
        UObject* Inventory;
        UObject* Item;
        static constexpr const TCHAR* CountPath=TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData");
        static constexpr const TCHAR* ReadyPath=TEXT("/Script/Dominion.InventoryComponent:CanAddItemByData");
        static constexpr const TCHAR* GivePath=TEXT("/Script/Dominion.InventoryComponent:AddItemByData");
        DialogueInventory(UObject* controller,UObject* item):Inventory(nullptr),Item(item) {
            if(!controller || !controller->GetClassPrivate() || !Item)throw std::runtime_error("Quest inventory owner or item is unavailable");
            auto* ref=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(controller->GetClassPrivate(),TEXT("InventoryComponent")));
            if(!ref || ref->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Player inventory reference unavailable");
            Inventory=ref->GetObjectPropertyValue(ref->ContainerPtrToValuePtr<void>(controller));
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));
            if(!Inventory || !type || !Inventory->IsA(type) || Inventory->GetOuterPrivate()!=controller || Inventory->GetWorld()!=controller->GetWorld())
                throw std::runtime_error("Player inventory is not owned by the current controller");
            Validate(CountPath,12,{{TEXT("ItemData"),0,8},{TEXT("ReturnValue"),8,4}});
            Validate(ReadyPath,13,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("ReturnValue"),12,1}});
            Validate(GivePath,49,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("DurabilityPercentage"),12,4},{TEXT("GameplayTags"),16,32},{TEXT("ReturnValue"),48,1}});
        }
        struct Field {const TCHAR* Name;int Offset,Size;};
        void Validate(const TCHAR* path,int size,std::initializer_list<Field> expected) const {
            auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path);
            if(!fn || fn->GetParmsSize()!=size)throw std::runtime_error("Native inventory function layout changed");
            size_t count=0;for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
            if(count!=expected.size())throw std::runtime_error("Native inventory parameter count changed");
            for(const auto& spec:expected) {
                auto* field=fn->FindProperty(FName(spec.Name,FNAME_Find));
                if(!field || !field->HasAnyPropertyFlags(CPF_Parm) || field->GetArrayDim()!=1 || field->GetOffset_Internal()!=spec.Offset || field->GetElementSize()!=spec.Size)
                    throw std::runtime_error("Native inventory parameter layout changed");
                const RC::StringType name=spec.Name;
                if(name==TEXT("ItemData")) {
                    auto* object=CastField<FObjectPropertyBase>(field);
                    if(!object || !object->GetPropertyClass().Get() || !Item->IsA(object->GetPropertyClass().Get()))throw std::runtime_error("Inventory item parameter type changed");
                } else if(name==TEXT("GameplayTags")) {
                    auto* structure=CastField<FStructProperty>(field);
                    if(!structure || !structure->GetStruct().Get() || structure->GetStruct()->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTagContainer"))throw std::runtime_error("Inventory tag parameter type changed");
                } else if(name==TEXT("DurabilityPercentage")) {
                    if(!CastField<FFloatProperty>(field))throw std::runtime_error("Inventory durability parameter type changed");
                } else if(spec.Size==4) {
                    if(!CastField<FIntProperty>(field))throw std::runtime_error("Inventory count parameter type changed");
                } else {
                    auto* boolean=CastField<FBoolProperty>(field);
                    if(!boolean || !boolean->IsNativeBool())throw std::runtime_error("Inventory result parameter type changed");
                }
                if(name==TEXT("ReturnValue") && fn->GetReturnProperty()!=field)throw std::runtime_error("Inventory return metadata changed");
            }
        }
        int32_t Count() const {
            ActorHelper::FunctionCall call(Inventory,CountPath);call.Arg(TEXT("ItemData"),Item).Invoke();
            const auto count=call.Result<int32_t>();
            if(count<0)throw std::runtime_error("Invalid inventory count");
            return count;
        }
        bool Ready(int32_t count) const {
            ActorHelper::FunctionCall call(Inventory,ReadyPath);call.Arg(TEXT("ItemData"),Item).Arg(TEXT("Count"),count).Invoke();return call.Result<bool>();
        }
        template<class Current>
        bool Take(int32_t count,Current current) const {
            const auto* path=TEXT("/Script/Dominion.InventoryComponent:RemoveItemByData");
            Validate(path,13,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("ReturnValue"),12,1}});
            if(!current())throw std::runtime_error("World changed before quest item removal");
            const auto before=Count();
            if(before<count)return false;
            ActorHelper::FunctionCall call(Inventory,path);call.Arg(TEXT("ItemData"),Item).Arg(TEXT("Count"),count).Invoke();
            if(!current())throw std::runtime_error("World changed after quest item removal; receipt remains pending");
            const auto after=Count();
            if(!call.Result<bool>() || int64_t(before)-after!=count)throw std::runtime_error("Quest item removal could not be confirmed; receipt remains pending");
            return true;
        }
        template<class Current>
        bool Give(int32_t count,Current current) const {
            const auto before=Count();
            if(!current())throw std::runtime_error("World changed before reward delivery");
            auto* tagType=UECustom::UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr,nullptr,TEXT("/Script/GameplayTags.GameplayTagContainer"));
            if(!tagType)throw std::runtime_error("Inventory tag container unavailable");
            FManagedStruct tags(tagType);
            struct EmptyTags {uint8_t Bytes[32];} empty{};
            std::memcpy(&empty,tags.GetData(),sizeof(empty));
            ActorHelper::FunctionCall call(Inventory,GivePath);
            call.Arg(TEXT("ItemData"),Item).Arg(TEXT("Count"),count).Arg(TEXT("DurabilityPercentage"),1.0f).Arg(TEXT("GameplayTags"),empty).Invoke();
            const bool success=call.Result<bool>();
            if(!current())throw std::runtime_error("World changed after reward delivery; receipt remains pending");
            const auto after=Count();
            if(!success && after==before)return false;
            if(!success || int64_t(after)-before!=count)throw std::runtime_error("Native item grant could not be confirmed; receipt remains pending");
            return true;
        }
    };
    void DialogueField(UObject* object,const TCHAR* name,const nlohmann::json& value) {
        auto* field=PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name);
        if(!field)throw std::runtime_error("Dialogue field unavailable: "+RC::to_string(name));
        HumanValidateValue(field,value);
        PropertyHelper::CopyJsonValueToContainer(object,field,value);
    }
    UObject* DialogueObject(UClass* type,UObject* outer,const std::string& name) {
        if(!type || !outer || ActorHelper::IsAbstract(type))throw std::runtime_error("Dialogue object class unavailable or abstract");
        FStaticConstructObjectParameters params(type,outer);
        params.Name=FName(RC::to_generic_string(name),FNAME_Add);
        params.SetFlags=RF_Transient;
        auto* object=UObjectGlobals::StaticConstructObject<UObject*>(params);
        if(!object)throw std::runtime_error("Dialogue object construction failed");
        return object;
    }
}

void DragonWildsNpcLoader::LoadDialogues(const nlohmann::json& data,const RC::StringType& mod) {
    auto pending=m_dialogues;
    const auto add=[&](const nlohmann::json& entry) {
        auto definition=Dialogue::Parse(RC::to_string(mod),entry);
        const auto key=definition.Key;
        if(pending.size()>=256)throw std::runtime_error("Dialogue definition limit exceeded");
        if(!pending.emplace(key,std::move(definition)).second)throw std::runtime_error("Duplicate dialogue: "+key);
    };
    if(data.is_array())for(const auto& entry:data)add(entry);else add(data);
    m_quests.EnsureDialogueState(RC::to_string(mod));
    m_dialogues=std::move(pending);
}

void DragonWildsNpcLoader::PlayDialogueCue(AActor* actor,const nlohmann::json& cue) {
    if(!actor || cue.empty())return;
    if(cue.contains("VisualEffect")) {
        ApplyDialogueVisualEffect(actor,cue.at("VisualEffect"));
        PublishDialogueCue(actor,cue);
        return;
    }
    const bool emote=cue.contains("Emote");
    const auto pose=HumanPose::Parse(cue.at(emote?"Emote":"Pose"));
    if(emote && pose.Mode!=HumanPose::Playback::Once)throw std::runtime_error("Dialogue Emote requires Playback Once");
    UObject* mesh=nullptr;
    AActor* previewActor=nullptr;
    if(auto* childType=ActorHelper::ResolveClass(TEXT("/Script/Engine.ChildActorComponent")))
        for(auto* child:actor->GetComponentsByClass(childType))if(auto* preview=ActorHelper::GetObjectRef(child,TEXT("ChildActor"))) {
            if(auto* candidate=ActorHelper::GetObjectRef(preview,TEXT("InvisMeshComponent"))){mesh=candidate;previewActor=static_cast<AActor*>(preview);break;}
        }
    if(!mesh)mesh=FindMeshComponent(actor);
    if(!mesh)throw std::runtime_error("Dialogue animation mesh is unavailable");
    auto* animation=ActorHelper::ResolveObject(RC::to_generic_string(pose.Path));
    auto* animationClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.AnimSequence"));
    auto* skeletal=ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMeshComponent"));
    if(!animation || !animationClass || !animation->IsA(animationClass) || !skeletal || !mesh->IsA(skeletal))
        throw std::runtime_error("Dialogue Pose/Emote requires a loaded AnimSequence and skeletal mesh");
    auto* sourceMesh=ActorHelper::GetObjectRef(mesh,TEXT("SkeletalMesh"));
    if(!sourceMesh || ActorHelper::GetObjectRef(sourceMesh,TEXT("Skeleton"))!=ActorHelper::GetObjectRef(animation,TEXT("Skeleton")))
        throw std::runtime_error("Dialogue Pose/Emote skeleton does not match the NPC");
    const auto setWeaponVisibility=[&](bool hidden) {
        auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
        if(!componentClass)return;
        auto* componentOwner=previewActor?previewActor:actor;
        for(auto* component:componentOwner->GetComponentsByClass(componentClass))for(const auto* tagName:{"RuneSchema.Human.MainHand","RuneSchema.Human.OffHand"}) {
            ActorHelper::FunctionCall tagged(component,TEXT("/Script/Engine.ActorComponent:ComponentHasTag"));
            tagged.Arg(TEXT("Tag"),FName(RC::to_generic_string(tagName),FNAME_Add)).Invoke();
            if(!tagged.Result<bool>())continue;
            ActorHelper::FunctionCall visibility(component,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"));
            visibility.Arg(TEXT("NewHidden"),hidden).Arg(TEXT("bPropagateToChildren"),false).Invoke();
        }
    };
    setWeaponVisibility(HumanPose::HideWeapon(pose));
    ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:PlayAnimation"))
        .Arg(TEXT("NewAnimToPlay"),animation).Arg(TEXT("bLooping"),pose.Mode==HumanPose::Playback::Loop).Invoke();
    if(pose.Mode==HumanPose::Playback::Hold) {
        auto* duration=CastField<FFloatProperty>(PropertyHelper::GetPropertyByName(animation->GetClassPrivate(),TEXT("SequenceLength")));
        if(!duration)throw std::runtime_error("Dialogue pose duration is unavailable");
        HumanPose::ValidateTime(pose.Time,*duration->ContainerPtrToValuePtr<float>(animation));
        ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:SetPosition"))
            .Arg(TEXT("InPos"),pose.Time).Arg(TEXT("bFireNotifies"),false).Invoke();
        ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:Stop")).Invoke();
    }
    if(emote) {
        auto* duration=CastField<FFloatProperty>(PropertyHelper::GetPropertyByName(animation->GetClassPrivate(),TEXT("SequenceLength")));
        if(!duration || duration->GetElementSize()!=sizeof(float))throw std::runtime_error("Dialogue emote duration is unavailable");
        const auto seconds=*duration->ContainerPtrToValuePtr<float>(animation);
        if(!std::isfinite(seconds) || seconds<=0)throw std::runtime_error("Dialogue emote duration is invalid");
        nlohmann::json restore;
        const auto binding=std::find_if(m_spawnedVendors.begin(),m_spawnedVendors.end(),[&](const auto& value){return value.Actor.Get()==actor;});
        if(binding!=m_spawnedVendors.end())if(const auto definition=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& value){return value.ModName+":"+value.Id==binding->Key;});definition!=m_definitions.end()) {
            const auto activeDialogue=std::find_if(m_dialogueSessions.begin(),m_dialogueSessions.end(),[&](const auto& value){return value.second && value.second->Npc==actor && value.second->DialoguePoseApplied;});
            const auto& base=activeDialogue!=m_dialogueSessions.end() && definition->HasDialoguePose?definition->DialoguePose:definition->Pose;
            nlohmann::json selection;
            if(!base.Name.empty())selection["Preset"]=base.Name;
            else {
                selection["Asset"]=base.Path;
                selection["Playback"]=base.Mode==HumanPose::Playback::Loop?"Loop":base.Mode==HumanPose::Playback::Once?"Once":"Hold";
            }
            if(base.Mode==HumanPose::Playback::Hold)selection["Time"]=base.Time;
            selection["WeaponVisibility"]=base.WeaponVisibility;
            restore["Pose"]=std::move(selection);
        }
        if(!restore.empty() && !m_applyingNetworkDialogueCue) {
            std::erase_if(m_pendingDialoguePoseRestores,[&](const auto& value){return value.Actor.Get()==actor;});
            const auto requestedLead=cue.value("RestoreLeadSeconds",0.0);
            if(!std::isfinite(requestedLead) || requestedLead<0.0 || requestedLead>0.25)
                throw std::runtime_error("Dialogue transition RestoreLeadSeconds must be between 0 and 0.25");
            const auto lead=std::min<double>(requestedLead,std::max(0.0,double(seconds)-0.01));
            m_pendingDialoguePoseRestores.push_back({PS::WeakObject(actor),std::move(restore),double(GetTickCount64())/1000.0+seconds-lead});
        }
    }
    PublishDialogueCue(actor,cue);
}

void DragonWildsNpcLoader::PublishDialogueCue(AActor* actor,const nlohmann::json& cue) {
    if(!actor || cue.empty())return;
    ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return;
    auto* type=RequireIdentityClass();const auto components=actor->GetComponentsByClass(type);
    if(components.Num()!=1)return;
    auto payload=nlohmann::json::parse(ReadIdentity(components[0]));
    const auto identity=NpcIdentity::Decode(payload.dump());
    payload["version"]=2;payload["cueRevision"]=++m_serverDialogueCueRevisions[actor];payload["cue"]=cue;
    const auto encoded=payload.dump();if(encoded.size()>NpcIdentity::MaxPayload)throw std::runtime_error("NPC visual cue payload is too large");
    PropertyHelper::CopyJsonValueToContainer(components[0],IdentityField(type),encoded);
    if(ReadIdentity(components[0])!=encoded)throw std::runtime_error("NPC visual cue replication write failed");
    ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:FlushNetDormancy")).Invoke();
    ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:ForceNetUpdate")).Invoke();
}

void DragonWildsNpcLoader::ApplyDialogueVisualEffect(AActor* actor,const nlohmann::json& action) {
    if(!actor || !action.is_object())return;
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Niagara.NiagaraComponent"));
    if(!type)throw std::runtime_error("Dialogue Niagara component unavailable");
    const auto remove=[&] {
        for(auto* component:actor->GetComponentsByClass(type)) {
            if(!component)continue;
            ActorHelper::FunctionCall tag(component,TEXT("/Script/Engine.ActorComponent:ComponentHasTag"));
            tag.Arg(TEXT("Tag"),FName(TEXT("RuneSchemaDialogueNiagara"),FNAME_Add)).Invoke();
            if(tag.Result<bool>())NiagaraAttachment::Destroy(component);
        }
    };
    remove();
    if(action.value("Action",std::string{})=="Deactivate")return;
    if(!NiagaraAttachment::CanRenderLocally())return;
    auto effect=action.at("Effect");effect["AutoActivate"]=false;
    auto* component=NiagaraAttachment::Attach(actor,ActorHelper::GetObjectRef(actor,TEXT("RootComponent")),effect);
    if(!component)throw std::runtime_error("Dialogue Niagara visual did not spawn");
    try {
        component->SetFlags(RF_Transient);
        DialogueField(component,TEXT("ComponentTags"),nlohmann::json::array({"RuneSchemaDialogueNiagara"}));
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:Activate")).Arg(TEXT("bReset"),true).Invoke();
    } catch(...) {try {NiagaraAttachment::Destroy(component);}catch(...){}throw;}
}

void DragonWildsNpcLoader::ApplyNetworkDialogueCue(AActor* actor,const std::string& payload) {
    if(!actor || payload.empty())return;
    const auto identity=NpcIdentity::Decode(payload);if(!identity.CueRevision || identity.Cue.empty())return;
    const auto key=RC::to_string(actor->GetPathName());auto& seen=m_clientDialogueCueRevisions[key];
    if(identity.CueRevision<=seen)return;seen=identity.CueRevision;
    struct NetworkCueGuard {
        bool& Value;
        explicit NetworkCueGuard(bool& value):Value(value){Value=true;}
        ~NetworkCueGuard(){Value=false;}
    } guard(m_applyingNetworkDialogueCue);
    PlayDialogueCue(actor,identity.Cue);
}

void DragonWildsNpcLoader::PumpDialoguePoseRestores() {
    const auto now=double(GetTickCount64())/1000.0;
    std::vector<PendingDialoguePoseRestore> due;
    for(auto iterator=m_pendingDialoguePoseRestores.begin();iterator!=m_pendingDialoguePoseRestores.end();) {
        if(!iterator->Actor.Get()){iterator=m_pendingDialoguePoseRestores.erase(iterator);continue;}
        if(now<iterator->Deadline){++iterator;continue;}
        due.push_back(std::move(*iterator));
        iterator=m_pendingDialoguePoseRestores.erase(iterator);
    }
    for(auto& restore:due)if(auto* object=restore.Actor.Get())try {
        PlayDialogueCue(static_cast<AActor*>(object),restore.Cue);
    } catch(const std::exception& error) {
        WarnOnce("dialogue-pose-restore:"+RC::to_string(object->GetPathName()),PS::ToWideSafe(error.what()));
    }
}

void DragonWildsNpcLoader::NpcGoAway(const std::string& key,const std::string& reason) {
    auto found=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& value){
        return value.ModName+":"+value.Id==key;
    });
    if(found==m_definitions.end())throw std::runtime_error("NpcGoAway references a missing NPC: "+key);
    found->Enabled=false;
    auto* world=FindLoadedWorld();
    auto* actor=world?FindPersistentVendor(world,*found):nullptr;
    if(actor) {
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetLifeSpan")).Arg(TEXT("InLifespan"),0.25f).Invoke();
    }
    PS::Log<LogLevel::Normal>(STR("NPC '{}': go-away requested by {}; respawn suppressed for this session.\n"),
        RC::to_generic_string(key),RC::to_generic_string(reason));
}

void DragonWildsNpcLoader::ReleaseDialogueGraphs() {
    m_networkDialogueFingerprints.clear();
    m_dialogueSessions.clear();
    m_dialogueCompletions.clear();
    for(const auto& [key,lease]:m_dialogueGraphs) {
        auto* slot=FUObjectArray::IndexToObject(lease.Index);
        if(slot && DialogueLifecycle::Matches(lease.Graph,slot->GetUObject(),lease.Serial,slot->GetSerialNumber(),slot->IsRootSet(),slot->IsValid(false)))
            lease.Graph->ClearRootSet();
    }
    m_dialogueGraphs.clear();
}

void DragonWildsNpcLoader::RetireDialoguePlayer(UObject* player) {
    std::erase_if(m_dialogueSessions,[&](const auto& entry){return entry.second->Player==player;});
    for(auto it=m_dialogueGraphs.begin();it!=m_dialogueGraphs.end();) {
        if(it->second.PlayerToken!=player){++it;continue;}
        const auto expired=it++;
        const auto lease=expired->second;
        auto* slot=FUObjectArray::IndexToObject(lease.Index);
        if(slot && DialogueLifecycle::Matches(lease.Graph,slot->GetUObject(),lease.Serial,slot->GetSerialNumber(),slot->IsRootSet(),slot->IsValid(false)))
            lease.Graph->ClearRootSet();
        DialogueLifecycle::RetireSessions(m_dialogueGraphs,expired,m_dialogueCompletions,m_dialogueSessions);
    }
}

bool DragonWildsNpcLoader::DialogueGateAllows(const DialogueCompletionBinding& action,UObject* controller) {
    if(!action.GateTime.empty()) {
        if(!controller || !TimeOfDay::Allows(controller,TimeOfDay::Parse(action.GateTime)))return false;
    }
    if(action.GateQuest.empty())return true;
    if(!controller || !m_quests.HasAsset(action.GateQuest))return false;
    const QuestNative::Adapter native(controller,m_quests.Asset(action.GateQuest));
    native.ValidateAll();
    const auto& conditions=action.GateConditions;
    if(!native.IsInitialized())return conditions.empty() && std::find(action.GateStates.begin(),action.GateStates.end(),"NotStarted")!=action.GateStates.end();
    auto state=RC::to_string(native.StateName());
    if(const auto colon=state.rfind("::");colon!=state.npos)state=state.substr(colon+2);
    const auto phase=native.GetInt(FName(TEXT("RuneSchema.Phase"),FNAME_Add));
    const std::string visible=state=="Ungiven" && phase==0?"NotStarted":state=="Given" && phase==1?"Active":state=="Complete" && phase==4?"Completed":"";
    if(visible.empty() || std::find(action.GateStates.begin(),action.GateStates.end(),visible)==action.GateStates.end())return false;
    const auto& quest=m_quests.Find("_",action.GateQuest);
    const auto read=[&](const std::string& key){return native.GetInt(FName(RC::to_generic_string(key).c_str(),FNAME_Add));};
    if(conditions.contains("RepeatReady")) {
        const auto time=[&](const std::string& key)->int64_t {
            const auto high=read(key+"Hi");if(high<0)throw std::runtime_error("Invalid saved quest timestamp");
            return (int64_t(high)<<32)|std::bit_cast<uint32_t>(read(key+"Lo"));
        };
        const bool ready=visible=="Completed" && Quests::RepeatReady(quest.Repeat,time("RuneSchema.Completed"),Quests::ReceiptJournal::EpochNow(),time("RuneSchema.Clock"));
        if(ready!=conditions.at("RepeatReady").get<bool>())return false;
    }
    if(conditions.contains("Stage") || conditions.contains("ObjectivesComplete")) {
        bool complete=visible=="Completed";
        std::string stage;
        if(visible=="Active") {
            if(!quest.Stages.empty()) {
                if(!Quests::StageReceiptActive(native,m_quests.Document(action.GateQuest)))return false;
                const auto progress=Quests::StageProgress(native,quest,read("RuneSchema.Run"));
                const auto index=progress.ActiveStage();complete=index==quest.Stages.size();
                if(!complete)stage=quest.Stages[index].first;
            } else if(quest.Kill || quest.Acquire)complete=read(quest.ObjectiveId)>=quest.Required.Count;
        }
        if(conditions.contains("Stage") && (visible!="Active" || stage!=conditions.at("Stage").get<std::string>()))return false;
        if(conditions.contains("ObjectivesComplete") && complete!=conditions.at("ObjectivesComplete").get<bool>())return false;
    }
    return true;
}

void DragonWildsNpcLoader::OnDialogueRequirement(UnrealScriptFunctionCallableContext& context) {
    if(!context.Context || !context.RESULT_DECL || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    const auto found=m_dialogueCompletions.find(context.Context);
    if(found==m_dialogueCompletions.end() || (found->second.GateQuest.empty() && found->second.GateTime.empty()))return;
    bool allowed=false;
    try {
        for(const auto& [key,lease]:m_dialogueGraphs) {
            if(lease.Graph!=found->second.Graph)continue;
            auto* slot=lease.PlayerIndex>=0?FUObjectArray::IndexToObject(lease.PlayerIndex):nullptr;
            if(!slot || slot->GetUObject()!=lease.PlayerToken || slot->GetSerialNumber()!=lease.PlayerSerial || !slot->IsValid(false))break;
            allowed=DialogueGateAllows(found->second,ActorHelper::GetObjectRef(lease.PlayerToken,TEXT("Controller")));
            break;
        }
    }catch(const std::exception& error){ErrorOnce("dialogue-gate:"+(found->second.GateQuest.empty()?"time:"+found->second.GateTime:found->second.GateQuest),PS::ToWideSafe(error.what()));}
    if(!allowed)std::memcpy(context.RESULT_DECL,&m_dialogueHiddenResult,1);
}

UObject* DragonWildsNpcLoader::BuildDialogueGraph(const VendorDefinition& definition,bool completed,const std::string& character,UObject* controller) {
    const auto found=m_dialogues.find(definition.DialogueKey);
    if(found==m_dialogues.end())throw std::runtime_error("NPC references a missing or disabled dialogue: "+definition.DialogueKey);
    auto data=found->second.Data;
    // Build a player-specific graph containing only choices whose authoring
    // requirements are currently met. Native conversations enumerate output
    // branches before late requirement callbacks on some builds; filtering
    // here prevents a blank selectable slot from ever reaching the client.
    for(auto& [nodeId,nodeData]:data["Nodes"].items()) {
        auto& choices=nodeData["Choices"];
        nlohmann::json available=nlohmann::json::array();
        for(const auto& choice:choices) {
            DialogueCompletionBinding gate;
            if(choice.contains("WhenQuest")) {
                const auto& value=choice.at("WhenQuest");
                gate.GateQuest=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),value.at("Id"));
                gate.GateStates=value.at("States");gate.GateConditions=value;gate.GateConditions.erase("Id");gate.GateConditions.erase("States");
            }
            if(choice.contains("WhenTimeOfDay"))gate.GateTime=choice.at("WhenTimeOfDay").get<std::string>();
            if(choice.contains("RequirementUnlock")) {
                const auto& unlock=choice.at("RequirementUnlock");
                if(unlock.contains("Quest")) {
                    const auto& value=unlock.at("Quest");
                    gate.GateQuest=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),value.at("Id"));
                    gate.GateStates=value.at("States");gate.GateConditions=value;gate.GateConditions.erase("Id");gate.GateConditions.erase("States");
                }
                if(unlock.contains("TimeOfDay"))gate.GateTime=unlock.at("TimeOfDay").get<std::string>();
            }
            bool allowed=true;
            if(!gate.GateQuest.empty() || !gate.GateTime.empty())try {allowed=DialogueGateAllows(gate,controller);}
            catch(const std::exception& error) {
                allowed=false;ErrorOnce("dialogue-unlock:"+definition.DialogueKey+":"+nodeId+":"+choice.at("Id").get<std::string>(),PS::ToWideSafe(error.what()));
            }
            if(allowed)available.push_back(choice);
        }
        if(available.empty())throw std::runtime_error("Dialogue node has no available choices after RequirementUnlock evaluation: "+nodeId);
        choices=std::move(available);
    }
    const auto cacheKey=DialogueIdentity::GraphKey(definition.ModName,definition.Id,definition.DialogueKey,character,completed,data);
    if(const auto found=m_dialogueGraphs.find(cacheKey);found!=m_dialogueGraphs.end()) {
        const auto& lease=found->second;
        auto* slot=FUObjectArray::IndexToObject(lease.Index);
        if(slot && DialogueLifecycle::Matches(lease.Graph,slot->GetUObject(),lease.Serial,slot->GetSerialNumber(),slot->IsRootSet(),slot->IsValid(false)))
            return lease.Graph;
        // Retire tokens only: an expired UObject must never be dereferenced or re-rooted.
        DialogueLifecycle::RetireSessions(m_dialogueGraphs,found,m_dialogueCompletions,m_dialogueSessions);
    }
    auto* databaseClass=ActorHelper::ResolveClass(TEXT("/Script/CommonConversationRuntime.ConversationDatabase"));
    auto* entryClass=ActorHelper::ResolveClass(TEXT("/Script/CommonConversationRuntime.ConversationEntryPointNode"));
    auto* choiceClass=ActorHelper::ResolveClass(TEXT("/Script/CommonConversationRuntime.ConversationChoiceNode"));
    auto* promptClass=ActorHelper::ResolveClass(TEXT("/Game/Gameplay/Quests/QuestFlow/ConversationNodes/QFT_PromptMessage.QFT_PromptMessage_C"));
    auto* blankClass=ActorHelper::ResolveClass(TEXT("/Game/Gameplay/Quests/QuestFlow/ConversationNodes/QFT_Blank.QFT_Blank_C"));
    if(!databaseClass || !entryClass || !choiceClass || !promptClass || !blankClass)
        throw std::runtime_error("Native dialogue classes could not be loaded");
    auto* package=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Engine/Transient"));
    auto* graph=DialogueObject(databaseClass,package,"RuneSchema_Dialogue_"+std::to_string(++m_dialogueRevision));
    graph->SetRootSet();
    try {
        using Json=nlohmann::json;
        Json nodeMap=Json::array(),nodeIds=Json::array();
        std::vector<VendorIdentity::Words> ownedNodes;
        std::unordered_map<std::string,UObject*> prompts;
        std::vector<std::pair<UObject*,std::string>> completionNodes;
        std::vector<std::pair<UObject*,std::string>> questNodes;
        const auto guid=[&](const std::string& node){
            const auto words=DialogueIdentity::Node(cacheKey,node);
            return Json{{"A",words[0]},{"B",words[1]},{"C",words[2]},{"D",words[3]}};
        };
        const auto link=[&](const std::string& node){return Json::array({guid(node)});};
        const auto path=[](UObject* object){return RC::to_string(object->GetPathName());};
        const auto node=[&](UClass* type,const std::string& id) {
            const auto identity=DialogueIdentity::Node(cacheKey,id);
            if(std::find(ownedNodes.begin(),ownedNodes.end(),identity)!=ownedNodes.end())throw std::runtime_error("Dialogue node identity collision");
            ownedNodes.push_back(identity);
            auto* created=DialogueObject(type,graph,"Node_"+std::to_string(nodeMap.size()));
            DialogueField(created,TEXT("Compiled_NodeGUID"),guid(id));
            nodeMap.push_back({{"Key",guid(id)},{"Value",path(created)}});
            nodeIds.push_back(guid(id));
            // Keep partially built nodes alive through the graph's reflected map.
            DialogueField(graph,TEXT("ReachableNodeMap"),Json::array({nodeMap.back()}));
            return created;
        };
        auto* entry=node(entryClass,"entry");
        const Json entryTag={{"TagName","QuestFlow.Entry.RuneSchema"}};
        const Json speakerTag={{"TagName","QuestFlow.Participant.RuneSchema"}};
        DialogueField(entry,TEXT("EntryTag"),entryTag);
        DialogueField(entry,TEXT("OutputConnections"),link("text:"+data.at(completed && data.contains("CompletedEntry")?"CompletedEntry":"Entry").get<std::string>()));
        for(const auto& [id,text]:data.at("Nodes").items()) {
            auto* prompt=node(promptClass,"text:"+id);
            prompts.emplace(id,prompt);
            DialogueField(prompt,TEXT("SpeakerTag"),speakerTag);
            DialogueField(prompt,TEXT("Message"),text.at("Text"));
            DialogueField(prompt,TEXT("SpeakerDisplayName"),definition.DisplayName);
            Json outputs=Json::array();
            for(const auto& choice:text.at("Choices")) {
                const auto branchId="choice:"+id+":"+choice.at("Id").get<std::string>();
                auto* branch=node(blankClass,branchId);
                auto* option=node(choiceClass,branchId+":label");
                DialogueField(option,TEXT("DefaultChoiceDisplayText"),choice.at("Text"));
                ActorHelper::SetObjectRef(option,TEXT("ParentNode"),branch);
                ActorHelper::SetObjectRef(branch,TEXT("ParentNode"),prompt);
                DialogueField(branch,TEXT("SubNodes"),Json::array({path(option)}));
                if(choice.contains("Next"))DialogueField(branch,TEXT("OutputConnections"),link("text:"+choice.at("Next").get<std::string>()));
                if(choice.value("Complete",false))completionNodes.push_back({branch,choice.at("Next").get<std::string>()});
                DialogueCompletionBinding binding;binding.Graph=graph;binding.EndsDialogue=!choice.contains("Next");
                if(choice.contains("Next")) {
                    const auto& target=data.at("Nodes").at(choice.at("Next").get<std::string>());
                    if(target.contains("Emote"))binding.Cue={{"Emote",target.at("Emote")}};
                    else if(target.contains("Pose"))binding.Cue={{"Pose",target.at("Pose")}};
                }
                if(choice.contains("Quest")) {
                    const auto& action=choice.at("Quest");
                    binding.QuestKey=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),action.at("Id"));
                    binding.QuestAction=action.at("Action").get<std::string>();
                    binding.QuestEntry=action.value("EntryID",std::string{});
                    if(choice.contains("Next")) {
                        binding.SuccessText=data.at("Nodes").at(choice.at("Next").get<std::string>()).at("Text").get<std::string>();
                        questNodes.push_back({branch,choice.at("Next").get<std::string>()});
                    }
                }
                if(choice.contains("Event")) {
                    const auto& action=choice.at("Event");
                    binding.EventKey=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),action.at("Id"));
                    binding.EventAction=action.at("Action").get<std::string>();
                    if(!choice.contains("Quest") && choice.contains("Next"))questNodes.push_back({branch,choice.at("Next").get<std::string>()});
                }
                if(choice.contains("NpcGoAway"))binding.NpcGoAwayKey=Dialogue::Reference(
                    definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),choice.at("NpcGoAway"));
                if(choice.contains("VisualEffect"))binding.VisualEffectAction=choice.at("VisualEffect");
                if(choice.contains("VendorID"))binding.Store=Dialogue::Reference(
                    definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),choice.at("VendorID"));
                if(choice.contains("Quest") || choice.contains("Event") || choice.contains("NpcGoAway")
                    || choice.contains("VendorID") || choice.contains("VisualEffect") || !binding.Cue.empty() || binding.EndsDialogue)
                    m_dialogueCompletions.emplace(branch,std::move(binding));
                outputs.push_back(guid(branchId));
                if(choice.contains("WhenQuest") || choice.contains("WhenTimeOfDay") || choice.contains("RequirementUnlock")) {
                    auto& binding=m_dialogueCompletions[branch];binding.Graph=graph;
                    if(choice.contains("WhenQuest")) {
                        binding.GateQuest=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),choice.at("WhenQuest").at("Id"));
                        binding.GateStates=choice.at("WhenQuest").at("States");
                        binding.GateConditions=choice.at("WhenQuest");binding.GateConditions.erase("Id");binding.GateConditions.erase("States");
                    }
                    if(choice.contains("WhenTimeOfDay"))binding.GateTime=choice.at("WhenTimeOfDay").get<std::string>();
                    if(choice.contains("RequirementUnlock")) {
                        const auto& unlock=choice.at("RequirementUnlock");
                        if(unlock.contains("Quest")) {
                            const auto& gate=unlock.at("Quest");
                            binding.GateQuest=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),gate.at("Id"));
                            binding.GateStates=gate.at("States");binding.GateConditions=gate;binding.GateConditions.erase("Id");binding.GateConditions.erase("States");
                        }
                        if(unlock.contains("TimeOfDay"))binding.GateTime=unlock.at("TimeOfDay").get<std::string>();
                    }
                    // Native conversation builds may evaluate requirements on
                    // either the executable branch or its displayed choice
                    // node. Bind the same read-only gate to both so a locked
                    // choice is omitted before selection on every host line.
                    auto optionGate=binding;
                    optionGate.QuestKey.clear();optionGate.QuestAction.clear();optionGate.QuestEntry.clear();
                    optionGate.EventKey.clear();optionGate.EventAction.clear();optionGate.Store.clear();
                    optionGate.Flag.clear();optionGate.Item.clear();optionGate.SuccessText.clear();optionGate.Count=0;
                    optionGate.Response=nullptr;
                    if(m_dialogueRequirementReady)m_dialogueCompletions.insert_or_assign(option,std::move(optionGate));
                }
            }
            DialogueField(prompt,TEXT("OutputConnections"),outputs);
        }
        DialogueField(graph,TEXT("CompilerVersion"),2);
        for(const auto& [branch,next]:questNodes)m_dialogueCompletions.at(branch).Response=prompts.at(next);
        DialogueField(graph,TEXT("InternalNodeIds"),nodeIds);
        DialogueField(graph,TEXT("EntryTags"),Json::array({{{"EntryTag",entryTag},{"DestinationList",link("entry")},{"EntryIdentifier",""}}}));
        for(const auto& [branch,next]:completionNodes) {
            const auto& completion=data.at("Completion");
            auto binding=DialogueCompletionBinding{graph,prompts.at(next),
                Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),completion.at("Flag")),
                completion.at("Item").get<std::string>(),data.at("Nodes").at(next).at("Text").get<std::string>(),completion.at("Count").get<int32_t>()};
            if(const auto prior=m_dialogueCompletions.find(branch);prior!=m_dialogueCompletions.end()) {
                binding.GateQuest=prior->second.GateQuest;binding.GateTime=prior->second.GateTime;
                binding.GateStates=prior->second.GateStates;binding.GateConditions=prior->second.GateConditions;
            }
            m_dialogueCompletions.insert_or_assign(branch,std::move(binding));
            DialogueField(prompts.at(next),TEXT("Message"),"I have not been able to confirm our exchange yet. The cabbage and shop unlock still need to be checked.");
        }
        auto* graphSlot=FUObjectArray::IndexToObject(graph->GetInternalIndex());
        if(!graphSlot || graphSlot->GetUObject()!=graph || !graphSlot->IsValid(false) || !graphSlot->IsRootSet())
            throw std::runtime_error("New dialogue graph could not acquire a live rooted lease");
        m_dialogueGraphs.emplace(cacheKey,DialogueGraphLease{graph,graph->GetInternalIndex(),graphSlot->GetSerialNumber(),std::move(ownedNodes)});
        return graph;
    } catch(...) {
        std::erase_if(m_dialogueCompletions,[&](const auto& entry){return entry.second.Graph==graph;});
        graph->ClearRootSet();throw;
    }
}

UObject* DragonWildsNpcLoader::ConfigureDialogueParticipant(AActor* actor,const VendorDefinition& definition) {
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DomConversationParticipant"));
    if(!actor || !type)throw std::runtime_error("NPC dialogue participant class unavailable");
    auto* participant=EnsureComponent(actor,type);
    if(!participant || participant->GetOuterPrivate()!=actor)throw std::runtime_error("NPC dialogue participant owner is invalid");
    ActorHelper::FunctionCall(participant,TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
        .Arg(TEXT("ShouldReplicate"),false).Invoke();
    DialogueField(participant,TEXT("DisplayName"),definition.DisplayName);
    DialogueField(participant,TEXT("ParticipantTag"),{{"TagName","QuestFlow.Participant.RuneSchema"}});
    TrackNpcName(participant,definition);
    RegisterComponent(actor,participant,definition);
    return participant;
}

void DragonWildsNpcLoader::OpenDialogue(AActor* actor,AActor* player,const VendorDefinition& definition) {
    const auto generation=m_worldGeneration;
    if(!actor || !player || !player->GetWorld() || actor->GetWorld()!=player->GetWorld())
        throw std::runtime_error("Dialogue candidate requires an authoritative player in the NPC world");
    ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())throw std::runtime_error("Only authority can start a RuneSchema conversation");
    auto* participantClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DomConversationParticipant"));
    if(!participantClass)throw std::runtime_error("Native dialogue participant unavailable");
    auto participants=player->GetComponentsByClass(participantClass);
    if(participants.Num()!=1 || !participants[0])throw std::runtime_error("Player dialogue participant is ambiguous or missing");
    if(DialogueIsActive(participants[0]))return;
    std::erase_if(m_dialogueSessions,[&](const auto& entry){return entry.second->Player==player;});
    ActorHelper::FunctionCall playerTagCall(participants[0],TEXT("/Script/Dominion.DomConversationParticipant:GetParticipantTag"));
    playerTagCall.Invoke();
    struct NativeTag {FName TagName;};
    const auto playerTag=playerTagCall.Result<NativeTag>();
    if(playerTag.TagName==FName(TEXT("None"),FNAME_Add))throw std::runtime_error("Player conversation participant tag is empty");
    const auto character=DialogueCharacter(ActorHelper::GetObjectRef(player,TEXT("Controller")));
    bool completed=false;
    const auto& dialogue=m_dialogues.at(definition.DialogueKey).Data;
    if(dialogue.contains("Completion")) {
        const auto flag=Dialogue::Reference(definition.DialogueKey.substr(0,definition.DialogueKey.find(':')),dialogue.at("Completion").at("Flag"));
        completed=SavedDialogue(m_quests,ActorHelper::GetObjectRef(player,TEXT("Controller")),character,flag).HasFlag(flag);
    }
    auto* graph=BuildDialogueGraph(definition,completed,character,ActorHelper::GetObjectRef(player,TEXT("Controller")));
    for(auto& [key,lease]:m_dialogueGraphs)if(lease.Graph==graph) {
        lease.PlayerToken=player;lease.PlayerIndex=player->GetInternalIndex();
        auto* slot=FUObjectArray::IndexToObject(lease.PlayerIndex);
        if(!slot || slot->GetUObject()!=player || !slot->IsValid(false))throw std::runtime_error("Dialogue gate player unavailable");
        lease.PlayerSerial=slot->GetSerialNumber();
    }
    ConfigureDialogueParticipant(actor,definition);
    auto* libraryClass=ActorHelper::ResolveClass(TEXT("/Script/CommonConversationRuntime.ConversationLibrary"));
    auto* library=libraryClass?libraryClass->GetClassDefaultObject().Get():nullptr;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
        TEXT("/Script/CommonConversationRuntime.ConversationLibrary:StartConversationFromGraph"));
    if(!library || !function || function->GetParmsSize()!=72)
        throw std::runtime_error("Native dialogue startup contract differs from the captured 72-byte layout");
    const std::array<const TCHAR*,8> names{TEXT("ConversationEntryTag"),TEXT("Instigator"),TEXT("InstigatorTag"),TEXT("Target"),TEXT("TargetTag"),TEXT("Graph"),TEXT("EntryPointIdentifier"),TEXT("ReturnValue")};
    const std::array<int,8> offsets{0,8,16,24,32,40,48,64},sizes{8,8,8,8,8,8,16,8};
    int parameterCount=0;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++parameterCount;
    if(parameterCount!=8)throw std::runtime_error("Native dialogue startup parameter count changed");
    for(size_t i=0;i<names.size();++i) {
        auto* field=function->FindProperty(FName(names[i],FNAME_Find));
        if(!field || !field->HasAnyPropertyFlags(CPF_Parm) || field->GetArrayDim()!=1 || field->GetOffset_Internal()!=offsets[i] || field->GetElementSize()!=sizes[i])
            throw std::runtime_error("Native dialogue startup parameter changed: "+RC::to_string(names[i]));
        if(i==0 || i==2 || i==4) {
            auto* structure=CastField<FStructProperty>(field);
            if(!structure || !structure->GetStruct().Get() || structure->GetStruct()->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTag"))throw std::runtime_error("Native dialogue tag representation changed");
        } else if(i==6) {
            if(!CastField<FStrProperty>(field))throw std::runtime_error("Native dialogue identifier representation changed");
        } else {
            auto* ref=CastField<FObjectPropertyBase>(field);
            if(!ref || !ref->GetPropertyClass().Get())throw std::runtime_error("Native dialogue object representation changed");
            auto* value=i==1?static_cast<UObject*>(actor):i==3?static_cast<UObject*>(player):i==5?graph:nullptr;
            if(value && !value->IsA(ref->GetPropertyClass().Get()))throw std::runtime_error("Native dialogue object class mismatch");
            if(i==7 && function->GetReturnProperty()!=field)throw std::runtime_error("Native dialogue return metadata changed");
        }
    }
    const NativeTag entryTag{FName(TEXT("QuestFlow.Entry.RuneSchema"),FNAME_Add)};
    const NativeTag npcTag{FName(TEXT("QuestFlow.Participant.RuneSchema"),FNAME_Add)};
    const FString identifier(TEXT(""));
    if(generation!=m_worldGeneration)throw std::runtime_error("World changed during dialogue startup");
    ActorHelper::FunctionCall start(library,TEXT("/Script/CommonConversationRuntime.ConversationLibrary:StartConversationFromGraph"));
    start.Arg(TEXT("ConversationEntryTag"),entryTag).Arg(TEXT("Instigator"),actor)
        .Arg(TEXT("InstigatorTag"),npcTag).Arg(TEXT("Target"),player).Arg(TEXT("TargetTag"),playerTag)
        .Arg(TEXT("Graph"),graph).Arg(TEXT("EntryPointIdentifier"),identifier).Invoke();
    auto* instance=start.Result<UObject*>();
    if(generation!=m_worldGeneration)return;
    if(!instance)throw std::runtime_error("Native conversation startup returned no instance");
    auto* playerSlot=FUObjectArray::IndexToObject(player->GetInternalIndex());
    auto* instanceSlot=FUObjectArray::IndexToObject(instance->GetInternalIndex());
    if(!playerSlot || playerSlot->GetUObject()!=player || !playerSlot->IsValid(false))
        throw std::runtime_error("Dialogue player no longer has a live object slot");
    if(!instanceSlot || instanceSlot->GetUObject()!=instance || !instanceSlot->IsValid(false))
        throw std::runtime_error("Conversation instance no longer has a live object slot");
    auto session=std::make_shared<DialogueSession>();
    session->Instance=instance;session->Graph=graph;session->Player=player;session->Character=character;
    session->NpcKey=definition.ModName+":"+definition.Id;
    session->Npc=actor;session->NpcIndex=actor->GetInternalIndex();
    if(auto* npcSlot=FUObjectArray::IndexToObject(session->NpcIndex);npcSlot && npcSlot->GetUObject()==actor && npcSlot->IsValid(false))
        session->NpcSerial=npcSlot->GetSerialNumber();
    else throw std::runtime_error("Dialogue NPC no longer has a live object slot");
    session->PlayerIndex=player->GetInternalIndex();session->PlayerSerial=playerSlot->GetSerialNumber();
    session->InstanceIndex=instance->GetInternalIndex();session->InstanceSerial=instanceSlot->GetSerialNumber();
    auto activeSession=session;
    m_dialogueSessions[instance]=std::move(session);
    // Dialogue presentation is optional. Keep the NPC's authored normal pose;
    // automatic crouch/stand transition clips do not replicate reliably and
    // must never gate the authoritative conversation.
    activeSession->DialoguePoseApplied=false;
    const auto entryNode=dialogue.at(completed && dialogue.contains("CompletedEntry")?"CompletedEntry":"Entry").get<std::string>();
    const auto& entryData=dialogue.at("Nodes").at(entryNode);
    try {
        if(entryData.contains("Emote"))PlayDialogueCue(actor,{{"Emote",entryData.at("Emote")}});
        else if(entryData.contains("Pose"))PlayDialogueCue(actor,{{"Pose",entryData.at("Pose")}});
    } catch(const std::exception& error) {
        WarnOnce("dialogue-entry-cue:"+definition.DialogueKey,PS::ToWideSafe(
            (std::string("Dialogue opened without its optional pose/emote: ")+error.what()).c_str()));
    }
    RecordPhase(definition,"Dialogue.StartRequested",actor,{{"DialogueID",definition.DialogueKey}});
}

void DragonWildsNpcLoader::OnDialogueTask(UObject* source,UFunction* function,void* parameters) {
    const auto thread=m_gameThreadId.load(std::memory_order_relaxed);
    if(!thread || thread!=GetCurrentThreadId() || !source || !function || !parameters)return;
    static const FName taskName(TEXT("ExecuteTaskNode"),FNAME_Add);
    if(function->GetNamePrivate()!=taskName)return;
    if(function->GetPathName()!=TEXT("/Game/Gameplay/Quests/QuestFlow/ConversationNodes/QFT_Blank.QFT_Blank_C:ExecuteTaskNode"))return;
    const auto found=m_dialogueCompletions.find(source);
    if(found==m_dialogueCompletions.end())return;
    const auto action=found->second;
    const auto generation=m_worldGeneration;
    auto* contextField=CastField<FStructProperty>(function->FindProperty(FName(TEXT("Context"),FNAME_Find)));
    if(!contextField || contextField->GetElementSize()!=56 || contextField->GetOffset_Internal()!=0 || function->GetParmsSize()<56)return;
    auto* contextType=contextField->GetStruct().Get();
    if(!contextType || contextType->GetPathName()!=TEXT("/Script/CommonConversationRuntime.ConversationContext"))return;
    auto* active=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(contextType,TEXT("ActiveConversation")));
    auto* server=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(contextType,TEXT("bServer_PRIVATE")));
    auto* context=contextField->ContainerPtrToValuePtr<void>(parameters);
    if(!active || !server || !server->GetPropertyValue(server->ContainerPtrToValuePtr<void>(context)))return;
    auto* instance=active->GetObjectPropertyValue(active->ContainerPtrToValuePtr<void>(context));
    const auto activeSession=m_dialogueSessions.find(instance);
    if(!instance || activeSession==m_dialogueSessions.end())return;
    const auto session=activeSession->second;
    auto* instanceSlot=FUObjectArray::IndexToObject(session->InstanceIndex);
    if(!instanceSlot || instanceSlot->GetUObject()!=instance || instanceSlot->GetSerialNumber()!=session->InstanceSerial || !instanceSlot->IsValid(false))return;
    if(session->Handling || session->Graph!=action.Graph || ActorHelper::GetObjectRef(instance,TEXT("ActiveConversationGraph"))!=action.Graph)return;
    struct Guard {bool& Value;Guard(bool& value):Value(value){Value=true;}~Guard(){Value=false;}} guard(session->Handling);
    try {
        auto* player=ResolveDialoguePlayer(*session);
        if(!player)return;
        auto* participantClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DomConversationParticipant"));
        if(!participantClass)return;
        const auto participants=player->GetComponentsByClass(participantClass);
        if(participants.Num()!=1 || !participants[0] || ActorHelper::GetObjectRef(participants[0],TEXT("Auth_CurrentConversation"))!=instance)return;
        auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
        const auto character=DialogueCharacter(controller);
        if(character!=session->Character)return;
        if(!DialogueGateAllows(action,controller)) {
            if(action.Response)DialogueField(action.Response,TEXT("Message"),"That option is no longer available. Please speak to me again.");
            return;
        }
        const auto current=[&]{const auto active=m_dialogueSessions.find(instance);return generation==m_worldGeneration && active!=m_dialogueSessions.end() && active->second==session && ResolveDialoguePlayer(*session)==player;};
        const auto applyVisual=[&] {
            if(action.VisualEffectAction.empty())return;
            const nlohmann::json cue{{"VisualEffect",action.VisualEffectAction}};
            PlayDialogueCue(session->Npc,cue);
        };
        if(action.EndsDialogue) {
            session->DialoguePoseApplied=false;
            std::erase_if(m_pendingDialoguePoseRestores,[&](const auto& value){return value.Actor.Get()==session->Npc;});
        }
        if(!action.Cue.empty()) {
            auto* npcSlot=session->NpcIndex<0?nullptr:FUObjectArray::IndexToObject(session->NpcIndex);
            if(!npcSlot || npcSlot->GetUObject()!=session->Npc || npcSlot->GetSerialNumber()!=session->NpcSerial || !npcSlot->IsValid(false))
                throw std::runtime_error("Dialogue animation NPC is no longer available");
            try {PlayDialogueCue(session->Npc,action.Cue);}
            catch(const std::exception& error) {
                WarnOnce("dialogue-choice-cue:"+session->NpcKey,PS::ToWideSafe(
                    (std::string("Dialogue continued without its optional pose/emote: ")+error.what()).c_str()));
            }
        }
        if(!action.NpcGoAwayKey.empty())NpcGoAway(action.NpcGoAwayKey,"dialogue choice");
        if(!action.QuestKey.empty()) {
            auto message=RunQuestAction(action,controller,current);
            if(!current())return;
            if(!action.EventKey.empty()) {
                const QuestNative::Adapter native(controller,m_quests.Asset(action.QuestKey));
                auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
                if(action.EventAction=="Start" && state=="Given")message+="\n"+m_events.Start(action.EventKey,player);
                else if(action.EventAction=="Cancel")message+="\n"+m_events.Cancel(action.EventKey,player);
            }
            applyVisual();
            if(current() && action.Response)DialogueField(action.Response,TEXT("Message"),message);
            return;
        }
        if(!action.EventKey.empty()) {
            const auto message=action.EventAction=="Start"?m_events.Start(action.EventKey,player):m_events.Cancel(action.EventKey,player);
            if(!current())return;
            applyVisual();
            if(action.Response)DialogueField(action.Response,TEXT("Message"),message);return;
        }
        if(!action.Store.empty()) {
            const auto definition=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& d){return d.ModName+":"+d.Id==session->NpcKey;});
            if(definition==m_definitions.end() || !definition->Enabled || definition->StoreOwner!="store:"+action.Store)
                throw std::runtime_error("Dialogue shop is not bound to the active NPC");
            if(!session->ShopPending) {
                session->ShopPending=true;
                session->ShopDeadline=double(GetTickCount64())/1000.0+5.0;
                session->ShopCheck=double(GetTickCount64())/1000.0+0.15;
            }
            applyVisual();
            return;
        }
        if(action.Flag.empty()){applyVisual();return;}
        auto saved=SavedDialogue(m_quests,controller,character,action.Flag);
        if(saved.HasFlag(action.Flag)) {
            DialogueField(action.Response,TEXT("Message"),action.SuccessText);
            return;
        }
        auto* item=ActorHelper::ResolveObject(RC::to_generic_string(action.Item));
        auto* itemClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        if(!item || !itemClass || !item->IsA(itemClass))throw std::runtime_error("Dialogue reward item is unavailable");
        const DialogueInventory inventory(controller,item);
        const auto result=saved.Complete(action.Flag,
            [&]{const auto okay=inventory.Ready(action.Count);if(!current())throw std::runtime_error("Conversation changed during reward preflight");return okay;},
            [&]{const auto okay=inventory.Give(action.Count,current);if(!current())throw std::runtime_error("Conversation changed during reward delivery");return okay;});
        if(!current())return;
        const auto text=result==DialogueProgress::Result::NotReady?"Please make room in your inventory and speak to me again. Your reward has not been claimed.":
            result==DialogueProgress::Result::Uncertain?"Our previous exchange could not be confirmed. The reward is paused to prevent giving it twice.":action.SuccessText;
        DialogueField(action.Response,TEXT("Message"),text);
        if(result==DialogueProgress::Result::Granted)applyVisual();
        if(result==DialogueProgress::Result::Uncertain)
            ErrorOnce("dialogue-pending:"+character+":"+action.Flag,TEXT("Dialogue reward has an unresolved pending receipt; inspect runtime/saved/progress/dialogue before retrying."));
    } catch(const std::exception& error) {
        if(generation!=m_worldGeneration)return;
        if(!action.EventKey.empty()) {
            if(action.Response)DialogueField(action.Response,TEXT("Message"),"The encounter could not be updated safely. Please check the RuneSchema error before retrying.");
            ErrorOnce("event-action:"+action.EventKey,RC::to_generic_string(std::string("Event action failed: ")+error.what()));return;
        }
        if(!action.QuestKey.empty()) {
            if(action.Response)DialogueField(action.Response,TEXT("Message"),"I could not safely update this quest. Please check the RuneSchema error before trying again.");
            ErrorOnce("quest-action:"+action.QuestKey,RC::to_generic_string(std::string("Quest action failed: ")+error.what()));return;
        }
        if(!action.Store.empty()) {
            session->ShopPending=false;
            ErrorOnce("dialogue-shop:"+action.Store,RC::to_generic_string(std::string("Dialogue shop failed: ")+error.what()));
            return;
        }
        if(action.Response)DialogueField(action.Response,TEXT("Message"),"I could not complete our exchange. Your reward and the shop unlock need to be checked before we try again.");
        ErrorOnce("dialogue-reward:"+action.Flag,RC::to_generic_string(std::string("Dialogue reward failed: ")+error.what()));
    }
}

void DragonWildsNpcLoader::PrepareClientDialogueTask(UObject* participant,void* parameters) {
    if(!participant || !parameters || !m_clientDialogueFunction || m_preparingClientDialogue)return;
    auto* fn=m_clientDialogueFunction;
    auto* handle=CastField<FStructProperty>(fn->FindProperty(FName(TEXT("Handle"),FNAME_Find)));
    auto* graphField=CastField<FObjectPropertyBase>(fn->FindProperty(FName(TEXT("Graph"),FNAME_Find)));
    if(fn->GetParmsSize()!=24 || !handle || handle->GetOffset_Internal()!=0 || handle->GetElementSize()!=16
        || handle->GetArrayDim()!=1 || !handle->HasAnyPropertyFlags(CPF_Parm)
        || !handle->GetStruct().Get() || handle->GetStruct()->GetPathName()!=TEXT("/Script/CommonConversationRuntime.ConversationNodeHandle")
        || !graphField || graphField->GetOffset_Internal()!=16 || graphField->GetElementSize()!=8
        || graphField->GetArrayDim()!=1 || !graphField->HasAnyPropertyFlags(CPF_Parm)
        || !graphField->GetPropertyClass().Get() || graphField->GetPropertyClass()->GetPathName()!=TEXT("/Script/CommonConversationRuntime.ConversationDatabase"))return;
    UObject* supplied=nullptr;std::memcpy(&supplied,static_cast<uint8_t*>(parameters)+16,8);
    if(supplied)return;
    VendorIdentity::Words requested{};std::memcpy(requested.data(),parameters,16);
    if(requested[0]!=0x44475352)return;
    auto* outer=participant->GetOuterPrivate();
    auto* player=outer && outer->IsA<AActor>()?static_cast<AActor*>(outer):nullptr;
    if(!player || PS::Network::Detect(player).Mode!=PS::Network::Role::Client)return;
    auto* pawnClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.Pawn"));
    if(!pawnClass || !player->IsA(pawnClass))return;
    ActorHelper::FunctionCall local(player,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();
    if(!local.Result<bool>())return;
    auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
    if(!controller || !controller->IsA<AActor>())return;
    auto* message=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(participant->GetClassPrivate(),TEXT("LastMessage")));
    if(!message || message->GetElementSize()!=104 || !message->GetStruct().Get())return;
    auto* participants=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(message->GetStruct().Get(),TEXT("Participants")));
    if(!participants || participants->GetOffset_Internal()!=56 || participants->GetElementSize()!=16 || !participants->GetStruct().Get())return;
    auto* list=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(participants->GetStruct().Get(),TEXT("List")));
    auto* entry=list?CastField<FStructProperty>(list->GetInner()):nullptr;
    if(!list || list->GetOffset_Internal()!=0 || list->GetElementSize()!=sizeof(FScriptArray)
        || !entry || entry->GetElementSize()!=16 || !entry->GetStruct().Get())return;
    auto* actorField=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(entry->GetStruct().Get(),TEXT("Actor")));
    auto* tagField=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(entry->GetStruct().Get(),TEXT("ParticipantID")));
    if(!actorField || actorField->GetOffset_Internal()!=0 || actorField->GetElementSize()!=8
        || !tagField || tagField->GetOffset_Internal()!=8 || tagField->GetElementSize()!=8
        || !tagField->GetStruct().Get() || tagField->GetStruct()->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTag"))return;
    auto* values=list->ContainerPtrToValuePtr<FScriptArray>(participants->ContainerPtrToValuePtr<void>(message->ContainerPtrToValuePtr<void>(participant)));
    if(!values || values->Num()<2 || values->Num()>16 || !values->GetData())return;
    AActor* npc=nullptr;
    bool playerPresent=false;
    for(int index=0;index<values->Num();++index) {
        auto* value=static_cast<uint8_t*>(values->GetData())+index*entry->GetElementSize();
        auto* current=actorField->GetObjectPropertyValue(actorField->ContainerPtrToValuePtr<void>(value));
        if(current==player){playerPresent=true;continue;}
        FName tag;std::memcpy(&tag,tagField->ContainerPtrToValuePtr<void>(value),sizeof(tag));
        if(tag!=FName(TEXT("QuestFlow.Participant.RuneSchema"),FNAME_Add))continue;
        if(!current || !current->IsA<AActor>())return;
        if(npc)throw std::runtime_error("Client dialogue NPC is ambiguous");
        npc=static_cast<AActor*>(current);
    }
    if(!playerPresent || !npc || npc->GetWorld()!=player->GetWorld())return;
    if(!PrepareClientReplica(npc))return;
    const auto* matched=FindClientDefinition(npc);
    if(!matched || (matched->DialogueKey.empty() && matched->LockedDialogueKey.empty()))return;
    const auto definition=*matched;
    const auto character=DialogueCharacter(controller);
    const auto generation=m_worldGeneration;
    struct Guard {bool& Flag;Guard(bool& flag):Flag(flag){Flag=true;}~Guard(){Flag=false;}} guard(m_preparingClientDialogue);
    UObject* selected=nullptr;
    std::set<std::string> dialogueKeys{definition.DialogueKey,definition.LockedDialogueKey};
    dialogueKeys.erase("");
    for(const auto& dialogueKey:dialogueKeys) {
    auto candidate=definition;candidate.DialogueKey=dialogueKey;
    for(bool completed:{false,true}) {
        auto* graph=BuildDialogueGraph(candidate,completed,character,controller);
        if(generation!=m_worldGeneration)return;
        const auto found=std::find_if(m_dialogueGraphs.begin(),m_dialogueGraphs.end(),[&](const auto& entry){return entry.second.Graph==graph;});
        if(found==m_dialogueGraphs.end())return;
        auto& lease=found->second;
        lease.PlayerToken=player;
        lease.PlayerIndex=player->GetInternalIndex();
        auto* playerSlot=FUObjectArray::IndexToObject(lease.PlayerIndex);
        if(!playerSlot || playerSlot->GetUObject()!=player || !playerSlot->IsValid(false))return;
        lease.PlayerSerial=playerSlot->GetSerialNumber();
        auto* slot=FUObjectArray::IndexToObject(lease.Index);
        if(!slot || !DialogueLifecycle::Matches(graph,slot->GetUObject(),lease.Serial,slot->GetSerialNumber(),slot->IsRootSet(),slot->IsValid(false)))return;
        if(std::find(lease.Nodes.begin(),lease.Nodes.end(),requested)==lease.Nodes.end())continue;
        if(selected && selected!=graph)throw std::runtime_error("Client dialogue GUID resolves to multiple owned graphs");
        selected=graph;
    }
    }
    if(selected && generation==m_worldGeneration)std::memcpy(static_cast<uint8_t*>(parameters)+16,&selected,sizeof(selected));
}

void DragonWildsNpcLoader::SanitizeDialogueRpc(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function || !parameters || m_dialogueGraphs.empty()
        || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    const auto flags=function->GetFunctionFlags();
    if(!(flags&FUNC_Net) || !(flags&FUNC_NetClient))return;
    const auto name=RC::to_string(function->GetName());
    if(name!="ClientUpdateConversation" && name!="ClientExecuteTaskAndSideEffects")return;
    const auto network=PS::Network::Detect(source);
    if(!PS::Network::OwnsGameplay(network.Mode))return;
    const auto owned=[&](UObject* value) {
        if(!value)return false;
        return std::any_of(m_dialogueGraphs.begin(),m_dialogueGraphs.end(),
            [&](const auto& entry){return entry.second.Graph==value;});
    };
    std::function<unsigned(FProperty*,void*,unsigned)> clear;
    clear=[&](FProperty* property,void* container,unsigned depth)->unsigned {
        if(!property || !container || depth>4)return 0;
        if(auto* object=CastField<FObjectPropertyBase>(property)) {
            auto* address=object->ContainerPtrToValuePtr<void>(container);
            if(!owned(object->GetObjectPropertyValue(address)))return 0;
            UObject* empty=nullptr;std::memcpy(address,&empty,sizeof(empty));return 1;
        }
        if(auto* structure=CastField<FStructProperty>(property)) {
            auto* value=structure->ContainerPtrToValuePtr<void>(container);
            unsigned changed=0;
            for(auto* child:TFieldRange<FProperty>(structure->GetStruct().Get(),EFieldIterationFlags::Default))
                changed+=clear(child,value,depth+1);
            return changed;
        }
        if(auto* array=CastField<FArrayProperty>(property)) {
            auto* value=array->ContainerPtrToValuePtr<FScriptArray>(container);
            if(!value || value->Num()<0 || value->Num()>128 || (value->Num()&&!value->GetData()))
                throw std::runtime_error("Dialogue RPC array layout is invalid");
            unsigned changed=0;
            for(int32_t index=0;index<value->Num();++index)
                changed+=clear(array->GetInner(),static_cast<uint8_t*>(value->GetData())
                    +index*array->GetInner()->GetElementSize(),depth+1);
            return changed;
        }
        return 0;
    };
    unsigned changed=0;
    for(auto* property:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
        if(property->HasAnyPropertyFlags(CPF_Parm) && !property->HasAnyPropertyFlags(CPF_ReturnParm))
            changed+=clear(property,parameters,0);
    // Expected transport hygiene; intentionally silent. Failures still surface
    // through the client-dialogue-graph error path.
}

AActor* DragonWildsNpcLoader::ResolveDialoguePlayer(const DialogueSession& session) {
    if(session.PlayerIndex<0 || !session.Player)return nullptr;
    auto* slot=FUObjectArray::IndexToObject(session.PlayerIndex);
    if(!slot || slot->GetUObject()!=session.Player || slot->GetSerialNumber()!=session.PlayerSerial || !slot->IsValid(false))return nullptr;
    auto* player=static_cast<AActor*>(session.Player);
    if(!player || !player->GetWorld())return nullptr;
    ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return nullptr;
    return DialogueCharacter(ActorHelper::GetObjectRef(player,TEXT("Controller")))==session.Character?player:nullptr;
}

void DragonWildsNpcLoader::PumpDialogueShop() {
    const auto now=double(GetTickCount64())/1000.0;
    const auto generation=m_worldGeneration;
    std::vector<std::shared_ptr<DialogueSession>> pending;
    for(const auto& [instance,session]:m_dialogueSessions)
        if(session->ShopPending && now>=session->ShopCheck)pending.push_back(session);
    for(const auto& session:pending) {
    if(generation!=m_worldGeneration)return;
    const auto stillActive=m_dialogueSessions.find(session->Instance);
    if(stillActive==m_dialogueSessions.end() || stillActive->second!=session)continue;
    session->ShopCheck=now+0.1;
    try {
        if(now>=session->ShopDeadline)throw std::runtime_error("Conversation did not close within the shop transition timeout");
        auto* player=ResolveDialoguePlayer(*session);
        if(!player)
            throw std::runtime_error("Player changed during dialogue shop transition");
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DomConversationParticipant"));
        if(!type)throw std::runtime_error("Conversation participant class unavailable");
        auto participants=player->GetComponentsByClass(type);
        if(participants.Num()!=1 || !participants[0])throw std::runtime_error("Conversation participant is ambiguous");
        const bool active=DialogueIsActive(participants[0]);
        if(generation!=m_worldGeneration)return;
        if(active)continue;
        const auto key=session->NpcKey;
        m_dialogueSessions.erase(session->Instance);
        const auto found=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& d){return d.ModName+":"+d.Id==key;});
        if(found==m_definitions.end() || !found->Enabled)throw std::runtime_error("Dialogue shop NPC is no longer enabled");
        const auto definition=*found;
        auto* actor=FindPersistentVendor(player->GetWorld(),definition);
        if(!actor)throw std::runtime_error("Dialogue shop NPC is no longer present");
        OpenNpcShop(actor,player,definition);
    } catch(const std::exception& error) {
        if(generation!=m_worldGeneration)return;
        const auto failed=m_dialogueSessions.find(session->Instance);
        if(failed!=m_dialogueSessions.end() && failed->second==session)m_dialogueSessions.erase(failed);
        ErrorOnce("dialogue-shop-transition",RC::to_generic_string(std::string("Dialogue shop transition failed: ")+error.what()));
    }
    }
}
