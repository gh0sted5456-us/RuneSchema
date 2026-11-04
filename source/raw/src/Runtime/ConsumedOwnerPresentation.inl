namespace {
std::string ConsumedOwnerScope(UObject* actor){
    if(!actor||actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))
        throw std::runtime_error("Consumed presentation actor is unavailable");
    const PS::WeakObjectHandle identity(actor);
    if(identity.Get()!=actor)throw std::runtime_error("Consumed presentation actor identity is unavailable");
    return std::to_string(identity.ObjectIndex)+":"+std::to_string(identity.ObjectSerialNumber);
}
std::string ConsumedCharacterGuid(UObject* pawn){
    DragonWilds::ActorHelper::FunctionCall owner(pawn,TEXT("/Script/Engine.Pawn:GetController"));owner.Invoke();
    auto* controller=owner.Result<UObject*>();
    if(!controller||controller->GetWorld()!=pawn->GetWorld()||ControllerPawn(controller)!=pawn)
        throw std::runtime_error("Consumed presentation has no current character controller");
    DragonWilds::ActorHelper::FunctionCall guid(controller,TEXT("/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid"));
    guid.Invoke();uint32_t lanes[4]{};guid.MoveResult(lanes,sizeof(lanes));
    if(!(lanes[0]|lanes[1]|lanes[2]|lanes[3]))throw std::runtime_error("Consumed presentation character GUID is unavailable");
    return std::format("{:08X}{:08X}{:08X}{:08X}",lanes[0],lanes[1],lanes[2],lanes[3]);
}
}

void RegistryBridge::ObserveConsumedOwnerPresentation(UObject* source,UFunction* function,void* parameters){
    if(!source||!function||!parameters||source==m_invokingConsumedItem||!MatchAuthorityGraph(source,function->GetFName()))return;
    const auto selected=m_authoritySelectionPaths.find(AssetPackage(RC::to_string(source->GetPathName())));
    if(selected==m_authoritySelectionPaths.end())return;
    const auto found=m_authorityActions.find(selected->second);
    if(found==m_authorityActions.end())return;
    const auto& action=found->second;
    if(action.Action!="ConsumedItemAuthority"||action.OwnerKey.empty()||function->GetFName()!=action.EntryName
        ||!ExactClass(source,action.GraphClass,found->second.GraphType))return;
    if(!IsInGameThread())throw std::runtime_error("Consumed presentation requires the native game thread");
    auto* field=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("PlayerController"),FNAME_Find)));
    DragonWilds::ReflectedContract::Frame(function);
    if(!DragonWilds::ReflectedContract::Input(field,function)||field->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Consumed presentation controller callback layout changed");
    auto* controller=field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
    auto* pawn=ControllerPawn(controller);
    if(!pawn||!ActorAuthority(pawn))return;
    const auto scope=ConsumedOwnerScope(pawn);m_consumedOwnerActors[scope].Assign(pawn);
    m_consumedOwnerLedger.NativeCompleted(scope,action.Key,PS::ConsumedOwnerPresentation::Clock::now());
    m_consumedOwnerPending=true;
}

void RegistryBridge::TickConsumedOwnerPresentations(){
    if(!m_consumedOwnerPending)return;
    if(!IsInGameThread())throw std::runtime_error("Consumed presentation delivery requires the native game thread");
    m_consumedOwnerPending=false;
    for(const auto& receipt:m_consumedOwnerLedger.TakeReady(PS::ConsumedOwnerPresentation::Clock::now())){
        try{
            const auto owner=m_consumedOwnerActors.find(receipt.Owner);
            auto* pawn=owner==m_consumedOwnerActors.end()?nullptr:owner->second.Get();
            const auto action=m_authorityActions.find(receipt.Key);
            if(!pawn||ConsumedOwnerScope(pawn)!=receipt.Owner||!ActorAuthority(pawn)||action==m_authorityActions.end()
                ||action->second.OwnerKey.empty())throw std::runtime_error("Consumed presentation owner or declaration expired");
            const auto guid=ConsumedCharacterGuid(pawn);
            auto* bridge=EnsureBridgeComponent(static_cast<AActor*>(pawn),false);
            if(!bridge)throw std::runtime_error("Consumed presentation owning-client endpoint is unavailable");
            auto* rpc=bridge->GetFunctionByNameInChain(TEXT("ClientRuneSchemaNotification"));
            if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net)||!rpc->HasAnyFunctionFlags(FUNC_NetClient)||!rpc->HasAnyFunctionFlags(FUNC_NetReliable))
                throw std::runtime_error("Consumed presentation requires the reliable owning-client notification RPC");
            const nlohmann::json payload={{"key",action->second.OwnerKey},{"connection",action->second.OwnerConnection},{"characterGuid",guid}};
            SendNotification(bridge,"consumable.owner",action->second.Key,int64_t(receipt.Id),payload.dump());
            Trace("consumable.owner",pawn,action->second.Key,"present",int64_t(receipt.Id),{},"owner_notification_sent","executed","accepted");
        }catch(const std::exception& error){
            RecordDiagnostic("consumable.owner",receipt.Key,int64_t(receipt.Id),"rejected",error.what());
        }
    }
    std::erase_if(m_consumedOwnerActors,[](const auto& value){return !value.second.Get();});
}

void RegistryBridge::ReceiveConsumedOwnerPresentation(UObject* source,UFunction* function,const std::string& key,int64_t revision,const nlohmann::json& payload){
    if(Storefront::IsDedicatedServer()||!IsInGameThread()||revision<=0||!function
        ||!function->HasAnyFunctionFlags(FUNC_Net)||!function->HasAnyFunctionFlags(FUNC_NetClient)||!function->HasAnyFunctionFlags(FUNC_NetReliable)
        ||!payload.is_object()||payload.size()!=3)throw std::runtime_error("Consumed owner notification contract is invalid");
    const auto action=m_authorityActions.find(key);
    if(action==m_authorityActions.end()||action->second.Action!="ConsumedItemAuthority"||action->second.OwnerKey.empty()
        ||payload.at("key")!=action->second.OwnerKey||payload.at("connection")!=action->second.OwnerConnection)
        throw std::runtime_error("Consumed owner notification is not locally registered");
    DragonWilds::ActorHelper::FunctionCall getOwner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    auto* pawn=getOwner.Result<UObject*>();const auto scope=ConsumedOwnerScope(pawn);
    DragonWilds::ActorHelper::FunctionCall getController(pawn,TEXT("/Script/Engine.Pawn:GetController"));getController.Invoke();
    auto* controller=getController.Result<UObject*>();
    if(!controller||!LocalController(controller)||ControllerPawn(controller)!=pawn||controller->GetWorld()!=pawn->GetWorld()
        ||payload.at("characterGuid")!=ConsumedCharacterGuid(pawn))
        throw std::runtime_error("Consumed notification does not match the local owning character");
    if(!m_consumedOwnerReplay.Accept(ConsumedOwnerScope(source),uint64_t(revision)))return;
    const auto separator=key.find(':');
    if(separator==std::string::npos)throw std::runtime_error("Consumed owner registry identity is invalid");
    DeliverPresentation(source,key.substr(0,separator),action->second.OwnerConnection,
        payload.at("characterGuid").get<std::string>(),payload.dump(),revision,pawn);
}
