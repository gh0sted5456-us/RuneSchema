namespace {
FObjectPropertyBase* AppearanceSubjectField(UFunction* function){
    auto* field=Parameter<FObjectPropertyBase>(function,TEXT("Subject"));
    auto* actor=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    if(!DragonWilds::ReflectedContract::Input(field,function)||field->GetElementSize()!=sizeof(UObject*)
        ||!actor||field->GetPropertyClass().Get()!=actor)throw std::runtime_error("Character appearance Subject must be an Actor input");
    return field;
}
}

void RegistryBridge::LoadCharacterAppearances(){
    std::map<std::string,PS::CharacterAppearance::Route> routes;
    const auto manifest=m_registrySnapshot.empty()?nlohmann::json{{"entries",nlohmann::json::array()}}:nlohmann::json::parse(m_registrySnapshot);
    for(const auto& entry:manifest.at("entries"))if(entry.value("kind",std::string{})=="CharacterAppearance"){
        auto route=PS::CharacterAppearance::Declaration(entry.at("owner").get<std::string>(),entry.at("id").get<std::string>(),entry.at("appearance"));
        if(entry.at("key")!=route.Key||!routes.emplace(route.Key,route).second)throw std::runtime_error("Character appearance registry route is duplicated or inconsistent");
    }
    m_characterAppearanceRoutes=std::move(routes);m_characterAppearances.Clear();m_appearanceSubjects.clear();m_appearancePeers.clear();
    m_appearanceClientSeen.clear();m_appearanceResyncAttempts=0;m_appearanceResyncPending=!m_characterAppearanceRoutes.empty();
    m_appearanceClientSubjects.clear();m_appearanceAdvertisedSubjects.clear();m_appearanceLocalBridge.Reset();
}

void RegistryBridge::HandleCharacterAppearance(UObject* source,UObject* pawn,UFunction* function,const std::string& key,const std::string& action,const std::string& payload,int64_t revision){
    if(m_characterAppearanceRoutes.empty()||!IsInGameThread()||!ActorAuthority(pawn)||!function->HasAnyFunctionFlags(FUNC_Net)
        ||!function->HasAnyFunctionFlags(FUNC_NetServer)||!function->HasAnyFunctionFlags(FUNC_NetReliable))
        throw std::runtime_error("Character appearance requires the reliable authoritative player endpoint");
    const auto peerScope=ConsumedOwnerScope(source);
    auto* notification=source->GetFunctionByNameInChain(TEXT("ClientRuneSchemaCharacterAppearance"));
    if(!notification||!notification->HasAnyFunctionFlags(FUNC_Net)||!notification->HasAnyFunctionFlags(FUNC_NetClient)||!notification->HasAnyFunctionFlags(FUNC_NetReliable))
        throw std::runtime_error("Character appearance requires the matching cooked RSNetworking trio");
    m_appearancePeers[peerScope].Component.Assign(source);
    if(action=="resync"){
        if(key!="all"&&!m_characterAppearanceRoutes.contains(key))throw std::runtime_error("Character appearance resync route is unavailable");
        m_appearancePeers[peerScope].Delivery.Seen.clear();m_appearancePending=true;
    }else{
        const auto route=m_characterAppearanceRoutes.find(key);
        if(route==m_characterAppearanceRoutes.end())throw std::runtime_error("Character appearance route is not enabled on this server");
        if(action!="publish"&&action!="reset")throw std::runtime_error("Unknown character appearance action");
        auto& delivery=m_appearancePeers[peerScope].Delivery;delivery.CheckPublication(key,revision);
        auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if(!playerType||!pawn->IsA(playerType))throw std::runtime_error("Character appearance sender is not a native player pawn");
        const auto subject=ConsumedOwnerScope(pawn),guid=ConsumedCharacterGuid(pawn);
        (void)m_characterAppearances.Put(route->second,subject,guid,payload,action=="reset");
        delivery.Published[key]=revision;
        m_appearanceSubjects[subject].Assign(pawn);m_appearancePending=true;
    }
    SendReceipt(source,"character.appearance",key,revision,true,"accepted");
}

