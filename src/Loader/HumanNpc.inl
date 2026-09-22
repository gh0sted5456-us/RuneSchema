namespace {
    UObject* HumanEquipment(const std::string& reference) {
        if(!DragonWilds::IsCanonicalPersistenceId(reference))return ActorHelper::ResolveObject(RC::to_generic_string(reference));
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        auto* field=type?CastField<FStrProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PersistenceID"))):nullptr;
        if(!field || field->GetArrayDim()!=1)throw std::runtime_error("Human equipment persistence identity contract unavailable");
        TArray<UObject*> items;UECustom::UObjectGlobals::GetObjectsOfClass(type,items,true);
        UObject* found=nullptr;
        for(auto* item:items) {
            if(!item || item->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            const auto& value=field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(item));
            const auto& chars=value.GetCharArray();
            if(chars.Num()!=23 || !chars.GetData() || RC::to_string(RC::StringType(chars.GetData(),22))!=reference)continue;
            if(found && found!=item)throw std::runtime_error("Human equipment persistence identity is ambiguous");
            found=item;
        }
        if(!found)throw std::runtime_error("Human equipment persistence identity not loaded: "+reference);
        return found;
    }
    void NpcSetCollisionEnabled(UObject* component, bool queryOnly) {
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"))
            .Arg(TEXT("NewType"),NpcCollision::Enabled(queryOnly)).Invoke();
    }
    void NpcSetPawnResponse(UObject* component, bool block) {
        const auto args=NpcCollision::PawnResponse(block);
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionResponseToChannel"))
            .Arg(TEXT("Channel"),args.Channel).Arg(TEXT("NewResponse"),args.NewResponse).Invoke();
    }
    std::string HumanEnum(UObject* object,const TCHAR* field) {
        auto* property=PropertyHelper::GetPropertyByName(object->GetClassPrivate(),field);
        UEnum* enumeration=nullptr;
        int64 value=0;
        if(auto* p=CastField<FEnumProperty>(property);p && p->GetUnderlyingProperty()) {
            enumeration=p->GetEnum();
            value=p->GetUnderlyingProperty()->GetSignedIntPropertyValue(p->ContainerPtrToValuePtr<void>(object));
        } else if(auto* byte=CastField<FByteProperty>(property)) {
            enumeration=byte->GetEnum().Get();
            value=byte->GetUnsignedIntPropertyValue(byte->ContainerPtrToValuePtr<void>(object));
        }
        if(!enumeration)throw std::runtime_error("Human enum contract unavailable: "+RC::to_string(field));
        auto name=RC::to_string(enumeration->GetNameByValue(value).ToString());
        const auto colon=name.rfind("::");return colon==name.npos?name:name.substr(colon+2);
    }
    bool RuneSchemaHumanSkeleton(UObject* skeleton) {
        if(!skeleton)return false;
        const auto path=RC::to_string(skeleton->GetPathName());
        return path=="/Game/RuneSchema/Animation/Data/SKEL_RS_Human.SKEL_RS_Human"
            || path=="/Game/RuneSchema/Animation/Temporary/SK_M_MED_Body_A_01_Skeleton.SK_M_MED_Body_A_01_Skeleton"
            || path=="/Game/RuneSchema/Animation/Temporary/SK_F_MED_Body_A_01_Skeleton.SK_F_MED_Body_A_01_Skeleton"
            || path=="/Game/Art/Skeleton/Player/Invis/SKEL_M_MED_Invis_01.SKEL_M_MED_Invis_01";
    }
    void HumanValidateValue(FProperty* property,const nlohmann::json& value) {
        if(auto* structure=CastField<FStructProperty>(property);structure && value.is_object()) {
            auto* type=structure->GetStruct().Get();
            if(!type)throw std::runtime_error("Human struct type unavailable");
            for(const auto& [key,child]:value.items()) {
                auto* field=PropertyHelper::GetPropertyByName(type,RC::to_generic_string(key));
                if(!field)throw std::runtime_error("Human native struct field unavailable: "+key);
                HumanValidateValue(field,child);
            }
        }
        PropertyHelper::ValidateJsonValueType(property,value);
    }
    void HumanCall(UObject* target, const TCHAR* path, const nlohmann::json& args) {
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        if(!target || !fn || fn->GetReturnProperty())throw std::runtime_error("Human preview function unavailable");
        std::vector<uint8_t> params(fn->GetParmsSize(),0);
        std::vector<FProperty*> initialized;
        const auto clean=[&] {for(auto* p:initialized)p->DestroyValue_InContainer(params.data());};
        try {
            for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default)) {
                if(!p->HasAnyPropertyFlags(CPF_Parm))continue;
                const auto name=RC::to_string(p->GetName());
                if(!args.contains(name) || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0
                    || p->GetOffset_Internal()+p->GetSize()>fn->GetParmsSize())
                    throw std::runtime_error("Human preview parameter contract changed: "+name);
                HumanValidateValue(p,args.at(name));
                p->InitializeValue_InContainer(params.data());initialized.push_back(p);
                PropertyHelper::CopyJsonValueToContainer(params.data(),p,args.at(name));
            }
            if(initialized.size()!=args.size())throw std::runtime_error("Human preview argument count mismatch");
            target->ProcessEvent(fn,params.data());
        } catch(...) {clean();throw;}
        clean();
    }
    void HumanAttach(UObject* child, UObject* parent, const FName& socket) {
        ActorHelper::FunctionCall attach(child,TEXT("/Script/Engine.SceneComponent:K2_AttachToComponent"));
        const uint8_t snap=2;
        attach.Arg(TEXT("Parent"),parent).Arg(TEXT("SocketName"),socket)
            .Arg(TEXT("LocationRule"),snap).Arg(TEXT("RotationRule"),snap)
            .Arg(TEXT("ScaleRule"),snap).Arg(TEXT("bWeldSimulatedBodies"),false).Invoke();
        if(!attach.Result<bool>())throw std::runtime_error("Human component attachment failed");
    }
    nlohmann::json HumanTransformField(UObject* source,const TCHAR* name,const TCHAR* type,
        std::initializer_list<const TCHAR*> axes) {
        auto* p=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(source->GetClassPrivate(),name));
        if(!p || !p->GetStruct().Get() || p->GetStruct()->GetFName()!=FName(type,FNAME_Add))
            throw std::runtime_error("Held mesh transform contract unavailable");
        auto* data=p->ContainerPtrToValuePtr<void>(source);
        nlohmann::json result=nlohmann::json::object();
        for(const auto* axis:axes) {
            auto* field=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(p->GetStruct().Get(),axis));
            if(!field || !field->IsFloatingPoint())throw std::runtime_error("Held mesh transform axis unavailable");
            const auto value=field->GetFloatingPointPropertyValue(field->ContainerPtrToValuePtr<void>(data));
            if(!std::isfinite(value))throw std::runtime_error("Held mesh transform must be finite");
            result[RC::to_string(axis)]=value;
        }
        return result;
    }
}

