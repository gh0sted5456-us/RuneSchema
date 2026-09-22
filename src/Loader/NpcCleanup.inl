void DragonWildsNpcLoader::QueueNpcCleanup(AActor* actor,const std::string& respawnDefinition) {
    if(!actor)return;
    if(std::any_of(m_pendingNpcCleanup.begin(),m_pendingNpcCleanup.end(),
        [&](const auto& item){return item.Actor==actor;}))return;
    const auto index=actor->GetInternalIndex();
    auto* slot=index>=0?FUObjectArray::IndexToObject(index):nullptr;
    if(!slot || slot->GetUObject()!=actor)throw std::runtime_error("Failed NPC cleanup identity unavailable");
    // Root only this failed, owned proxy until destruction is acknowledged. This
    // avoids serial-zero weak handles silently dropping cleanup responsibility.
    const bool root=!slot->IsRootSet();
    m_pendingNpcCleanup.push_back({actor,index,root,respawnDefinition});
    if(root)actor->SetRootSet();
}

void DragonWildsNpcLoader::PumpNpcCleanup(double deltaSeconds) {
    if(m_pendingNpcCleanup.empty()){m_npcCleanupElapsed=0;return;}
    m_npcCleanupElapsed+=deltaSeconds;
    if(m_npcCleanupElapsed<0.5)return;
    m_npcCleanupElapsed=0;
    for(auto it=m_pendingNpcCleanup.begin();it!=m_pendingNpcCleanup.end();) {
        auto* actor=it->Actor;
        auto* slot=FUObjectArray::IndexToObject(it->Index);
        if(!slot || slot->GetUObject()!=actor) {
            it=m_pendingNpcCleanup.erase(it);continue;
        }
        try {
            if(IsNpcObjectUsable(actor)) {
                // Independent attempts: a changed component contract must not
                // prevent the actor's native destruction from being attempted.
                const auto attempt=[&](const auto& operation){try{operation();}catch(...) {}};
                attempt([&]{ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorEnableCollision"))
                    .Arg(TEXT("bNewActorEnableCollision"),false).Invoke();});
                attempt([&]{ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorHiddenInGame"))
                    .Arg(TEXT("bNewHidden"),true).Invoke();});
                for(const auto* path:{TEXT("/Script/Dominion.InteractionComponent"),
                    TEXT("/Script/Engine.ChildActorComponent"),TEXT("/Script/Engine.BillboardComponent"),
                    TEXT("/Script/MinimapPlugin.MapIconComponent"),TEXT("/Script/Niagara.NiagaraComponent")}) {
                    attempt([&]{
                        auto* type=ActorHelper::ResolveClass(path);
                        if(!type)return;
                        for(auto* component:actor->GetComponentsByClass(type))
                            if(component && component->GetOuterPrivate()==actor)
                                attempt([&]{
                                    std::erase_if(m_npcNames,[&](const auto& entry){return entry.second.Token==component;});
                                    ActorHelper::DestroyComponent(component);
                                });
                    });
                }
                ActorHelper::DestroyActor(actor);
            }
            if(IsNpcObjectUsable(actor)){++it;continue;}
            std::erase_if(m_merchantBindings,[&](const auto& binding){return binding.Actor.Get()==actor;});
            std::erase_if(m_npcNames,[&](const auto& entry){return entry.second.Token==actor;});
            if(it->AddedRoot)actor->ClearRootSet();
            if(!it->RespawnDefinition.empty()) {
                const auto key=it->RespawnDefinition;
                if(auto definition=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& value){return value.ModName+":"+value.Id==key;});definition!=m_definitions.end())
                    definition->SpawnGate.ResetForMap();
                m_scanBudget.ResetForMap();
            }
            it=m_pendingNpcCleanup.erase(it);
        }catch(const std::exception& error) {
            ErrorOnce("npc-cleanup-retry:"+std::to_string(it->Index),PS::ToWideSafe(error.what()));
            ++it;
        }catch(...) {++it;}
    }
}