void RegistryBridge::SetCharacterAppearanceSubject(UObject* source,UFunction* function,void* parameters){
    if(!source||!function||!parameters||m_characterAppearanceRoutes.empty()||!BridgeClassMatches<BridgeKind::Player>(source))return;
    DragonWilds::ActorHelper::FunctionCall getOwner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    auto* owner=getOwner.Result<UObject*>();if(!ActorAuthority(owner))return;
    DragonWilds::ReflectedContract::Frame(function);
    if(!IsInGameThread()||!function->HasAnyFunctionFlags(FUNC_Net)||!function->HasAnyFunctionFlags(FUNC_NetServer)
        ||!function->HasAnyFunctionFlags(FUNC_NetReliable)||DragonWilds::ReflectedContract::Count(function)!=2)
        throw std::runtime_error("Character appearance visibility RPC contract changed");
    auto* field=AppearanceSubjectField(function);auto* subject=field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
    if(!subject)return;
    auto* type=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!type||!subject->IsA(type)||subject->GetWorld()!=owner->GetWorld())throw std::runtime_error("Character appearance visibility subject is outside this player world");
    const bool present=ParameterBool(function,parameters,TEXT("Present"));
    const auto now=std::chrono::steady_clock::now();auto& window=m_requestWindows[ConsumedOwnerScope(source)+"|appearance.subject"];
    if(window.Started.time_since_epoch().count()==0||now-window.Started>=std::chrono::seconds(1)){window.Started=now;window.Count=0;}
    if(++window.Count>MaxRequestsPerSecond)throw std::runtime_error("Character appearance visibility requests exceeded their rate limit");
    const auto scope=ConsumedOwnerScope(subject);auto& peer=m_appearancePeers[ConsumedOwnerScope(source)];peer.Component.Assign(source);
    peer.Delivery.Subject(scope,present);if(present)m_appearanceSubjects[scope].Assign(subject);m_appearancePending=true;
}

void RegistryBridge::AdvertiseCharacterAppearanceSubject(UObject* subject,bool present){
    const PS::WeakObjectHandle identity(subject);
    if(identity.Get()!=subject)return;
    const auto scope=std::to_string(identity.ObjectIndex)+":"+std::to_string(identity.ObjectSerialNumber);
    if(!present){m_appearanceClientSubjects.erase(scope);m_appearanceAdvertisedSubjects.erase(scope);}
    auto* endpoint=m_appearanceLocalBridge.Get();if(!endpoint)return;
    DragonWilds::ActorHelper::FunctionCall call(endpoint,TEXT("ServerSetRuneSchemaAppearanceSubject"));
    call.Arg(TEXT("Subject"),subject).Arg(TEXT("Present"),present).Invoke();
    if(present)m_appearanceAdvertisedSubjects.insert(scope);
}

