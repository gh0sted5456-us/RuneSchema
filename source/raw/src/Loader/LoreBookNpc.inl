void DragonWildsNpcLoader::PumpLoreVisuals(float deltaSeconds){
    if(!LoreRead||!std::isfinite(deltaSeconds)||deltaSeconds<=0||!NiagaraAttachment::CanRenderLocally())return;
    m_loreVisualElapsed+=deltaSeconds;if(m_loreVisualElapsed<0.25)return;m_loreVisualElapsed=0;
    auto* pawn=FindLocalGameplayTestPawn();if(!pawn||!pawn->GetWorld())return;
    auto* controller=ActorHelper::GetObjectRef(pawn,TEXT("Controller"));if(!controller)return;
    std::erase_if(m_loreVisuals,[](const auto& pair){return !pair.second.Actor.Get();});
    if(m_spawnedVendors.empty())return;
    // Bounded walk over our own tracked actors, never a world actor census.
    for(size_t checked=0;checked<std::min<size_t>(4,m_spawnedVendors.size());++checked){
        const auto& binding=m_spawnedVendors[m_loreVisualCursor++%m_spawnedVendors.size()];
        if(m_loreVisualFailed.contains(binding.Key))continue;
        auto* actor=static_cast<AActor*>(binding.Actor.Get());if(!actor||actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed))||actor->GetWorld()!=pawn->GetWorld())continue;
        const auto definition=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& d){return d.ModName+":"+d.Id==binding.Key;});
        if(definition==m_definitions.end()||!definition->LorePresentation.Enabled)continue;
        try{
            const auto read=LoreRead(controller,definition->LoreEntry);if(!read)continue;
            auto& applied=m_loreVisuals[binding.Key];
            if(applied.Actor.Get()!=actor){applied={};applied.Actor=PS::WeakObject(actor);}
            const auto& config=definition->LorePresentation;const auto& mesh=*read?config.ReadMesh:config.UnreadMesh;
            if(applied.Mesh!=mesh){
                auto visual=*definition;visual.VisualMeshPath=mesh;ApplyResourceVisuals(actor,visual);applied.Mesh=mesh;
            }
            if(*read||config.GlowSystem.empty()){
                if(auto* glow=applied.Glow.Get())NiagaraAttachment::Destroy(glow);applied.Glow={};
            }else if(!applied.Glow.Get()){
                const nlohmann::json effect={{"Type","Niagara"},{"System",config.GlowSystem}};
                auto* glow=NiagaraAttachment::Attach(actor,ActorHelper::GetObjectRef(actor,TEXT("RootComponent")),effect);
                if(!glow)throw std::runtime_error("Lore unread glow attachment was not confirmed");
                applied.Glow=PS::WeakObject(glow);
            }
        }catch(const std::exception& error){m_loreVisualFailed.insert(binding.Key);WarnOnce("lore-presentation:"+binding.Key,PS::ToWideSafe(error.what()));}
    }
}
