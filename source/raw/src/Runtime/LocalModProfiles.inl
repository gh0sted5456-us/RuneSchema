namespace {
struct LocalProfileFrame {
    UFunction* Function;void* Parameters;FFrame& Stack;
    void* Output(FProperty* field){
        if(auto* address=FindOutParamValueAddress(Stack,field))return address;
        return field->ContainerPtrToValuePtr<void>(Parameters);
    }
    template<class T> T* Field(const TCHAR* name,bool output){
        auto* field=CastField<T>(Function->FindProperty(FName(name,FNAME_Find)));
        const bool fits=DragonWilds::ReflectedContract::Fits(field,Function->GetParmsSize());
        if(!fits||!field->HasAnyPropertyFlags(CPF_Parm)||field->HasAnyPropertyFlags(CPF_ReturnParm)
            ||(output?(!field->HasAnyPropertyFlags(CPF_OutParm)||field->HasAnyPropertyFlags(CPF_ReferenceParm))
                       :!DragonWilds::ReflectedContract::Input(field,Function)))
            throw std::runtime_error("Local profile function parameter type or direction changed");
        return field;
    }
    FNumericProperty* Integer(const TCHAR* name,bool output){
        auto* field=Field<FNumericProperty>(name,output);
        if(!field->IsInteger()||field->GetElementSize()!=sizeof(int64_t))throw std::runtime_error("Local profile revision requires int64");
        return field;
    }
    std::string String(FStrProperty* field,size_t maximum){
        const auto& chars=field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(Parameters)).GetCharArray();
        if(chars.Num()==0)return {};
        if(chars.Num()<0||!chars.GetData()||chars.Num()>int64_t(maximum+1)||chars.GetData()[chars.Num()-1]!=0)
            throw std::runtime_error("Local profile input string is invalid or too large");
        auto value=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
        if(value.size()>maximum)throw std::runtime_error("Local profile input exceeds its UTF-8 limit");return value;
    }
    void String(FStrProperty* field,const std::string& value){DragonWilds::PropertyHelper::SetStrPropertyValueFromJsonValue(Output(field),field,value);}
    void Boolean(FBoolProperty* field,bool value){field->SetPropertyValue(Output(field),value);}
    void Integer(FNumericProperty* field,int64_t value){field->SetIntPropertyValue(Output(field),value);}
};
}

void RegistryBridge::LoadLocalProfiles(){
    std::unordered_map<std::string,std::string> routes;
    const auto manifest=m_registrySnapshot.empty()?nlohmann::json{{"entries",nlohmann::json::array()}}:nlohmann::json::parse(m_registrySnapshot);
    for(const auto& entry:manifest.at("entries")){
        if(entry.value("kind",std::string{})!="LocalProfile")continue;
        const auto path=PS::LocalModProfiles::Class(entry.at("profile"));
        const auto key=entry.at("owner").get<std::string>()+":"+entry.at("id").get<std::string>();
        if(entry.at("key")!=key)throw std::runtime_error("Local profile registry namespace changed");
        if(!routes.emplace(path,key).second)throw std::runtime_error("Local profile component class has multiple namespaces");
    }
    m_localProfiles=std::move(routes);
    if(m_localProfiles.empty()&&m_localProfileHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_localProfileHook);m_localProfileHook=Hook::ERROR_ID;}
    if(!m_localProfiles.empty()&&m_localProfileHook==Hook::ERROR_ID){
        Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("LocalModProfiles");
        m_localProfileHook=Hook::RegisterProcessLocalScriptFunctionPreCallback([this](Hook::TCallbackIterationData<void>& iteration,UObject* source,FFrame& stack,void*){
            auto* function=stack.Node();if(!function)return;const auto name=function->GetFName();
            if(name!=Names().ReadProfile&&name!=Names().WriteProfile)return;
            try{if(HandleLocalProfile(source,stack))iteration.PreventOriginalFunctionCall();}
            catch(const std::exception& error){iteration.PreventOriginalFunctionCall();RecordDiagnostic("local.profile",{},0,"rejected",error.what());}
        },options);
        if(m_localProfileHook==Hook::ERROR_ID)throw std::runtime_error("Local profile Blueprint dispatch hook is unavailable");
    }
}