void RegistryBridge::RequestCharacterAppearances(UObject* component){
    if(!component||m_characterAppearanceRoutes.empty()||Storefront::IsDedicatedServer())return;
    DragonWilds::ActorHelper::FunctionCall getOwner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    auto* pawn=getOwner.Result<UObject*>();if(!pawn)return;
    DragonWilds::ActorHelper::FunctionCall getController(pawn,TEXT("/Script/Engine.Pawn:GetController"));getController.Invoke();
    auto* controller=getController.Result<UObject*>();if(!controller||!LocalController(controller)||ControllerPawn(controller)!=pawn){m_appearanceLocalBridge.Reset();return;}
    if(pawn->GetWorld()!=m_appearanceClientWorld.Get()){m_appearanceClientSeen.clear();m_appearanceClientWorld.Assign(pawn->GetWorld());}
    if(component!=m_appearanceLocalBridge.Get()){m_appearanceClientSeen.clear();m_appearanceAdvertisedSubjects.clear();m_appearanceLocalBridge.Assign(component);}
    std::erase_if(m_appearanceClientSubjects,[&](const auto& row){auto* subject=row.second.Get();return !subject||subject->GetWorld()!=pawn->GetWorld()
        ||subject->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed));});
    size_t advertised=0;
    for(const auto& [scope,handle]:m_appearanceClientSubjects)if(!m_appearanceAdvertisedSubjects.contains(scope))if(auto* subject=handle.Get()){
        if(advertised>=8)return;
        AdvertiseCharacterAppearanceSubject(subject,true);++advertised;m_appearanceResyncAttempts=0;
    }
    DragonWilds::ActorHelper::FunctionCall request(component,TEXT("ServerRequestRuneSchemaAction"));
    request.Arg(TEXT("Channel"),std::string("character.appearance")).Arg(TEXT("EntityId"),std::string("all"))
        .Arg(TEXT("ActionKey"),std::string("resync")).Arg(TEXT("Payload"),std::string("{}"))
        .Arg(TEXT("Revision"),++m_outboundRevision).Invoke();m_appearanceResyncPending=false;
}

void RegistryBridge::TickCharacterAppearances(){
    if(m_characterAppearanceRoutes.empty())return;
    const auto now=std::chrono::steady_clock::now();
    if(m_appearanceResyncPending&&!Storefront::IsDedicatedServer()&&now>=m_appearanceResyncAt){
        m_appearanceResyncAt=now+(m_appearanceLocalBridge.Get()?std::chrono::milliseconds(250):std::chrono::milliseconds(2000));
        if(++m_appearanceResyncAttempts>16){m_appearanceResyncPending=false;RecordDiagnostic("character.appearance","all",0,"unavailable","Character appearance snapshot retry expired; another character spawn or explicit resync can retry.");}
        else try{
            if(auto* endpoint=m_appearanceLocalBridge.Get())RequestCharacterAppearances(endpoint);
            else{
                auto* type=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));TArray<UObject*> players;
                if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,players,true);
                UObject* candidateEndpoint=nullptr;
                if(players.Num()<=256)for(auto* pawn:players)if(pawn&&pawn->IsA<AActor>()&&!pawn->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){
                    m_appearanceClientSubjects[ConsumedOwnerScope(pawn)].Assign(pawn);
                    DragonWilds::ActorHelper::FunctionCall local(pawn,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();if(!local.Result<bool>())continue;
                    auto* bridgeType=ResolveContract().Type;const auto components=static_cast<AActor*>(pawn)->GetComponentsByClass(bridgeType);
                    if(components.Num()&&!candidateEndpoint)candidateEndpoint=components[0];
                }
                if(candidateEndpoint)RequestCharacterAppearances(candidateEndpoint);
            }
        }catch(const std::exception& error){if(m_appearanceResyncAttempts==16)RecordDiagnostic("character.appearance","all",0,"unavailable",error.what());}
    }
    if(now>=m_appearanceMaintenance){
        m_appearanceMaintenance=now+std::chrono::seconds(1);
        for(auto at=m_appearanceSubjects.begin();at!=m_appearanceSubjects.end();)if(!at->second.Get()||at->second.Get()->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){m_characterAppearances.Remove(at->first);at=m_appearanceSubjects.erase(at);}else ++at;
        for(auto at=m_appearancePeers.begin();at!=m_appearancePeers.end();)if(!at->second.Component.Get()
            ||at->second.Component.Get()->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){
            m_requestWindows.erase(at->first+"|appearance.subject");at=m_appearancePeers.erase(at);
        }else ++at;
        for(auto& [_,peer]:m_appearancePeers){std::erase_if(peer.Delivery.Seen,[&](const auto& row){return !m_characterAppearances.Snapshot().contains(row.first);});std::erase_if(peer.Delivery.Known,[&](const auto& scope){return !m_appearanceSubjects.contains(scope);});}
        std::erase_if(m_appearanceClientSubjects,[](const auto& row){return !row.second.Get();});
        std::erase_if(m_appearanceAdvertisedSubjects,[&](const auto& scope){return !m_appearanceClientSubjects.contains(scope);});
        std::erase_if(m_appearanceClientSeen,[&](const auto& row){return !m_appearanceSubjects.contains(row.first.first);});
    }
    if(!m_appearancePending)return;
    size_t sent=0;m_appearancePending=false;
    for(auto& [_,peer]:m_appearancePeers){
      auto* component=peer.Component.Get();if(!component)continue;
      DragonWilds::ActorHelper::FunctionCall getOwner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();auto* owner=getOwner.Result<UObject*>();if(!owner)continue;
      DragonWilds::ActorHelper::FunctionCall local(owner,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();const auto ownScope=ConsumedOwnerScope(owner);
      for(const auto& [key,record]:m_characterAppearances.Snapshot()){
        if(!peer.Delivery.Wants(record,ownScope,local.Result<bool>()))continue;
        if(sent>=8){m_appearancePending=true;return;}
        const auto at=m_appearanceSubjects.find(record.Subject);
        auto* subject=at==m_appearanceSubjects.end()?nullptr:at->second.Get();if(!component||!subject||component->GetWorld()!=subject->GetWorld())continue;
        try{
            DragonWilds::ActorHelper::FunctionCall send(component,TEXT("ClientRuneSchemaCharacterAppearance"));
            send.Arg(TEXT("RegistryKey"),record.Key).Arg(TEXT("Subject"),subject).Arg(TEXT("CharacterGuid"),record.CharacterGuid)
                .Arg(TEXT("Payload"),record.Payload).Arg(TEXT("Revision"),record.Revision).Arg(TEXT("Removed"),record.Removed).Invoke();
            peer.Delivery.Seen[key]=record.Revision;++sent;
        }catch(const std::exception& error){peer.Delivery.Seen[key]=record.Revision;RecordDiagnostic("character.appearance",record.Key,record.Revision,"unavailable",error.what());}
      }
    }
}

