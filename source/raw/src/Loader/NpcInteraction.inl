void DragonWildsNpcLoader::TrackNpcName(UObject* target,const VendorDefinition& definition) {
    if(target)m_npcNames.insert_or_assign(target->GetPathName(),NamedTarget{target,definition.HideName?std::string{}:definition.DisplayName});
}

void DragonWildsNpcLoader::OnNpcNameQuery(UObject* source,UFunction* function,void* parameters) {
    const auto thread=m_gameThreadId.load(std::memory_order_relaxed);
    if(!thread || thread!=GetCurrentThreadId() || !source || !function || !parameters || m_npcNames.empty())return;
    static const FName display(TEXT("GetDisplayName"),FNAME_Add);
    static const FName interaction(TEXT("GetInteractionName"),FNAME_Add);
    static const FName interactionDisplay(TEXT("GetInteractionDisplayName"),FNAME_Add);
    const auto name=function->GetNamePrivate();
    if(name!=display && name!=interaction && name!=interactionDisplay)return;
    const auto found=m_npcNames.find(source->GetPathName());
    if(found==m_npcNames.end() || found->second.Token!=source)return;
    auto* result=CastField<FTextProperty>(function->GetReturnProperty());
    if(!result || result->GetArrayDim()!=1 || result->GetOffset_Internal()<0
        || result->GetOffset_Internal()+result->GetSize()>function->GetParmsSize())return;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
        if(field->HasAnyPropertyFlags(CPF_Parm) && field!=result)return;
    PropertyHelper::CopyJsonValueToContainer(parameters,result,found->second.Name);
}

void DragonWildsNpcLoader::ConfigureNpcInteraction(AActor* actor,const VendorDefinition& definition) {
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InteractionComponent"));
    if(!type)throw std::runtime_error("NPC interaction class unavailable");
    for(auto* component:actor->GetComponentsByClass(type)) {
        if(!component)continue;
        if(definition.Stage==VendorPolicy::VendorStage::Visual) {
            ActorHelper::DestroyComponent(component);
        } else {
            TrackNpcName(component,definition);
        }
    }
    RecordPhase(definition,"NPC.Interaction.Ready",actor,
        {{"Interaction",definition.Stage==VendorPolicy::VendorStage::Visual?"None":!definition.LoreEntry.empty()?"Examine":definition.DialogueKey.empty()?"Trade":"Talk"},
         {"NameRouting","Owned actor/preview/component FText name getters"}});
}
