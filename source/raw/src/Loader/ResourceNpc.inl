void DragonWildsNpcLoader::ApplyResourceVisuals(AActor* actor,VendorDefinition& definition) {
    auto* visual=ActorHelper::ResolveObject(RC::to_generic_string(definition.VisualMeshPath));
    auto* assetType=ActorHelper::ResolveClass(TEXT("/Script/Engine.StaticMesh"));
    auto* componentType=ActorHelper::ResolveClass(TEXT("/Script/Engine.StaticMeshComponent"));
    if(!visual || !assetType || !visual->IsA(assetType))throw std::runtime_error("Resource Mesh must resolve to a StaticMesh");
    if(!componentType)throw std::runtime_error("StaticMeshComponent class unavailable");
    std::vector<NpcComponents::Candidate<UObject>> candidates;
    for(auto* component:actor->GetComponentsByClass(componentType)) {
        if(!component || !component->IsA(componentType))continue;
        candidates.push_back({component,component->GetOuterPrivate()==actor,
            !component->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)),
            component->GetFName()==FName(TEXT("ReplacementMeshComponent"),FNAME_Add)});
    }
    auto* mesh=NpcComponents::Select(candidates);
    const bool created=mesh==nullptr;
    if(created) {
        ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
        add.Arg(TEXT("Class"),componentType).Arg(TEXT("bManualAttachment"),true)
            .Arg(TEXT("RelativeTransform"),FTransform{}).Arg(TEXT("bDeferredFinish"),true).Invoke();
        mesh=add.Result<UObject*>();
        if(!mesh || !mesh->IsA(componentType) || mesh->GetOuterPrivate()!=actor)
            throw std::runtime_error("Resource mesh creation did not return an owned StaticMeshComponent");
        mesh->SetFlags(RF_Transient);
        m_createdComponents.insert(mesh);
    }
    try {
    auto* root=ActorHelper::GetObjectRef(actor,TEXT("RootComponent"));
    if(!root || root==mesh)throw std::runtime_error("Resource NPC root is missing or is its visual component");
    RegisterComponent(actor,mesh,definition);
    HumanAttach(mesh,root,FName(TEXT("None"),FNAME_Add));
    auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Engine.StaticMeshComponent:SetStaticMesh"),false);
    auto* input=fn?CastField<FObjectProperty>(fn->FindProperty(FName(TEXT("NewMesh"),FNAME_Find))):nullptr;
    auto* result=fn?CastField<FBoolProperty>(fn->GetReturnProperty()):nullptr;
    size_t count=0;
    if(fn)for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
    if(!input || !result || count!=2 || fn->GetParmsSize()!=9 || input->GetArrayDim()!=1
        || input->GetOffset_Internal()!=0 || input->GetElementSize()!=8 || !input->HasAnyPropertyFlags(CPF_Parm)
        || !input->GetPropertyClass().Get() || !visual->IsA(input->GetPropertyClass().Get())
        || result->GetArrayDim()!=1 || result->GetOffset_Internal()!=8 || result->GetElementSize()!=1
        || !result->IsNativeBool() || result->GetByteOffset()!=0)
        throw std::runtime_error("Resource StaticMesh setter contract changed");
    ActorHelper::FunctionCall set(mesh,fn->GetPathName());
    set.Arg(TEXT("NewMesh"),visual).Invoke();
    // SetStaticMesh may return false when the same asset is already assigned.
    if(ActorHelper::GetObjectRef(mesh,TEXT("StaticMesh"))!=visual)
        throw std::runtime_error("Resource StaticMesh assignment failed");
    ApplyVendorMaterials(mesh,definition);
    HumanCall(mesh,TEXT("/Script/Engine.SceneComponent:SetVisibility"),{{"bNewVisibility",!definition.HideMesh},{"bPropagateToChildren",false}});
    HumanCall(mesh,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"),{{"NewHidden",definition.HideMesh},{"bPropagateToChildren",false}});
    const bool collision=definition.EnableCollision && definition.MeshCollision!="None";
    if(collision) {
        if(!ActorHelper::GetObjectRef(visual,TEXT("BodySetup")))
            throw std::runtime_error("Resource Mesh has no authored BodySetup for collision");
        HumanCall(mesh,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionProfileName"),{{"InCollisionProfileName","BlockAll"},{"bUpdateOverlaps",true}});
    }
    NpcSetCollisionEnabled(mesh,collision);
    auto* capsuleType=ActorHelper::ResolveClass(TEXT("/Script/Engine.CapsuleComponent"));
    if(!capsuleType)throw std::runtime_error("Resource capsule class unavailable");
    for(auto* capsule:actor->GetComponentsByClass(capsuleType))
        NpcSetPawnResponse(capsule,false);
    if(auto* skeletal=FindMeshComponent(actor))
        NpcSetCollisionEnabled(skeletal,false);
    RecordPhase(definition,"Resource.Ready",actor,{{"Mesh",definition.VisualMeshPath},{"Collision",collision?"AuthoredQuery":"None"}});
    } catch(...) {
        if(created)try {ActorHelper::DestroyComponent(mesh);m_createdComponents.erase(mesh);}catch(...){}
        throw;
    }
}
