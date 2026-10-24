// UsableItemInterface is reached before native consumable removal. Never attach
// this to OnConsumeSuccess: rejection there would already have spent the item.
bool DragonWildsNpcLoader::OnQuestScrollUse(Hook::TCallbackIterationData<void>& dispatch,UObject* source,UFunction* function,void* parameters) {
    if(m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId()||!source||!function||!parameters)return false;
    static const FName useItem(TEXT("UseItem"),FNAME_Add);
    if(function->GetFName()!=useItem)return false;
    const auto contract=QuestScrolls::Find(RC::to_string(source->GetPathName()));if(!contract)return false;
    // An opted-in scroll replaces (never supplements) its ordinary use body.
    // The original cannot run after rejection or execute a second cooked grant.
    dispatch.PreventOriginalFunctionCall();
    FBoolProperty* result=nullptr;
    QuestProgress::Result accepted=QuestProgress::Result::NotReady;
    const auto succeeded=[&]{return accepted==QuestProgress::Result::Accepted || accepted==QuestProgress::Result::Abandoned;};
    UObject* controller=nullptr;
    try {
        ReflectedContract::Frame(function);
        result=CastField<FBoolProperty>(function->GetReturnProperty());
        if(!ReflectedContract::Output(result,function)||!result->IsNativeBool()||result->GetElementSize()!=1)
            throw std::runtime_error("Quest-scroll UseItem return contract changed; use blocked");
        result->SetPropertyValue(result->ContainerPtrToValuePtr<void>(parameters),false);
        auto* playerField=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("PlayerController"),FNAME_Find)));
        auto* inventoryField=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("Inventory"),FNAME_Find)));
        auto* slotField=CastField<FIntProperty>(function->FindProperty(FName(TEXT("SlotIndex"),FNAME_Find)));
        const auto objectInput=[&](FObjectPropertyBase* field,const TCHAR* type){return ReflectedContract::Input(field,function)&&field->GetElementSize()==sizeof(UObject*)&&field->GetPropertyClass().Get()&&field->GetPropertyClass()->GetPathName()==type;};
        if(ReflectedContract::Count(function)!=4
            ||!objectInput(playerField,TEXT("/Script/Dominion.DominionPlayerController"))
            ||!objectInput(inventoryField,TEXT("/Script/Dominion.InventoryComponent"))
            ||!ReflectedContract::Input(slotField,function)||slotField->GetElementSize()!=sizeof(int32_t))
            throw std::runtime_error("Quest-scroll UseItem input contract changed; use blocked");
        controller=playerField->GetObjectPropertyValue(playerField->ContainerPtrToValuePtr<void>(parameters));
        auto* inventory=inventoryField->GetObjectPropertyValue(inventoryField->ContainerPtrToValuePtr<void>(parameters));
        const auto slot=slotField->GetPropertyValue(slotField->ContainerPtrToValuePtr<void>(parameters));
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ConsumableItemData"));
        if(!type||!source->IsA(type)||!IsGameplayQuestController(controller)||!inventory||slot<0||slot>100000
            ||inventory->GetOuterPrivate()!=controller||ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"))!=inventory)
            throw std::runtime_error("Quest-scroll use requires the player's current main inventory");
        auto* ready=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(inventory->GetClassPrivate(),TEXT("bHasLoadedFromSave")));
        if(!ready||ready->GetArrayDim()!=1||!ready->GetPropertyValue(ready->ContainerPtrToValuePtr<void>(inventory)))throw std::runtime_error("Quest-scroll inventory is not loaded");
        auto* removes=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(inventory->GetClassPrivate(),TEXT("bAllowRemoves")));
        if(!removes||removes->GetArrayDim()!=1||!removes->GetPropertyValue(removes->ContainerPtrToValuePtr<void>(inventory)))throw std::runtime_error("Quest-scroll inventory is temporarily locked against removal");
        const auto identity=[&](UObject* object,const TCHAR* name,bool allowEmptySaveCache=false){
            auto* field=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name));
            const auto description="field='"+RC::to_string(name)+"' object='"+RC::to_string(object->GetPathName())
                +"' class='"+RC::to_string(object->GetClassPrivate()->GetPathName())+"' expected='"+contract->PersistenceId+"'";
            if(!field || field->GetElementSize()!=sizeof(FString) || !ReflectedContract::Fits(field,object->GetClassPrivate()->GetPropertiesSize()))
                throw std::runtime_error("[QUEST-SCROLL][IDENTITY] "+description+" storage=unavailable reason='reflected string contract changed'");
            const auto& value=field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(object));
            const auto& chars=value.GetCharArray();
            const auto observed=QuestScrolls::ReadIdentity(chars.GetData(),chars.Num(),chars.Max());
            const bool emptyAllowed=allowEmptySaveCache && QuestScrolls::AllowEmptySaveIdentity(
                field->HasAnyPropertyFlags(CPF_SaveGame),field->HasAnyPropertyFlags(CPF_Transient),
                field->HasAnyPropertyFlags(CPF_Net|CPF_RepNotify),true,true);
            const auto reason=QuestScrolls::IdentityMismatch(observed,contract->PersistenceId,emptyAllowed);
            if(!reason.empty())throw std::runtime_error("[QUEST-SCROLL][IDENTITY] "+description
                +" storageNum="+std::to_string(observed.StorageCount)+" capacity="+std::to_string(observed.Capacity)
                +" boundedChars="+std::to_string(observed.Value.size())+" observed='"+observed.Value+"' reason='"+reason+"'");
        };
        identity(source,TEXT("PersistenceID"));
        const auto provenance=PS::AssetProvenance::Lookup(source);
        if(provenance.is_object()&&(!provenance.value("Registered",false)||provenance.value("LastCloneDefinitionErrors",0)!=0))throw std::runtime_error("Quest-scroll item has not registered successfully");
        const auto* slotPath=TEXT("/Script/Dominion.InventoryComponent:GetItemFromSlot");
        auto* slotFunction=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,slotPath,false);
        auto* index=slotFunction?CastField<FIntProperty>(slotFunction->FindProperty(FName(TEXT("SlotIndex"),FNAME_Find))):nullptr;
        auto* slotResult=slotFunction?CastField<FObjectPropertyBase>(slotFunction->GetReturnProperty()):nullptr;
        if(ReflectedContract::Count(slotFunction)!=2||!ReflectedContract::Input(index,slotFunction)||index->GetElementSize()!=4
            ||!ReflectedContract::Output(slotResult,slotFunction)||slotResult->GetElementSize()!=sizeof(UObject*)
            ||!slotResult->GetPropertyClass().Get()||slotResult->GetPropertyClass()->GetPathName()!=TEXT("/Script/Dominion.Item"))throw std::runtime_error("Quest-scroll slot lookup contract changed");
        ActorHelper::FunctionCall getSlot(inventory,slotFunction);getSlot.Arg(TEXT("SlotIndex"),slot).Invoke();
        auto* item=getSlot.Result<UObject*>();
        if(!item||!item->IsA(slotResult->GetPropertyClass().Get())||ActorHelper::GetObjectRef(item,TEXT("ItemData"))!=source)
            throw std::runtime_error("The requested quest scroll is not in that owned inventory slot");
        // Resolve through the native Item API as well as the owned slot's live
        // pointer. The serialized save cache can legitimately be empty on new
        // items (and is not replicated to clients); never fill or rewrite it.
        const auto* dataPath=TEXT("/Script/Dominion.Item:BP_GetItemData");
        auto* dataFunction=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,dataPath,false);
        auto* dataResult=dataFunction?CastField<FObjectPropertyBase>(dataFunction->GetReturnProperty()):nullptr;
        if(!dataFunction||!dataFunction->HasAnyFunctionFlags(FUNC_Native)||ReflectedContract::Count(dataFunction)!=1
            ||!ReflectedContract::Output(dataResult,dataFunction)||dataResult->GetElementSize()!=sizeof(UObject*)
            ||!dataResult->GetPropertyClass().Get()||dataResult->GetPropertyClass()->GetPathName()!=TEXT("/Script/Dominion.ItemData"))
            throw std::runtime_error("Quest-scroll native ItemData lookup contract changed");
        ActorHelper::FunctionCall getData(item,dataFunction);getData.Invoke();
        if(getData.Result<UObject*>()!=source)throw std::runtime_error("Quest-scroll native ItemData does not match the owned slot");
        identity(item,TEXT("ItemDataPersistenceId"),true);
        auto* count=CastField<FIntProperty>(PropertyHelper::GetPropertyByName(item->GetClassPrivate(),TEXT("Count")));
        if(!count||count->GetArrayDim()!=1||count->GetPropertyValue(count->ContainerPtrToValuePtr<void>(item))<1)throw std::runtime_error("Quest scroll slot is empty");
        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>()) {
            ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
            auto* router=ActorHelper::GetObjectRef(controller,TEXT("InventoryController"));
            if(!local.Result<bool>()||!router||router->GetOuterPrivate()!=controller)throw std::runtime_error("Quest scroll requires an owned native inventory route");
            const auto* routePath=TEXT("/Script/Dominion.InventoryController:Server_UseItemFromInventory");
            auto* route=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,routePath,false);
            auto* routeInventory=route?CastField<FObjectPropertyBase>(route->FindProperty(FName(TEXT("Inventory"),FNAME_Find))):nullptr;
            auto* routeSlot=route?CastField<FIntProperty>(route->FindProperty(FName(TEXT("SlotIndex"),FNAME_Find))):nullptr;
            if(!route||!route->HasAnyFunctionFlags(FUNC_NetServer)||!route->HasAnyFunctionFlags(FUNC_Net)||route->GetReturnProperty()||ReflectedContract::Count(route)!=2)
                throw std::runtime_error("Native quest-scroll inventory RPC is unavailable");
            if(!ReflectedContract::Input(routeInventory,route)||routeInventory->GetElementSize()!=sizeof(UObject*)||!routeInventory->GetPropertyClass().Get()
                ||routeInventory->GetPropertyClass()->GetPathName()!=TEXT("/Script/Dominion.InventoryComponent")||!ReflectedContract::Input(routeSlot,route)||routeSlot->GetElementSize()!=4)
                throw std::runtime_error("Native quest-scroll inventory RPC fields changed");
            // Forward only the normal owned inventory/slot. No client-selected
            // QuestID, script function, asset path or reward travels over RPC.
            ActorHelper::FunctionCall send(router,route);send.Arg(TEXT("Inventory"),inventory).Arg(TEXT("SlotIndex"),slot).Invoke();
            return true; // Not accepted locally; authority supplies native quest updates.
        }
        const auto controllerHandle=PS::WeakObjectHandle(controller),inventoryHandle=PS::WeakObjectHandle(inventory);
        auto* pawn=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));const auto pawnHandle=PS::WeakObjectHandle(pawn);
        const auto generation=m_worldGeneration;
        const auto current=[&]{return generation==m_worldGeneration&&controllerHandle.Get()==controller&&inventoryHandle.Get()==inventory
            &&pawnHandle.Get()==pawn&&IsGameplayQuestController(controller)&&ActorHelper::GetObjectRef(controller,TEXT("Pawn"))==pawn
            &&ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"))==inventory;};
        const auto character=DialogueCharacter(controller);
        const QuestProgress::OperationGuard guard(PS::HostServices::ProgressDirectory()/"quests"/(character+".json"));
        DialogueCompletionBinding binding;binding.QuestKey=contract->QuestKey;binding.QuestAction=contract->Action;binding.QuestEntry=contract->EntryId;
        binding.SuccessText=contract->Action=="Abandon"?"Quest cancelled.":"Quest accepted.";
        const auto message=RunQuestAction(binding,controller,current,source,&accepted);
        result->SetPropertyValue(result->ContainerPtrToValuePtr<void>(parameters),succeeded());
        if((!succeeded() || contract->Action=="Abandon")&&!message.empty())try{m_events.Notify(controller,message,Events::Scope::Participant);}catch(...){}
    }catch(const std::exception& error) {
        // A post-commit presentation error must not report a second usable
        // scroll. Uncertain receipts are not replayed, refunded or rewarded.
        if(result&&ReflectedContract::Output(result,function))result->SetPropertyValue(result->ContainerPtrToValuePtr<void>(parameters),succeeded());
        WarnOnce("quest-scroll:"+contract->QuestKey,PS::ToWideSafe(error.what()));
    }catch(...) {ErrorOnceDiagnostic("quest-scroll-internal",
        TEXT("Quest-scroll activation failed; no automatic retry or refund was attempted."),
        "RS-NPC-029","Quest-scroll activation encountered an unknown exception",
        "Error","Partial",{},"QuestScroll","Application",true);}
    return true;
}