void RegistryBridge::ReceiveCharacterAppearance(UObject* source,UFunction* function,void* parameters){
    if(!source||!function||!parameters||function->GetFName()!=Names().CharacterAppearance||Storefront::IsDedicatedServer()
        ||!BridgeClassMatches<BridgeKind::Player>(source))return;
    if(!IsInGameThread()||!function->HasAnyFunctionFlags(FUNC_Net)||!function->HasAnyFunctionFlags(FUNC_NetClient)
        ||!function->HasAnyFunctionFlags(FUNC_NetReliable))throw std::runtime_error("Character appearance client RPC contract is invalid");
    DragonWilds::ActorHelper::FunctionCall getOwner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    auto* owner=getOwner.Result<UObject*>();if(!owner)return;
    DragonWilds::ActorHelper::FunctionCall getController(owner,TEXT("/Script/Engine.Pawn:GetController"));getController.Invoke();
    auto* controller=getController.Result<UObject*>();if(!controller||!LocalController(controller)||ControllerPawn(controller)!=owner)return;
    DragonWilds::ReflectedContract::Frame(function);
    const auto key=ParameterString(function,parameters,TEXT("RegistryKey"),193);
    const auto route=m_characterAppearanceRoutes.find(key);if(route==m_characterAppearanceRoutes.end())return;
    auto* field=AppearanceSubjectField(function);
    if(DragonWilds::ReflectedContract::Count(function)!=6)
        throw std::runtime_error("Character appearance subject parameter changed");
    auto* subject=field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
    if(!subject){m_appearanceResyncPending=true;return;}
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType||!subject->IsA(playerType)||subject->GetWorld()!=owner->GetWorld())throw std::runtime_error("Character appearance subject is not a player in this world");
    if(owner->GetWorld()!=m_appearanceClientWorld.Get()){m_appearanceClientSeen.clear();m_appearanceClientWorld.Assign(owner->GetWorld());}
    const auto guid=PS::CharacterAppearance::Guid(ParameterString(function,parameters,TEXT("CharacterGuid"),32));
    const auto payload=PS::CharacterAppearance::Payload(route->second,ParameterString(function,parameters,TEXT("Payload"),PS::CharacterAppearance::MaximumBytes));
    const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));const bool removed=ParameterBool(function,parameters,TEXT("Removed"));
    if(revision<=0)throw std::runtime_error("Character appearance revision is invalid");
    const auto scope=ConsumedOwnerScope(subject);const auto identity=std::pair(scope,key);
    if(const auto at=m_appearanceClientSeen.find(identity);at!=m_appearanceClientSeen.end()&&at->second>=revision)return;
    m_appearanceSubjects[scope].Assign(subject);
    if(!route->second.Class.empty()){
        auto* type=DragonWilds::ActorHelper::ResolveClass(PS::ToWideSafe(route->second.Class.c_str()));
        if(!type)type=UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(UECustom::TSoftClassPtr<UObject>(UECustom::FSoftObjectPath(PS::ToWideSafe(route->second.Class.c_str()))));
        auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
        if(!type||!base||!type->IsChildOf(base)||DragonWilds::ActorHelper::IsAbstract(type))throw std::runtime_error("Character appearance consumer is unavailable");
        const auto existing=static_cast<AActor*>(subject)->GetComponentsByClass(type);UObject* component=nullptr;
        for(auto* candidate:existing)if(candidate&&!candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){component=candidate;break;}
        if(!component){
            const FTransform transform{};DragonWilds::ActorHelper::FunctionCall add(subject,TEXT("/Script/Engine.Actor:AddComponentByClass"));
            add.Arg(TEXT("Class"),type).Arg(TEXT("bManualAttachment"),true).Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),true).Invoke();component=add.Result<UObject*>();
            if(!component)throw std::runtime_error("Character appearance consumer creation failed");
            DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated")).Arg(TEXT("ShouldReplicate"),false).Invoke();
            DragonWilds::ActorHelper::FunctionCall finish(subject,TEXT("/Script/Engine.Actor:FinishAddComponent"));
            finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),true).Arg(TEXT("RelativeTransform"),transform).Invoke();
            DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled")).Arg(TEXT("bEnabled"),false).Invoke();
        }
        auto* callback=component->GetFunctionByNameInChain(PS::ToWideSafe(route->second.Function.c_str()).c_str());
        if(!callback||callback->HasAnyFunctionFlags(FUNC_Net))throw std::runtime_error("Character appearance consumer callback must be local");
        DragonWilds::ReflectedContract::Frame(callback);
        (void)AppearanceSubjectField(callback);
        if(DragonWilds::ReflectedContract::Count(callback)!=6||!Parameter<FObjectPropertyBase>(callback,TEXT("Subject"))
            ||!Parameter<FStrProperty>(callback,TEXT("RegistryKey"))||!Parameter<FStrProperty>(callback,TEXT("CharacterGuid"))
            ||!Parameter<FStrProperty>(callback,TEXT("Payload"))||!Parameter<FInt64Property>(callback,TEXT("Revision"))||!Parameter<FBoolProperty>(callback,TEXT("Removed")))
            throw std::runtime_error("Character appearance consumer callback signature changed");
        DragonWilds::ActorHelper::FunctionCall invoke(component,callback);
        invoke.Arg(TEXT("RegistryKey"),key).Arg(TEXT("Subject"),subject).Arg(TEXT("CharacterGuid"),guid).Arg(TEXT("Payload"),payload)
            .Arg(TEXT("Revision"),revision).Arg(TEXT("Removed"),removed).Invoke();
    }
    m_appearanceClientSeen[identity]=revision;
}