bool RegistryBridge::HandleLocalProfile(UObject* source,FFrame& stack){
    auto* function=stack.Node();auto* parameters=stack.Locals();
    if(!source||!function||!source->GetClassPrivate())return false;
    const auto route=m_localProfiles.find(RC::to_string(source->GetClassPrivate()->GetPathName()));
    if(route==m_localProfiles.end())return false;
    const auto profileNamespace=route->second;
    if(!parameters||!IsInGameThread())
        throw std::runtime_error("Local profile requires a game-thread Blueprint frame");
    DragonWilds::ReflectedContract::Frame(function);
    LocalProfileFrame frame{function,parameters,stack};
    auto* success=frame.Field<FBoolProperty>(TEXT("Success"),true);
    frame.Boolean(success,false);
    auto* error=frame.Field<FStrProperty>(TEXT("Error"),true);
    frame.String(error,std::string{"Local profile function contract is invalid"});
    if(function->HasAnyFunctionFlags(FUNC_Net)||DragonWilds::ReflectedContract::Count(function)!=6)
        throw std::runtime_error("Local profile requires six local, non-RPC Blueprint parameters on the game thread");
    const bool writing=function->GetFName()==Names().WriteProfile;
    auto* player=frame.Field<FObjectPropertyBase>(TEXT("PlayerController"),false);
    auto* revision=frame.Integer(TEXT("Revision"),true);
    auto* payload=frame.Field<FStrProperty>(TEXT("Payload"),!writing);
    auto* found=writing?nullptr:frame.Field<FBoolProperty>(TEXT("Found"),true);
    auto* expected=writing?frame.Integer(TEXT("ExpectedRevision"),false):nullptr;
    if(player->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Local profile controller parameter layout changed");
    frame.Integer(revision,0);frame.String(error,std::string{});
    if(found){frame.Boolean(found,false);frame.String(payload,std::string{});}
    try{
        if(Storefront::IsDedicatedServer())throw std::runtime_error("Local player profiles are unavailable on dedicated servers");
        (void)ConsumedOwnerScope(source);
        auto* controller=player->GetObjectPropertyValue(player->ContainerPtrToValuePtr<void>(parameters));
        auto* controllerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.PlayerController"));
        auto* componentType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
        if(!componentType||!source->IsA(componentType)||!controllerType||!controller||!controller->IsA(controllerType)
            ||!player->GetPropertyClass().Get()||!player->GetPropertyClass().Get()->IsChildOf(controllerType)
            ||!controller->IsA(player->GetPropertyClass().Get())||!LocalController(controller)
            ||!controller->GetWorld()||controller->GetWorld()!=source->GetWorld())
            throw std::runtime_error("Profile requires the component's actual local PlayerController");
        DragonWilds::ActorHelper::FunctionCall getOwner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
        if(getOwner.Result<UObject*>()!=controller)throw std::runtime_error("Profile component belongs to a different controller");
        auto* pawn=ControllerPawn(controller);
        if(!pawn||pawn->GetWorld()!=controller->GetWorld())throw std::runtime_error("Profile character identity is not assigned; keep the draft unbound");
        const auto guid=ConsumedCharacterGuid(pawn);
        const auto root=PS::RuneSchemaData::Directory()/"core"/"companion";
        PS::LocalModProfiles::Profile result;
        if(writing){
            const auto value=expected->GetSignedIntPropertyValue(expected->ContainerPtrToValuePtr<void>(parameters));
            const auto bytes=frame.String(payload,PS::LocalModProfiles::MaximumPayloadBytes);
            result=PS::LocalModProfiles::Write(root,profileNamespace,guid,value,bytes);
        }else result=PS::LocalModProfiles::Read(root,profileNamespace,guid);
        frame.Integer(revision,result.Revision);
        if(found){frame.Boolean(found,result.Found);frame.String(payload,result.Payload);}
        frame.Boolean(success,true);
    }catch(const std::exception& problem){
        frame.String(error,std::string(problem.what()).substr(0,1024));
        RecordDiagnostic("local.profile",profileNamespace,0,"rejected",problem.what());
    }
    return true;
}
