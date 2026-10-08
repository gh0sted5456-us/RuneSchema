void DragonWildsNpcLoader::OnQuestInventoryChanged(UObject* source,UFunction* function) {
    if(!source || !function || m_observingAcquisition || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    const auto name=function->GetNamePrivate();
    static const FName changed(TEXT("OnInventoryChanged_BrokenItemFTUE"),FNAME_Add),
        loadout(TEXT("OnReceiveInventoryChanged"),FNAME_Add),loaded(TEXT("OnRep_HasLoadedFromSave"),FNAME_Add);
    if(name!=changed && name!=loadout && name!=loaded)return;
    const auto path=function->GetPathName();
    UObject* controller=nullptr;bool credit=true;
    if(path==TEXT("/Game/Gameplay/Character/Player/BP_PlayerController.BP_PlayerController_C:OnInventoryChanged_BrokenItemFTUE"))controller=source;
    else if(path==TEXT("/Script/Dominion.LoadoutComponent:OnReceiveInventoryChanged"))controller=source->GetOuterPrivate();
    else if(path==TEXT("/Script/Dominion.InventoryComponent:OnRep_HasLoadedFromSave")){controller=source->GetOuterPrivate();credit=false;}
    if(!controller)return;
    try {ObserveQuestInventory(controller,credit);ReconcileQuestLocations(controller);}
    catch(const std::exception& error){ErrorOnce("quest-acquisition-route",PS::ToWideSafe(error.what()));}
}

void DragonWildsNpcLoader::ObserveQuestInventory(UObject* controller,bool credit) {
    if(m_observingAcquisition || !controller || !controller->GetWorld()
        || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    std::set<std::string> items;
    m_quests.ForEachVisible([&](const auto&,const Quests::Definition& quest,const auto&){
        if(quest.Acquire)items.insert(quest.Required.Item);
        for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
            if(objective.Acquire)items.insert(objective.Required.Item);
    });
    if(items.empty())return;
    auto* controllerType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
    if(!controllerType || !controller->IsA(controllerType))return;
    ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return;
    auto* player=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
    if(!player || !player->IsA<AActor>() || player->GetWorld()!=controller->GetWorld())return;
    auto* inventory=ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"));
    if(!inventory || inventory->GetOuterPrivate()!=controller)return;
    auto* ready=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(inventory->GetClassPrivate(),TEXT("bHasLoadedFromSave")));
    if(!ready || ready->GetArrayDim()!=1)throw std::runtime_error("Inventory load-state contract unavailable");
    if(!ready->GetPropertyValue(ready->ContainerPtrToValuePtr<void>(inventory)))return;
    auto* slot=FUObjectArray::IndexToObject(controller->GetInternalIndex());
    if(!slot || slot->GetUObject()!=controller || !slot->IsValid(false) || slot->GetSerialNumber()<=0)
        throw std::runtime_error("Inventory owner serial identity unavailable");
    const auto identity=std::to_string(controller->GetInternalIndex())+":"+std::to_string(slot->GetSerialNumber());
    if(!m_acquisitionBaselines.contains(identity) && m_acquisitionBaselines.size()>=1024)
        throw std::runtime_error("Acquisition observer player limit reached");
    m_observingAcquisition=true;
    struct Guard {bool& Flag;~Guard(){Flag=false;}} guard{m_observingAcquisition};
    auto& baseline=m_acquisitionBaselines[identity];
    std::map<std::string,int> additions;
    auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
    for(const auto& path:items)try {
        auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path));
        if(!item || !itemType || !item->IsA(itemType))throw std::runtime_error("Acquisition item could not be resolved");
        const auto delta=baseline.Observe(path,DialogueInventory(controller,item).Count(),credit);
        if(delta>0)additions.emplace(path,std::min(delta,999));
    }catch(const std::exception& error){ErrorOnce("quest-acquisition-item:"+path,PS::ToWideSafe(error.what()));}
    if(additions.empty())return;
    const auto character=DialogueCharacter(controller);
    const auto progressPath=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
    const auto location=ActorHelper::GetActorLocation(static_cast<AActor*>(player));
    const std::array<double,3> position{location.X(),location.Y(),location.Z()};
    m_quests.ForEachVisible([&](const std::string& key,const Quests::Definition& quest,const nlohmann::json& document){
        if(!quest.Acquire && quest.Stages.empty())return;
        try {
            const QuestNative::Adapter native(controller,m_quests.Asset(key));
            if(!native.IsInitialized())return;
            const Quests::NativeReceipt receipt(native,quest,document,progressPath,character);
            if(receipt.Phase()!=1)return;
            auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");
            if(colon!=state.npos)state=state.substr(colon+2);
            if(state!="Given")return;
            if(!quest.Stages.empty()) {
                auto staged=Quests::StageProgress(native,quest,receipt.Run());
                const auto active=staged.ActiveStage();
                if(active==quest.Stages.size())return;
                bool changed=false;
                // All deltas in this notification belong to the original stage.
                for(const auto& objective:quest.Stages[active].second) {
                    if(!objective.Acquire || !additions.contains(objective.Required.Item))continue;
                    if(objective.Marker && objective.Marker->RadiusMeters
                        && !Quests::Stages::Area{objective.Marker->Position,*objective.Marker->RadiusMeters}.Contains(position))continue;
                    const auto counter=Quests::StageCounter(objective);
                    const int old=native.GetInt(counter);
                    if(old<0 || old>objective.Required.Count)throw std::runtime_error("Invalid saved acquisition counter");
                    const int next=old+std::min(additions.at(objective.Required.Item),objective.Required.Count-old);
                    if(next==old)continue;
                    native.SetInt(counter,next);
                    if(native.GetInt(counter)!=next)throw std::runtime_error("Acquisition credit save was not confirmed");
                    if(objective.AnnounceProgress)try {
                        m_events.Notify(controller,Quests::ObjectiveProgressText(objective,next,receipt.Run()),Events::Scope::Participant);
                    }catch(const std::exception& error){ErrorOnce("quest-progress-message:"+key+":"+objective.ObjectiveId,PS::ToWideSafe(error.what()));}
                    changed=true;
                }
                if(changed) {
                    Quests::SetStageText(native,quest,staged.ActiveStage());
                    if(staged.ActiveStage()>active)LogQuestStage(controller,key,active+1);
                    if(staged.Complete())receipt.MarkObjectiveSatisfied();
                    QueueQuestRefresh(controller,key);
                }
            } else if(additions.contains(quest.Required.Item)) {
                if(quest.Marker && quest.Marker->RadiusMeters
                    && !Quests::Stages::Area{quest.Marker->Position,*quest.Marker->RadiusMeters}.Contains(position))return;
                const FName counter(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add);
                const int old=native.GetInt(counter);
                if(old<0 || old>quest.Required.Count)throw std::runtime_error("Invalid saved acquisition counter");
                const int next=old+std::min(additions.at(quest.Required.Item),quest.Required.Count-old);
                if(next==old)return;
                native.SetInt(counter,next);
                if(native.GetInt(counter)!=next)throw std::runtime_error("Acquisition credit save was not confirmed");
                if(quest.AnnounceProgress)try {
                    m_events.Notify(controller,Quests::ObjectiveProgressText(quest,next,receipt.Run()),Events::Scope::Participant);
                }catch(const std::exception& error){ErrorOnce("quest-progress-message:"+key,PS::ToWideSafe(error.what()));}
                if(next==quest.Required.Count) {
                    receipt.MarkObjectiveSatisfied();native.SetObjective(FName(TEXT("__ready"),FNAME_Add));
                    LogQuestStage(controller,key,1);
                }
                QueueQuestRefresh(controller,key);
            }
        }catch(const std::exception& error){ErrorOnce("quest-acquisition:"+key,PS::ToWideSafe(error.what()));}
    });
}
