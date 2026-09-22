void DragonWildsNpcLoader::TryAutomaticQuests(UObject* controller) {
    if(m_completingAutomaticQuests || m_observingAcquisition || !controller || !controller->GetWorld()
        || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    bool enabled=false;m_quests.ForEachVisible([&](const auto&,const auto& quest,const auto&){enabled|=quest.AutomaticReward;});
    if(!enabled)return;
    ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return;
    auto* inventory=ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"));
    if(!inventory || inventory->GetOuterPrivate()!=controller)return;
    auto* loaded=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(inventory->GetClassPrivate(),TEXT("bHasLoadedFromSave")));
    if(!loaded || loaded->GetArrayDim()!=1)throw std::runtime_error("Automatic reward inventory readiness contract unavailable");
    if(!loaded->GetPropertyValue(loaded->ContainerPtrToValuePtr<void>(inventory)))return;
    auto* player=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
    if(!player || !player->IsA<AActor>() || player->GetWorld()!=controller->GetWorld())return;
    m_completingAutomaticQuests=true;
    struct Guard {bool& Flag;~Guard(){Flag=false;}} guard{m_completingAutomaticQuests};
    const auto generation=m_worldGeneration;
    const auto current=[&]{return generation==m_worldGeneration && controller->GetWorld()==player->GetWorld()
        && ActorHelper::GetObjectRef(controller,TEXT("Pawn"))==player;};
    const auto character=DialogueCharacter(controller);
    const auto legacy=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
    const auto location=ActorHelper::GetActorLocation(static_cast<AActor*>(player));
    const std::array<double,3> position{location.X(),location.Y(),location.Z()};
    const auto inArea=[&](const auto& objective){return !objective.Marker || !objective.Marker->RadiusMeters
        || Quests::Stages::Area{objective.Marker->Position,*objective.Marker->RadiusMeters}.Contains(position);};
    m_quests.ForEachVisible([&](const std::string& key,const Quests::Definition& quest,const nlohmann::json& document){
        if(!quest.AutomaticReward || !current())return;
        try {
            const QuestNative::Adapter native(controller,m_quests.Asset(key));
            if(!native.IsInitialized())return;
            const Quests::NativeReceipt receipt(native,quest,document,legacy,character);
            auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
            receipt.RecoverConfirmedExchange(state);
            if(receipt.Phase()!=1 || state!="Given")return;
            const auto count=[&](const Quests::Definition& objective){return DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(objective.Required.Item))).Count();};
            if(!quest.Stages.empty()) {
                auto staged=Quests::StageProgress(native,quest,receipt.Run());
                // Bound automatic hand-ins by the authored stage count, never a timer.
                for(size_t pass=0;pass<quest.Stages.size() && !staged.Complete();++pass) {
                    const auto active=staged.ActiveStage();
                    const auto plan=Quests::SelectHandIns(quest.Stages[active].second,
                        [&](size_t i){return staged.Count(active,i);},inArea,count);
                    const auto& amounts=plan.Amounts;
                    if(plan.Objectives.empty())return;
                    const auto exchanged=receipt.StageExchange([&]{
                        if(!current())return false;
                        for(const auto& [path,amount]:amounts)
                            if(DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(path))).Count()<amount)return false;
                        return true;
                    },[&]{
                        for(const auto& [path,amount]:amounts)
                            if(!DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(path))).Take(amount,current))return false;
                        return true;
                    },[&]{
                        for(const auto index:plan.Objectives) {
                            const auto& objective=quest.Stages[active].second[index];
                            native.SetInt(Quests::StageCounter(objective),objective.Required.Count);
                        }
                        Quests::SetStageText(native,quest,staged.ActiveStage());return current();
                    },current);
                    if(exchanged!=QuestProgress::Result::Accepted)return;
                    if(staged.ActiveStage()>active)LogQuestStage(controller,key,active+1);
                }
                if(!staged.Complete())return;
            } else if(quest.Kill || quest.Acquire) {
                if(native.GetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add))!=quest.Required.Count)return;
            } else if(!inArea(quest) || count(quest)<quest.Required.Count)return;
            receipt.MarkObjectiveSatisfied();
            const int entry=receipt.EntryIndex();
            const auto tier=Quests::SelectRewardTier(quest,entry?quest.EntryOptions.at(entry-1).Id:std::string{},receipt.Elapsed(),[&](const std::string& id){
                for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
                    if(objective.ObjectiveId==id)return native.GetInt(Quests::StageCounter(objective))==objective.Required.Count;
                return false;
            });
            const auto& reward=Quests::RewardForRun(quest,receipt.Run(),tier);
            const DialogueInventory destination(controller,ActorHelper::ResolveObject(RC::to_generic_string(reward.Item)));
            const auto completed=receipt.TurnIn([&]{return current() && destination.Ready(reward.Count);},[&]{
                if(!quest.Stages.empty() || quest.Kill || quest.Acquire)return current();
                return DialogueInventory(controller,ActorHelper::ResolveObject(RC::to_generic_string(quest.Required.Item))).Take(quest.Required.Count,current);
            },[&]{return destination.Give(reward.Count,current);},[&]{
                if(!current())return false;
                native.SetComplete(false);
                auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");
                if(colon!=state.npos)state=state.substr(colon+2);
                return current() && state=="Complete";
            },tier);
            if(completed==QuestProgress::Result::Completed)LogQuestStage(controller,key,quest.Stages.empty()?1:quest.Stages.size()+1);
        }catch(const std::exception& error){ErrorOnce("quest-auto-completion:"+key,PS::ToWideSafe(error.what()));}
    });
}