void DragonWildsNpcLoader::ApplyHumanVisuals(AActor* actor, VendorDefinition& definition) {
    auto* previewClass=ActorHelper::ResolveClass(TEXT("/Game/Gameplay/Frontend/BP_PlayerCharacterPreview.BP_PlayerCharacterPreview_C"));
    auto* nativeClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.PlayerCharacterPreview"));
    auto* childClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.ChildActorComponent"));
    auto* anchor=FindMeshComponent(actor);
    if(!previewClass || !nativeClass || !previewClass->IsChildOf(nativeClass) || !childClass || !anchor)
        throw std::runtime_error("Human preview class or neutral mesh anchor unavailable");
    auto* child=EnsureComponent(actor,childClass);
    ActorHelper::FunctionCall(child,TEXT("/Script/Engine.ActorComponent:SetIsReplicated")).Arg(TEXT("ShouldReplicate"),false).Invoke();
    const bool created=m_createdComponents.contains(child);
    auto* previousPreview=ActorHelper::GetObjectRef(child,TEXT("ChildActor"));
    if(previousPreview && !previousPreview->IsA(previewClass))throw std::runtime_error("Human child component is occupied by a different actor class");
    RegisterComponent(actor,child,definition);
    try {
        HumanAttach(child,anchor,FName());
        auto* preview=ActorHelper::GetObjectRef(child,TEXT("ChildActor"));
        if(preview && !preview->IsA(previewClass))throw std::runtime_error("Human child component belongs to a different actor class");
        if(!preview) {
            ActorHelper::FunctionCall set(child,TEXT("/Script/Engine.ChildActorComponent:SetChildActorClass"));
            set.Arg(TEXT("InClass"),previewClass).Invoke();
            preview=ActorHelper::GetObjectRef(child,TEXT("ChildActor"));
        }
        if(!preview || !preview->IsA(previewClass) || static_cast<AActor*>(preview)->GetWorld()!=actor->GetWorld())
            throw std::runtime_error("Human child preview did not initialize in the NPC world");
        ActorHelper::FunctionCall(preview,TEXT("/Script/Engine.Actor:SetReplicates")).Arg(TEXT("bInReplicates"),false).Invoke();
        ActorHelper::FunctionCall collision(preview,TEXT("/Script/Engine.Actor:SetActorEnableCollision"));
        collision.Arg(TEXT("bNewActorEnableCollision"),false).Invoke();
        auto* rotating=ActorHelper::GetObjectRef(preview,TEXT("Rotating"));
        if(rotating) {
            ActorHelper::FunctionCall tick(rotating,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled"));
            tick.Arg(TEXT("bEnabled"),false).Invoke();
        }
        for(const auto* field:{TEXT("InvisMeshComponent"),TEXT("BodyMeshComponent"),TEXT("HeadMeshComponent"),TEXT("InventoryComponent")})
            if(!ActorHelper::GetObjectRef(preview,field))throw std::runtime_error("Human preview native component initialization incomplete");

        nlohmann::json customization=nlohmann::json::object();
        for(size_t i=0;i<HumanNpc::AppearanceKeys.size();++i) {
            const auto path=std::format("/Game/Gameplay/Character/Player/Customization/DT_Customization_{}.DT_Customization_{}",HumanNpc::Tables[i],HumanNpc::Tables[i]);
            auto* object=ActorHelper::ResolveObject(RC::to_generic_string(path));
            auto* tableClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.DataTable"));
            const auto row=definition.Appearance.at(HumanNpc::AppearanceKeys[i]).get<std::string>();
            if(!object || !tableClass || !object->IsA(tableClass)
                || !static_cast<UDataTable*>(object)->FindRowUnchecked(FName(RC::to_generic_string(row),FNAME_Add)))
                throw std::runtime_error("Human appearance row unavailable: "+row);
            customization[HumanNpc::HandleKeys[i]]={{"DataTable",path},{"RowName",row}};
        }
        RecordPhase(definition,"Human.Appearance.Before",actor);
        HumanCall(preview,TEXT("/Script/Dominion.PlayerCharacterPreview:ApplyFullCustomization"),
            {{"CustomizationData",customization},{"bInIsNewCharacter",false}});

        nlohmann::json loadout=nlohmann::json::array();
        UObject* held=nullptr;
        UObject* offhand=nullptr;
        bool mainHandTwoHanded=false;
        auto* wearableClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.WearableEquipmentData"));
        auto* heldClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.HeldEquipmentData"));
        for(const auto& [slot,path]:definition.Equipment.items()) {
            auto* item=HumanEquipment(path.get<std::string>());
            const bool heldSlot=slot=="MainHand" || slot=="OffHand";
            auto* expected=heldSlot?heldClass:wearableClass;
            if(!item || !expected || !item->IsA(expected))throw std::runtime_error("Human equipment has the wrong item type: "+slot);
            const auto nativeSlot=HumanEnum(item,TEXT("Slot"));
            if(!heldSlot && nativeSlot!=slot)throw std::runtime_error("Human equipment slot mismatch: "+slot+" versus "+nativeSlot);
            if(slot=="MainHand" && !HumanNpc::IsMainHandSlot(nativeSlot))
                throw std::runtime_error("Human MainHand requires HeldOnlyRight or HeldTwoHanded equipment");
            if(slot=="OffHand" && !HumanNpc::IsOffHandSlot(nativeSlot))
                throw std::runtime_error("Human OffHand requires HeldOnlyLeft equipment");
            if(slot=="MainHand"){held=item;mainHandTwoHanded=nativeSlot=="HeldTwoHanded";}
            else if(slot=="OffHand")offhand=item;
            else loadout.push_back(RC::to_string(item->GetPathName()));
        }
        if(mainHandTwoHanded && offhand)
            throw std::runtime_error("Human equipment cannot combine a two-handed MainHand item with OffHand equipment");
        RecordPhase(definition,"Human.Outfit.Before",actor);
        HumanCall(preview,TEXT("/Script/Dominion.PlayerCharacterPreview:HandleCharacterLoadoutChanged"),
            {{"CustomizationLoadout",loadout},{"bInIsNewCharacter",false}});

        // Use the preview's native animation mesh directly. The body, head,
        // hair and outfit components are already authored to follow this mesh;
        // inserting a second cooked driver can leave the whole follower graph
        // initialized but visually empty.
        auto* mesh=ActorHelper::GetObjectRef(preview,TEXT("InvisMeshComponent"));
        if(!mesh)throw std::runtime_error("Human native preview animation mesh is unavailable");
        auto* skeletalMesh=ActorHelper::GetObjectRef(mesh,TEXT("SkeletalMesh"));
        auto* skeleton=skeletalMesh?ActorHelper::GetObjectRef(skeletalMesh,TEXT("Skeleton")):nullptr;
        UObject* idle=nullptr;
        const auto& pose=definition.Pose;
        auto mode=pose.Mode;
        const bool equipmentPose=pose.Name=="Equipment" && definition.IdleAnimationPath.empty();
        if(!definition.IdleAnimationPath.empty())idle=ActorHelper::ResolveObject(RC::to_generic_string(definition.IdleAnimationPath));
        else if(!pose.Path.empty())idle=ActorHelper::ResolveObject(RC::to_generic_string(pose.Path));
        else if(held)idle=ActorHelper::GetObjectRef(held,TEXT("AnimationPosesSequence"));
        else idle=ActorHelper::ResolveObject(TEXT("/Game/Art/Animation/PlayerM/Idle/A_PlayerM_CharacterSelect_Fresh.A_PlayerM_CharacterSelect_Fresh"));
        if(!definition.IdleAnimationPath.empty() || (equipmentPose && !held))mode=HumanPose::Playback::Loop;
        const bool heldPose=mode==HumanPose::Playback::Hold;
        auto* animationClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.AnimSequence"));
        auto* animationSkeleton=idle?ActorHelper::GetObjectRef(idle,TEXT("Skeleton")):nullptr;
        const bool skeletonCompatible=animationSkeleton==skeleton;
        if(!skeleton || !idle || !animationClass || !idle->IsA(animationClass) || !skeletonCompatible)
            throw std::runtime_error("Human idle must be an AnimSequence compatible with the preview skeleton");
        if(HumanEnum(idle,TEXT("AdditiveAnimType"))!="AAT_None")
            throw std::runtime_error("Human Pose/IdleAnimation requires a full-body sequence, not an additive layer");
        if(heldPose) {
            auto* duration=CastField<FFloatProperty>(PropertyHelper::GetPropertyByName(idle->GetClassPrivate(),TEXT("SequenceLength")));
            if(!duration || duration->GetElementSize()!=sizeof(float))throw std::runtime_error("Human pose duration unavailable");
            HumanPose::ValidateTime(pose.Time,*duration->ContainerPtrToValuePtr<float>(idle));
        }
        ActorHelper::FunctionCall play(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:PlayAnimation"));
        play.Arg(TEXT("NewAnimToPlay"),idle).Arg(TEXT("bLooping"),mode==HumanPose::Playback::Loop).Invoke();
        if(heldPose) {
            ActorHelper::FunctionCall position(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:SetPosition"));
            position.Arg(TEXT("InPos"),pose.Time).Arg(TEXT("bFireNotifies"),false).Invoke();
            ActorHelper::FunctionCall stop(mesh,TEXT("/Script/Engine.SkeletalMeshComponent:Stop"));stop.Invoke();
        }
        ApplyVendorDisplayName(static_cast<AActor*>(preview),definition);
        const auto applyHeldVisual=[&](UObject* held,const char* componentTag,const char* phaseName)->UObject* {
        UObject* renderedVisual=nullptr;
        if(held) {
            UObject* staff=nullptr;
            nlohmann::json equipmentDiagnostics={{"HeldVisualReady",false}};
            HumanNpc::ApplyOptionalHeldVisual([&] {
            auto* soft=CastField<FSoftClassProperty>(PropertyHelper::GetPropertyByName(held->GetClassPrivate(),TEXT("HeldEquipmentActorClass")));
            if(!soft || soft->GetElementSize()!=sizeof(UECustom::TSoftClassPtr<UObject>))
                throw std::runtime_error("Held item actor class contract unavailable");
            auto* equipmentClass=UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(
                *soft->ContainerPtrToValuePtr<UECustom::TSoftClassPtr<UObject>>(held));
            auto* nativeEquipment=ActorHelper::ResolveClass(TEXT("/Script/Dominion.HeldEquipmentActor"));
            if(!equipmentClass || !nativeEquipment || !equipmentClass->IsChildOf(nativeEquipment))
                throw std::runtime_error("Held equipment visual class unavailable");
            auto* defaults=equipmentClass->GetClassDefaultObject().Get();
            auto* source=defaults?ActorHelper::GetObjectRef(defaults,TEXT("MeshComponent")):nullptr;
            auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMeshComponent"));
            auto* visual=source?ActorHelper::GetObjectRef(source,TEXT("SkeletalMesh")):nullptr;
            auto* visualClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMesh"));
            if(!source || !componentClass || !source->IsA(componentClass) || !visual || !visualClass || !visual->IsA(visualClass))
                throw std::runtime_error("Held equipment skeletal visual unavailable");
            if(ActorHelper::GetObjectRef(source,TEXT("AttachParent"))!=ActorHelper::GetObjectRef(defaults,TEXT("SceneRoot")))
                throw std::runtime_error("Held equipment uses an unsupported nested mesh transform");
            auto* socketField=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(defaults->GetClassPrivate(),TEXT("DefaultAttachSocketName")));
            if(!socketField || socketField->GetElementSize()!=sizeof(FName))
                throw std::runtime_error("Held equipment attachment socket unavailable");
            const auto declared=RC::to_string(socketField->ContainerPtrToValuePtr<FName>(defaults)->ToString());
            const auto hand=HumanEnum(held,TEXT("HandPreference"));
            equipmentDiagnostics["HeldDeclaredSocket"]=declared;
            equipmentDiagnostics["HeldHandPreference"]=hand;
            equipmentDiagnostics["HeldSkeleton"]=RC::to_string(skeleton->GetPathName());
            const auto socketName=HumanNpc::SelectAttachment(declared,hand,[&](const std::string& name) {
                ActorHelper::FunctionCall exists(mesh,TEXT("/Script/Engine.SceneComponent:DoesSocketExist"));
                exists.Arg(TEXT("InSocketName"),FName(RC::to_generic_string(name),FNAME_Add)).Invoke();
                const bool available=exists.Result<bool>();
                equipmentDiagnostics["HeldSocketCandidates"][name]=available;
                return available;
            });
            equipmentDiagnostics["HeldSocket"]=socketName;
            equipmentDiagnostics["HeldSocketSource"]=socketName.empty()?"unresolved":socketName==declared?"equipment-default":"hand-prop-bone";
            if(socketName.empty())throw std::runtime_error("No valid equipment socket or hand prop bone; weapon visual omitted");
            const FName socket(RC::to_generic_string(socketName),FNAME_Add);
            auto location=HumanTransformField(source,TEXT("RelativeLocation"),TEXT("Vector"),{TEXT("X"),TEXT("Y"),TEXT("Z")});
            const auto rotation=HumanTransformField(source,TEXT("RelativeRotation"),TEXT("Rotator"),{TEXT("Pitch"),TEXT("Yaw"),TEXT("Roll")});
            const auto scale=HumanTransformField(source,TEXT("RelativeScale3D"),TEXT("Vector"),{TEXT("X"),TEXT("Y"),TEXT("Z")});
            const FName tag(RC::to_generic_string(componentTag),FNAME_Add);
            for(auto* candidate:static_cast<AActor*>(preview)->GetComponentsByClass(componentClass)) {
                ActorHelper::FunctionCall hasTag(candidate,TEXT("/Script/Engine.ActorComponent:ComponentHasTag"));
                hasTag.Arg(TEXT("Tag"),tag).Invoke();
                if(!hasTag.Result<bool>())continue;
                if(staff || !m_createdComponents.contains(candidate))throw std::runtime_error("Human held visual ownership conflict");
                staff=candidate;
            }
            if(!staff) {
                ActorHelper::FunctionCall add(preview,TEXT("/Script/Engine.Actor:AddComponentByClass"));
                add.Arg(TEXT("Class"),componentClass).Arg(TEXT("bManualAttachment"),true)
                    .Arg(TEXT("RelativeTransform"),FTransform{}).Arg(TEXT("bDeferredFinish"),true).Invoke();
                staff=add.Result<UObject*>();
                if(!staff || !staff->IsA(componentClass))throw std::runtime_error("Human held visual creation failed");
                m_createdComponents.insert(staff);
                auto* tags=PropertyHelper::GetPropertyByName(staff->GetClassPrivate(),TEXT("ComponentTags"));
                if(!CastField<FArrayProperty>(tags))throw std::runtime_error("Human held visual tag contract unavailable");
                PropertyHelper::CopyJsonValueToContainer(staff,tags,nlohmann::json::array({componentTag}));
            }
            RegisterComponent(static_cast<AActor*>(preview),staff,definition);
            auto* materials=PropertyHelper::GetPropertyByName(componentClass,TEXT("OverrideMaterials"));
            if(!CastField<FArrayProperty>(materials))throw std::runtime_error("Held visual materials contract unavailable");
            materials->CopyCompleteValue_InContainer(staff,source);
            const auto setter=ResolveNpcMeshSetter(staff,visual);
            equipmentDiagnostics["HeldMeshSetter"]=RC::to_string(setter.Function->GetPathName());
            setter.Apply(staff,visual);
            if(ActorHelper::GetObjectRef(staff,TEXT("SkeletalMesh"))!=visual)throw std::runtime_error("Held visual mesh could not be applied");
            ActorHelper::FunctionCall disable(staff,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"));
            disable.Arg(TEXT("NewType"),uint8_t(0)).Invoke();
            HumanAttach(staff,mesh,socket);
            HumanCall(staff,TEXT("/Script/Engine.SceneComponent:K2_SetRelativeLocationAndRotation"),
                {{"NewLocation",location},{"NewRotation",rotation},{"bSweep",false},{"SweepHitResult",nlohmann::json::object()},{"bTeleport",true}});
            HumanCall(staff,TEXT("/Script/Engine.SceneComponent:SetRelativeScale3D"),{{"NewScale3D",scale}});
            equipmentDiagnostics["HeldVisualReady"]=true;
            equipmentDiagnostics["HeldMesh"]=RC::to_string(visual->GetPathName());
            equipmentDiagnostics["HeldRelativeLocation"]=location;
            equipmentDiagnostics["HeldRelativeRotation"]=rotation;
            auto visibilityPose=pose;
            if(!definition.IdleAnimationPath.empty())visibilityPose.Name.clear();
            ActorHelper::FunctionCall visibility(staff,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"));
            visibility.Arg(TEXT("NewHidden"),HumanPose::HideWeapon(visibilityPose)).Arg(TEXT("bPropagateToChildren"),false).Invoke();
            renderedVisual=staff;
            RecordPhase(definition,phaseName,actor,equipmentDiagnostics);
            },[&](const char* error) {
                if(staff && m_createdComponents.contains(staff))try {
                    ActorHelper::DestroyComponent(staff);
                    m_createdComponents.erase(staff);
                } catch(...) {}
                try {
                    equipmentDiagnostics["HeldVisualReady"]=false;
                    equipmentDiagnostics["HeldVisualError"]=error;
                    RecordPhase(definition,"Human.Equipment.Omitted",actor,equipmentDiagnostics);
                    WarnOnce("human-equipment:"+definition.ModName+":"+definition.Id+":"+componentTag,
                        RC::to_generic_string(std::format("NPC '{}' omitted its held visual; interaction setup will continue: {}",definition.Id,error)));
                } catch(...) {}
            });
        }
        return renderedVisual;
        };
        auto* heldVisual=applyHeldVisual(held,"RuneSchema.Human.MainHand","Human.Equipment.MainHand.Ready");
        auto* offhandVisual=applyHeldVisual(offhand,"RuneSchema.Human.OffHand","Human.Equipment.OffHand.Ready");
        if(!definition.Ghost.empty() && GhostMaterials::CanRender(preview)) {
            std::vector<UObject*> roots;
            struct ReleaseRoots {
                std::vector<UObject*>& Roots;
                ~ReleaseRoots(){for(auto* object:Roots)object->ClearRootSet();}
            } releaseRoots{roots};
            try {
                const bool character=definition.Ghost.value("Character",false);
                const bool equipment=definition.Ghost.value("Equipment",false);
                const bool weapons=definition.Ghost.value("Weapons",false);
                if(character || equipment || weapons) {
                    const auto materials=GhostMaterials::Create(preview,{{"Overlay",true},{"BodyMaterial",true}},roots);
                    const auto apply=[&](UObject* component) {
                        if(!component)return;
                        auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.SkinnedMeshComponent"));
                        if(!componentClass || !component->IsA(componentClass) || component->GetOuterPrivate()!=preview)
                            throw std::runtime_error("NPC ghost target is not a preview-owned skinned mesh");
                        ActorHelper::FunctionCall count(component,TEXT("/Script/Engine.PrimitiveComponent:GetNumMaterials"));
                        count.Invoke();const auto slots=count.Result<int32_t>();
                        if(slots<0 || slots>256)throw std::runtime_error("NPC ghost material slot count is invalid");
                        for(int32_t slot=0;slot<slots;++slot) {
                            ActorHelper::FunctionCall set(component,TEXT("/Script/Engine.PrimitiveComponent:SetMaterial"));
                            set.Arg(TEXT("ElementIndex"),slot).Arg(TEXT("Material"),materials.Body).Invoke();
                        }
                        ActorHelper::SetObjectRef(component,TEXT("OverlayMaterial"),materials.Overlay);
                    };
                    if(character)for(const auto* field:{TEXT("BodyMeshComponent"),TEXT("HeadMeshComponent"),TEXT("HairZone1MeshComponent"),TEXT("FacialHairZone1MeshComponent")})
                        apply(ActorHelper::GetObjectRef(preview,field));
                    if(equipment)for(const auto* field:{TEXT("BodyDefaultOutfitMeshComponent"),TEXT("LegsDefaultOutfitMeshComponent"),TEXT("HeadOutfitMeshComponent"),TEXT("BodyOutfitMeshComponent"),TEXT("LegsOutfitMeshComponent"),TEXT("CapeOutfitMeshComponent"),TEXT("TrinketOutfitMeshComponent"),TEXT("OffhandPropMeshComponent")})
                        apply(ActorHelper::GetObjectRef(preview,field));
                    if(weapons){apply(heldVisual);apply(offhandVisual);}
                }
            } catch(const std::exception& error) {
                WarnOnce("human-ghost:"+definition.ModName+":"+definition.Id,
                    RC::to_generic_string(std::format("NPC '{}' ghost visual could not be fully applied: {}",definition.Id,error.what())));
            }
        }
        try {
            auto* box=ActorHelper::GetObjectRef(preview,TEXT("StaticCollision"));
            auto* boxClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.BoxComponent"));
            if(!box || !boxClass || !box->IsA(boxClass) || box->GetOuterPrivate()!=preview)
                throw std::runtime_error("Preview-owned StaticCollision box unavailable");
            ActorHelper::FunctionCall hideBox(box,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"));
            hideBox.Arg(TEXT("NewHidden"),true).Arg(TEXT("bPropagateToChildren"),false).Invoke();
        } catch(const std::exception& error) {
            WarnOnce("human-preview-box:"+definition.ModName+":"+definition.Id,
                RC::to_generic_string(std::format("NPC '{}' preview box could not be hidden: {}",definition.Id,error.what())));
        }
        ApplyNpcVisualEffect(preview,definition);
        ActorHelper::FunctionCall hide(anchor,TEXT("/Script/Engine.SceneComponent:SetVisibility"));
        hide.Arg(TEXT("bNewVisibility"),false).Arg(TEXT("bPropagateToChildren"),false).Invoke();
        NpcSetCollisionEnabled(anchor,false);
        auto* capsules=ActorHelper::ResolveClass(TEXT("/Script/Engine.CapsuleComponent"));
        if(!capsules)throw std::runtime_error("Human NPC capsule class unavailable");
        for(auto* capsule:actor->GetComponentsByClass(capsules))
            NpcSetPawnResponse(capsule,definition.EnableCollision);
        RecordPhase(definition,"Human.Ready",actor,{{"Preview",RC::to_string(preview->GetPathName())},{"HeldPose",heldPose},
            {"Animation",RC::to_string(idle->GetPathName())},{"Equipment",definition.Equipment}});
    } catch(...) {
        if(created)try {
            ActorHelper::DestroyComponent(child);
        } catch(...) {}
        throw;
    }
}
