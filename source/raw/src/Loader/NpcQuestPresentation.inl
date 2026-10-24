void DragonWildsNpcLoader::OnQuestTextQuery(UnrealScriptFunctionCallableContext& context,UFunction* function) {
    if(!function || !context.Context || !context.TheStack.Locals() || !context.RESULT_DECL
        || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
    try {
        const bool current=function->GetFName()==FName(TEXT("GetCurrentObjectiveText"),FNAME_Add);
        const auto expected=RC::StringType(TEXT("/Script/Dominion.QuestProgressComponent:"))+(current?TEXT("GetCurrentObjectiveText"):TEXT("GetObjectiveText"));
        if(function->GetPathName()!=expected)return;
        auto* input=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("QuestData"),FNAME_Find)));
        auto* output=CastField<FTextProperty>(function->FindProperty(FName(TEXT("ObjectiveTextResult"),FNAME_Find)));
        auto* result=CastField<FBoolProperty>(function->GetReturnProperty());
        auto* name=current?nullptr:CastField<FNameProperty>(function->FindProperty(FName(TEXT("ObjectiveName"),FNAME_Find)));
        size_t count=0;for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
        ReflectedContract::Frame(function);
        if(count!=(current?3:4)
            || !ReflectedContract::Input(input,function) || input->GetElementSize()!=sizeof(UObject*)
            || !input->HasAnyPropertyFlags(CPF_Parm) || input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)
            || !ReflectedContract::Fits(output,function->GetParmsSize()) || !ReflectedContract::Text(output)
            || !output->HasAnyPropertyFlags(CPF_OutParm) || output->HasAnyPropertyFlags(CPF_ReturnParm)
            || !ReflectedContract::Output(result,function) || !result->IsNativeBool() || result->GetElementSize()!=1
            || (!current && (!ReflectedContract::Input(name,function) || name->GetElementSize()!=sizeof(FName))))
            throw std::runtime_error("Quest objective text function layout changed");
        if(!result->GetPropertyValue(context.RESULT_DECL))return;
        auto* controller=context.Context->GetOuterPrivate();
        if(!IsGameplayQuestController(controller)
            || ActorHelper::GetObjectRef(controller,TEXT("QuestProgressComponent"))!=context.Context)return;
        ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
        if(!local.Result<bool>())return;
        UObject* asset=nullptr;std::memcpy(&asset,input->ContainerPtrToValuePtr<void>(context.TheStack.Locals()),sizeof(asset));
        const Quests::Definition* definition=nullptr;
        m_quests.ForEachVisible([&](const auto& key,const auto& quest,const auto&){
            if(!definition && m_quests.HasAsset(key) && m_quests.Asset(key)==asset)definition=&quest;
        });
        if(!definition)return;
        const QuestNative::Adapter native(controller,asset);
        if(!native.IsInitialized() || native.GetInt(FName(TEXT("RuneSchema.Phase"),FNAME_Add))!=1)return;
        std::string objective;
        if(!current)objective=RC::to_string(name->ContainerPtrToValuePtr<FName>(context.TheStack.Locals())->ToString());
        else if(definition->Stages.empty()) {
            if((definition->Kill || definition->Acquire)
                && native.GetInt(FName(RC::to_generic_string(definition->ObjectiveId).c_str(),FNAME_Add))==definition->Required.Count)return;
            objective=definition->ObjectiveId;
        }
        else {
            const int run=native.GetInt(FName(TEXT("RuneSchema.Run"),FNAME_Add));
            if(run<1)return;
            const auto stage=Quests::StageProgress(native,*definition,run).ActiveStage();
            if(stage>=definition->Stages.size())return;
            objective=definition->Stages[stage].first;
        }
        const auto text=Quests::ProgressText(*definition,objective,[&](const auto& entry){
            if(!definition->Stages.empty())return native.GetInt(Quests::StageCounter(entry));
            if(entry.Kill || entry.Acquire)return native.GetInt(FName(RC::to_generic_string(entry.ObjectiveId).c_str(),FNAME_Add));
            auto* item=ActorHelper::ResolveObject(RC::to_generic_string(entry.Required.Item));
            return std::min(DialogueInventory(controller,item).Count(),entry.Required.Count);
        });
        if(!text)return;
        void* destination=output->ContainerPtrToValuePtr<void>(context.TheStack.Locals());
        size_t seen=0,matches=0;
        for(auto* out=context.TheStack.OutParms();out;out=out->NextOutParm) {
            if(++seen>16)throw std::runtime_error("Quest text output chain invalid");
            if(out->Property==output){if(++matches>1 || !out->PropAddr)throw std::runtime_error("Quest text output is ambiguous");destination=out->PropAddr;}
        }
        PropertyHelper::SetTextPropertyValueFromJsonValue(destination,output,*text);
    }catch(const std::exception& error){ErrorOnceDiagnostic("quest-progress-text",PS::ToWideSafe(error.what()),
        "RS-NPC-018","Quest objective text presentation failed","Warning","Skipped",{},"QuestProgressComponent","Application");}
}
