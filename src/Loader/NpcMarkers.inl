void DragonWildsNpcLoader::ConfigureNpcMarkers(AActor* actor,const VendorDefinition& definition) {
    for(const auto* group:{"Map","OverheadIcon"}) {
        UObject* component=nullptr;
        try {
            if(!GhostMaterials::CanRender(actor))return;
            const bool map=std::string(group)=="Map";
            const auto tagText=std::string("RuneSchema.Npc.Marker.")+group;
            const FName tag(RC::to_generic_string(tagText),FNAME_Add);
            auto* type=ActorHelper::ResolveClass(map?TEXT("/Script/MinimapPlugin.MapIconComponent"):TEXT("/Script/Engine.BillboardComponent"));
            if(!type) {
                if(!definition.Markers.contains(group))continue;
                throw std::runtime_error("Marker component class unavailable");
            }
            for(auto* previous:actor->GetComponentsByClass(type)) {
                ActorHelper::FunctionCall tagged(previous,TEXT("/Script/Engine.ActorComponent:ComponentHasTag"));
                tagged.Arg(TEXT("Tag"),tag).Invoke();
                if(!tagged.Result<bool>())continue;
                ActorHelper::DestroyComponent(previous);
                m_createdComponents.erase(previous);
            }
            if(!definition.Markers.contains(group))continue;
            const auto& options=definition.Markers.at(group);
            if(!options.value("Enabled",false))continue;
            auto* texture=ActorHelper::ResolveObject(RC::to_generic_string(options.value("Icon",std::string(NpcMarkers::DefaultIcon))));
            auto* textureClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.Texture2D"));
            if(!texture || !textureClass || !texture->IsA(textureClass))throw std::runtime_error("Marker icon did not resolve to Texture2D");
            auto* root=ActorHelper::GetObjectRef(actor,TEXT("RootComponent"));
            if(!root)throw std::runtime_error("NPC root unavailable");
            ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
            add.Arg(TEXT("Class"),type).Arg(TEXT("bManualAttachment"),true).Arg(TEXT("RelativeTransform"),FTransform{}).Arg(TEXT("bDeferredFinish"),true).Invoke();
            component=add.Result<UObject*>();
            if(!component || !component->IsA(type) || component->GetOuterPrivate()!=actor)throw std::runtime_error("Marker component ownership unavailable");
            component->SetFlags(RF_Transient);
            m_createdComponents.insert(component);
            auto* tags=PropertyHelper::GetPropertyByName(type,TEXT("ComponentTags"));
            if(!CastField<FArrayProperty>(tags))throw std::runtime_error("Marker component tags unavailable");
            PropertyHelper::CopyJsonValueToContainer(component,tags,nlohmann::json::array({tagText}));
            HumanAttach(component,root,FName(TEXT("None"),FNAME_Add));
            RegisterComponent(actor,component,definition);
            if(map) {
                ActorHelper::FunctionCall icon(component,TEXT("/Script/MinimapPlugin.MapIconComponent:SetIconTexture"));
                icon.Arg(TEXT("NewIcon"),texture).Invoke();
                const FString label(RC::to_generic_string(NpcMarkers::MapLabel(options,definition.DisplayName)).c_str());
                ActorHelper::FunctionCall text(component,TEXT("/Script/MinimapPlugin.MapIconComponent:SetIconLabel"));
                text.Arg(TEXT("NewLabel"),label).Invoke();
                ActorHelper::FunctionCall visible(component,TEXT("/Script/MinimapPlugin.MapIconComponent:SetIconVisible"));
                visible.Arg(TEXT("bNewVisible"),true).Invoke();
                if(options.contains("Size")) {
                    auto* size=PropertyHelper::GetPropertyByName(type,TEXT("IconSize"));
                    auto* unit=PropertyHelper::GetPropertyByName(type,TEXT("IconSizeUnit"));
                    if(!size || !unit)throw std::runtime_error("Map marker size contract is unavailable");
                    PropertyHelper::CopyJsonValueToContainer(component,size,options.at("Size"));
                    PropertyHelper::CopyJsonValueToContainer(component,unit,
                        NpcMarkers::MapSizeUnit(options.value("SizeMode",std::string("Pixels"))));
                }
            } else {
                ActorHelper::FunctionCall icon(component,TEXT("/Script/Engine.BillboardComponent:SetSprite"));
                icon.Arg(TEXT("NewSprite"),texture).Invoke();
                HumanCall(component,TEXT("/Script/Engine.SceneComponent:K2_SetRelativeLocation"),
                    {{"NewLocation",{{"X",0},{"Y",0},{"Z",options.value("Height",220.0)}}},{"bSweep",false},{"SweepHitResult",nlohmann::json::object()},{"bTeleport",true}});
                auto scale=options.value("Scale",0.35);
                if(options.value("SizeMode",std::string("Scale"))=="Pixels" && options.contains("Size")) {
                    auto* screen=PropertyHelper::GetPropertyByName(type,TEXT("ScreenSize"));
                    auto* scaled=PropertyHelper::GetPropertyByName(type,TEXT("bIsScreenSizeScaled"));
                    if(!screen || !scaled)throw std::runtime_error("Overhead pixel-size contract is unavailable");
                    PropertyHelper::CopyJsonValueToContainer(component,scaled,true);
                    PropertyHelper::CopyJsonValueToContainer(component,screen,options.at("Size").get<double>()/1080.0);
                    scale=1.0;
                }
                HumanCall(component,TEXT("/Script/Engine.SceneComponent:SetRelativeScale3D"),{{"NewScale3D",{{"X",scale},{"Y",scale},{"Z",scale}}}});
                ActorHelper::FunctionCall distance(component,TEXT("/Script/Engine.PrimitiveComponent:SetCullDistance"));
                distance.Arg(TEXT("NewCullDistance"),static_cast<float>(options.value("Distance",5000.0))).Invoke();
                ActorHelper::FunctionCall hidden(component,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"));
                hidden.Arg(TEXT("NewHidden"),false).Arg(TEXT("bPropagateToChildren"),false).Invoke();
            }
            RecordPhase(definition,map?"NPC.Marker.Map":"NPC.Marker.OverheadIcon",actor);
        } catch(const std::exception& error) {
            if(component)try {
                ActorHelper::DestroyComponent(component);m_createdComponents.erase(component);
            }catch(...){}
            WarnOnce("npc-marker:"+definition.ModName+":"+definition.Id+":"+group,
                RC::to_generic_string(std::format("NPC '{}' {} marker unavailable: {}",definition.Id,group,error.what())));
        }
    }
}
