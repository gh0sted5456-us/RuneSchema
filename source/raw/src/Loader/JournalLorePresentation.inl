bool DragonWildsJournalModLoader::OpenLoreForPlayer(UObject* controller,const std::string& reference) {
    if(!m_loreOnly || !m_initialJournalApplied || !controller || !controller->GetWorld())
        throw std::runtime_error("Lore loader or player world is not ready");
    if(reference.empty() || reference.size()>1024 || reference.find_first_of("\r\n\t")!=reference.npos)
        throw std::runtime_error("Invalid lore entry reference");
    ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
    if(!local.Result<bool>())throw std::runtime_error("Lore presentation requires the local player's controller");
    auto key=RC::to_generic_string(reference);
    // /npc LoreID is namespace-qualified during catalog resolution while the
    // lore loader retains the authored local key. The lore catalog rejects
    // duplicate local keys across mods, so the qualified suffix is the same
    // unambiguous entry rather than a weaker path lookup.
    if(!key.starts_with(TEXT("/")))if(const auto separator=key.find(TEXT(":"));separator!=key.npos)key=key.substr(separator+1);
    if(m_rejectedEntries.contains(key))throw std::runtime_error("Lore entry was rejected during loading");
    UObject* entry=nullptr;
    if(const auto found=m_entries.find(key);found!=m_entries.end())entry=found->second.Get();
    else if(key.starts_with(TEXT("/")))entry=ActorHelper::ResolveObject(key);
    auto* loreClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.JournalEntryKnowLoreData"));
    if(!entry || !loreClass || !entry->IsA(loreClass))throw std::runtime_error("Lore entry is missing or is not native lore data");
    const auto invalid=static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed);
    if(entry->HasAnyFlags(invalid))throw std::runtime_error("Lore entry is not a live loaded asset");
    auto* apiClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.LorePopupUIAPI"));
    auto* owner=apiClass?CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(apiClass,TEXT("CharacterController"))):nullptr;
    if(!owner || owner->GetArrayDim()!=1 || owner->GetElementSize()!=sizeof(UObject*)
        || !owner->GetPropertyClass().Get() || !controller->IsA(owner->GetPropertyClass().Get()))
        throw std::runtime_error("Lore UI controller ownership contract changed");
    TArray<UObject*> apis;UECustom::UObjectGlobals::GetObjectsOfClass(apiClass,apis,true,invalid);
    UObject* selected=nullptr;
    for(auto* api:apis) {
        if(!api || owner->GetObjectPropertyValue(owner->ContainerPtrToValuePtr<void>(api))!=controller)continue;
        if(api->GetWorld() && api->GetWorld()!=controller->GetWorld())continue;
        if(selected)throw std::runtime_error("Multiple lore APIs belong to the local controller");
        selected=api;
    }
    // The API is constructed after the interactable can become usable. Treat
    // that short first-use window as retryable instead of failing the NPC.
    if(!selected)return false;
    auto* initialized=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.LorePopupUIAPI:IsInitialized"),false);
    auto* result=initialized?CastField<FBoolProperty>(initialized->GetReturnProperty()):nullptr;
    size_t count=0;if(initialized)for(auto* field:TFieldRange<FProperty>(initialized,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
    if(!initialized || count!=1 || initialized->GetParmsSize()!=1 || !result || !result->IsNativeBool()
        || result->GetArrayDim()!=1 || result->GetElementSize()!=1 || result->GetOffset_Internal()!=0)
        throw std::runtime_error("Lore UI initialization signature changed");
    ActorHelper::FunctionCall ready(selected,initialized->GetPathName());ready.Invoke();
    auto* inputManagerProperty=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(apiClass,TEXT("InputManagerAPI")));
    auto* inputManager=inputManagerProperty && inputManagerProperty->GetArrayDim()==1
        && inputManagerProperty->GetElementSize()==sizeof(UObject*)
        ? inputManagerProperty->GetObjectPropertyValue(inputManagerProperty->ContainerPtrToValuePtr<void>(selected)) : nullptr;
    if(!inputManager || inputManager->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return false;
    // Current builds can report false until the first native popup request even
    // though controller ownership and InputManagerAPI are both ready. Those live
    // dependencies are the fail-closed presentation contract; keep IsInitialized
    // as a diagnostic rather than an incorrect first-use veto.
    (void)ready.Result<bool>();
    auto* open=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.LorePopupUIAPI:OpenLorePopup"),false);
    auto* input=open?CastField<FSoftObjectProperty>(open->FindProperty(FName(TEXT("Entry"),FNAME_Find))):nullptr;
    count=0;if(open)for(auto* field:TFieldRange<FProperty>(open,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
    if(!open || count!=1 || open->GetReturnProperty() || open->GetParmsSize()!=40 || !input
        || input->GetOffset_Internal()!=0 || input->GetElementSize()!=40 || input->GetArrayDim()!=1
        || !input->GetPropertyClass().Get() || !entry->IsA(input->GetPropertyClass().Get()))
        throw std::runtime_error("Lore popup entry signature changed");
    ActorHelper::FunctionCall(selected,open->GetPathName()).SoftObjectArg(TEXT("Entry"),entry).Invoke();
    return true;
}
