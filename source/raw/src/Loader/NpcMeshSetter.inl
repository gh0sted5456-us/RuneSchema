namespace {
    struct NpcMeshSetter {
        UFunction* Function;
        FProperty* Asset;
        FProperty* Reinit;
        void Apply(UObject* component,UObject* mesh) const {
            ActorHelper::FunctionCall call(component,Function->GetPathName());
            call.Arg(Asset->GetName().c_str(),mesh);
            if(Reinit)call.Arg(Reinit->GetName().c_str(),true);
            call.Invoke();
        }
    };
    NpcMeshSetter ResolveNpcMeshSetter(UObject* mesh,UObject* visualMesh) {
        if(!mesh || !visualMesh)throw std::runtime_error("Mesh setter requires a component and asset");
        UFunction* setter = nullptr;
        FProperty* assetParameter = nullptr;
        FProperty* reinitParameter = nullptr;
        for (const auto* method : {TEXT("SetSkinnedAssetAndUpdate"), TEXT("SetSkeletalMesh"), TEXT("SetSkeletalMeshAsset")}) {
            for (UStruct* owner = mesh->GetClassPrivate(); owner; owner = owner->GetSuperStruct()) {
                const auto functionPath = owner->GetPathName() + TEXT(":") + method;
                auto* candidate = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, functionPath.c_str(), false);
                if (!candidate || candidate->GetReturnProperty()) continue;
                FProperty* objectArg = nullptr;
                FProperty* boolArg = nullptr;
                bool usable = true;
                for (auto* field : TFieldRange<FProperty>(candidate, EFieldIterationFlags::Default)) {
                    if (!field->HasAnyPropertyFlags(CPF_Parm)) continue;
                    if (field->HasAnyPropertyFlags(CPF_OutParm) || field->GetArrayDim() != 1) { usable=false; break; }
                    if (auto* objectField = CastField<FObjectProperty>(field)) {
                        auto* expected = objectField->GetPropertyClass().Get();
                        if (objectArg || field->GetElementSize() != sizeof(UObject*) || !expected || !visualMesh->IsA(expected)) { usable=false; break; }
                        objectArg = field;
                    } else if (auto* boolean = CastField<FBoolProperty>(field)) {
                        if (boolArg || field->GetElementSize() != sizeof(bool) || !boolean->IsNativeBool() || boolean->GetByteOffset() != 0) { usable=false; break; }
                        boolArg = field;
                    } else { usable=false; break; }
                }
                if (usable && objectArg) { setter=candidate; assetParameter=objectArg; reinitParameter=boolArg; break; }
            }
            if (setter) break;
        }
        if (!setter) throw std::runtime_error("No compatible reflected native mesh setter was found in the component's class ancestry");
        return {setter,assetParameter,reinitParameter};
    }
}
