// Included in DragonWildsSpawnLoader.cpp, on the game thread only.
namespace {
    struct ToolMapTravelJob {
        PS::WeakObjectHandle World,Controller,Pawn,Anchor,Source,Service;
        PS::MapProjection::Projection Projection;PS::MapProjection::Point Point;
        PS::MapLanding::Position Landing;PS::MapTravel::Gate Gate;
        double Radius=0,HalfHeight=0,GroundZ=0;bool NearMarker=false;
        uint64_t Generation=0;std::chrono::steady_clock::time_point Next{},NextProgress{};
        std::string Phase;
        std::string LastGroundError;
    };
    std::optional<ToolMapTravelJob> g_mapTravel;
    std::int64_t ToolMapTravelTime(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    void ToolMapTravelProgress(const char* phase,const char* status){
        if(!g_mapTravel)return;auto& job=*g_mapTravel;const auto now=std::chrono::steady_clock::now();
        if(job.Phase==phase&&now<job.NextProgress)return;
        job.Phase=phase;job.NextProgress=now+std::chrono::milliseconds(500);
        PS::SpawnToolRequests::Progress(status,{{"MapTravel",{{"Phase",phase},{"ElapsedSeconds",std::max<std::int64_t>(0,ToolMapTravelTime()-job.Gate.Began)/1000},
            {"Committed",job.Gate.Committed}}}});
    }
    bool ToolMapTravelAlive(UObject* object){return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed));}
    bool ToolMapTravelTeleporting(UObject* pawn){ActorHelper::FunctionCall call(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:IsTeleporting"));call.Invoke();return call.Result<bool>();}
    void ToolMapTravelReset(bool destroy=true){
        if(!g_mapTravel)return;
        // Only our transient, non-replicated actor is ever retired. During DLL
        // destruction do not call ProcessEvent against a shutting-down engine.
        if(destroy){
            if(auto* source=g_mapTravel->Source.Get();ToolMapTravelAlive(source))try{ActorHelper::FunctionCall off(source,TEXT("/Script/Engine.WorldPartitionStreamingSourceComponent:DisableStreamingSource"));off.Invoke();}catch(...){}
            if(auto* actor=g_mapTravel->Anchor.Get();ToolMapTravelAlive(actor)&&actor->IsA<AActor>())try{ActorHelper::DestroyActor(static_cast<AActor*>(actor));}catch(...){}
        }
        g_mapTravel.reset();
    }
    void ToolMapTravelFinish(const std::string& status,bool arrived=false){
        auto result=PS::SpawnToolRequests::Read();result["Status"]=status;
        if(arrived&&g_mapTravel)if(auto* pawn=g_mapTravel->Pawn.Get();ToolMapTravelAlive(pawn)){
            const auto at=ActorHelper::GetActorLocation(static_cast<AActor*>(pawn));
            result["MapPoint"]={{"GroundAvailable",true},{"Location",{g_mapTravel->Landing.X,g_mapTravel->Landing.Y,g_mapTravel->GroundZ}}};
            result["PlayerLocation"]={at.X(),at.Y(),at.Z()};
        }
        ToolMapTravelReset();PS::SpawnToolRequests::Publish(std::move(result));
    }
    UFunction* ToolMapTravelContract(UObject* service){
        using namespace DragonWilds::ReflectedContract;
        auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.TeleportationSubsystem:TeleportWithParams"),false);
        if(!service||!function||Count(function)!=4)throw std::runtime_error("The native teleport service contract is unavailable; no teleport attempted");
        auto* player=CastField<FObjectProperty>(function->FindProperty(FName(TEXT("PlayerCharacter"),FNAME_Find)));
        auto* params=CastField<FStructProperty>(function->FindProperty(FName(TEXT("TeleportParams"),FNAME_Find)));
        auto* callback=CastField<FDelegateProperty>(function->FindProperty(FName(TEXT("TeleportSuccessCallbackDelegate"),FNAME_Find)));
        auto* returned=CastField<FBoolProperty>(function->GetReturnProperty());
        if(!Input(player,function)||!Input(params,function)||!Input(callback,function)||!Output(returned,function)
            ||!params->GetStruct()||params->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.PlayerTeleportParams"))
            throw std::runtime_error("The native teleport parameters changed; no teleport attempted");
        auto* structure=params->GetStruct().Get();
        for(const auto* name:{TEXT("TargetLocation"),TEXT("TargetRotation"),TEXT("bShowLoadingScreen"),TEXT("bNoCheck"),TEXT("bIsATest"),TEXT("TeleportationType")}){
            auto* field=PropertyHelper::GetPropertyByName(structure,name);
            if(!Fits(field,structure->GetPropertiesSize()))throw std::runtime_error("Native teleport fields are unavailable; no teleport attempted");
        }
        for(const auto* name:{TEXT("bShowLoadingScreen"),TEXT("bNoCheck"),TEXT("bIsATest")})
            if(!CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(structure,name)))throw std::runtime_error("Native teleport safety flags changed type");
        auto* location=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(structure,TEXT("TargetLocation")));
        auto* rotation=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(structure,TEXT("TargetRotation")));
        if(!location||!rotation||!location->GetStruct()||!rotation->GetStruct()
            ||location->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Vector")
            ||rotation->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Rotator"))throw std::runtime_error("Native teleport coordinate types changed");
        return function;
    }
    void ToolMapTravelBegin(UWorld* world,UObject* controller,AActor* pawn,const PS::MapProjection::Projection& projection,PS::MapProjection::Point point,bool nearby,std::optional<double> groundHint={}){
        if(g_mapTravel)throw std::runtime_error("A map journey is already active");
        if(!world||!controller||!pawn||!projection.Contains(point)||pawn->GetWorld()!=world)throw std::runtime_error("Map travel has no valid player/world");
        auto* playerClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if(!playerClass||!pawn->IsA(playerClass))throw std::runtime_error("The selected pawn is not a native player character; no teleport attempted");
        if(ToolMapTravelTeleporting(pawn))throw std::runtime_error("This player is already teleporting; wait for native travel to finish");
        auto* capsule=ActorHelper::GetObjectRef(pawn,TEXT("CapsuleComponent"));if(!capsule)throw std::runtime_error("Player capsule is unavailable");
        ActorHelper::FunctionCall height(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleHalfHeight"));height.Invoke();
        ActorHelper::FunctionCall width(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleRadius"));width.Invoke();
        const double halfHeight=height.Result<float>(),radius=width.Result<float>();
        if(!std::isfinite(radius)||!std::isfinite(halfHeight)||radius<1||radius>500||halfHeight<radius||halfHeight>1000)throw std::runtime_error("Player collision bounds are invalid");
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.TeleportationSubsystem"));
        auto* library=ActorHelper::ResolveObject(TEXT("/Script/Engine.Default__SubsystemBlueprintLibrary"));
        if(!type||!library)throw std::runtime_error("Native world-subsystem lookup is unavailable; no teleport attempted");
        ActorHelper::FunctionCall get(library,TEXT("/Script/Engine.SubsystemBlueprintLibrary:GetWorldSubsystem"));get.Arg(TEXT("ContextObject"),static_cast<UObject*>(world)).Arg(TEXT("Class"),type).Invoke();
        auto* service=get.Result<UObject*>();if(!type||!service||!service->IsA(type))throw std::runtime_error("Native teleport service is not available in this world");
        ToolMapTravelContract(service);
        auto* anchorClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.TargetPoint"));
        auto* sourceClass=ActorHelper::ResolveClass(TEXT("/Script/Engine.WorldPartitionStreamingSourceComponent"));
        if(!anchorClass||!sourceClass)throw std::runtime_error("Destination streaming support is unavailable");
        ToolMapTravelJob job;job.World=PS::WeakObject(world);job.Controller=PS::WeakObject(controller);job.Pawn=PS::WeakObject(pawn);job.Service=PS::WeakObject(service);
        job.Point=point;job.Projection=projection;job.Radius=radius;job.HalfHeight=halfHeight;job.NearMarker=nearby;
        job.Gate=PS::MapTravel::Gate(ToolMapTravelTime());job.Generation=PS::SpawnToolRequests::Generation.load();g_mapTravel=std::move(job);
        try{
            const auto origin=ActorHelper::GetActorLocation(pawn);
            auto* anchor=ActorHelper::SpawnActor(world,anchorClass,FVector(point.X,point.Y,std::clamp(groundHint.value_or(origin.Z()),projection.MinZ,projection.MaxZ)),FRotator{},[](AActor* actor){
                actor->SetFlags(RF_Transient);ActorHelper::FunctionCall replicate(actor,TEXT("/Script/Engine.Actor:SetReplicates"));replicate.Arg(TEXT("bInReplicates"),false).Invoke();
            });
            if(!anchor)throw std::runtime_error("Could not create a destination streaming anchor");g_mapTravel->Anchor=PS::WeakObject(anchor);
            if(!ActorHelper::GetObjectRef(anchor,TEXT("RootComponent")))throw std::runtime_error("Destination anchor has no scene root");
            ActorHelper::FunctionCall add(anchor,TEXT("/Script/Engine.Actor:AddComponentByClass"));add.Arg(TEXT("Class"),sourceClass).Arg(TEXT("bManualAttachment"),true).Arg(TEXT("RelativeTransform"),FTransform{}).Arg(TEXT("bDeferredFinish"),true).Invoke();
            auto* source=add.Result<UObject*>();if(!source||!source->IsA(sourceClass)||source->GetOuterPrivate()!=anchor)throw std::runtime_error("Could not create an owned destination streaming source");
            g_mapTravel->Source=PS::WeakObject(source);source->SetFlags(RF_Transient);
            auto* state=PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("TargetState"));
            if(!CastField<FEnumProperty>(state))throw std::runtime_error("Streaming activation state is unavailable");
            PropertyHelper::CopyJsonValueToContainer(source,state,"Activated");
            ActorHelper::FunctionCall finish(anchor,TEXT("/Script/Engine.Actor:FinishAddComponent"));finish.Arg(TEXT("Component"),source).Arg(TEXT("bManualAttachment"),true).Arg(TEXT("RelativeTransform"),FTransform{}).Invoke();
            ActorHelper::FunctionCall enable(source,TEXT("/Script/Engine.WorldPartitionStreamingSourceComponent:EnableStreamingSource"));enable.Invoke();
            ActorHelper::FunctionCall enabled(source,TEXT("/Script/Engine.WorldPartitionStreamingSourceComponent:IsStreamingSourceEnabled"));enabled.Invoke();
            if(!enabled.Result<bool>())throw std::runtime_error("The destination streaming source did not enable");
            ToolMapTravelProgress("Preparing","Loading destination terrain...");
        }catch(...){ToolMapTravelReset();throw;}
    }
    bool ToolMapTravelPump(UWorld* activeWorld){
        if(!g_mapTravel)return false;
        if(g_mapTravel->Generation!=PS::SpawnToolRequests::Generation.load()){ToolMapTravelReset();return false;}
        const auto now=std::chrono::steady_clock::now();if(now<g_mapTravel->Next)return true;g_mapTravel->Next=now+std::chrono::milliseconds(100);
        try{
            auto& job=*g_mapTravel;auto* world=job.World.Get();auto* controller=job.Controller.Get();auto* pawnObject=job.Pawn.Get();auto* source=job.Source.Get();
            const bool valid=activeWorld&&world==activeWorld&&ToolMapTravelAlive(world)&&GetGameMode(activeWorld)&&ToolMapTravelAlive(controller)&&ToolMapTravelAlive(pawnObject)&&ToolMapTravelAlive(source)
                &&ActorHelper::GetObjectRef(controller,TEXT("Pawn"))==pawnObject;
            if(!valid){ToolMapTravelFinish("Map travel stopped because the world or selected character changed.");return true;}
            auto* pawn=static_cast<AActor*>(pawnObject);
            const bool cancelled=PS::SpawnToolRequests::CancelRequested.exchange(false);
            if(cancelled&&!job.Gate.Committed){ToolMapTravelFinish("Map travel cancelled before departure; the player was not moved.");return true;}
            ActorHelper::FunctionCall completed(source,TEXT("/Script/Engine.WorldPartitionStreamingSourceComponent:IsStreamingCompleted"));completed.Invoke();
            const auto step=job.Gate.Poll(ToolMapTravelTime(),valid,completed.Result<bool>());
            if(step==PS::MapTravel::Gate::Step::Expired){ToolMapTravelFinish(job.Gate.Committed?"Native map travel did not confirm arrival within 60 seconds. Check the player in game before retrying; no second teleport was issued.":"Destination did not become collision-ready within 30 seconds; the player was not moved. "+job.LastGroundError);return true;}
            if(step==PS::MapTravel::Gate::Step::Invalidated){ToolMapTravelFinish("Map travel was invalidated before completion.");return true;}
            if(job.Gate.Committed){
                ToolMapTravelProgress("Travelling","Native travel in progress; waiting for confirmed arrival...");
                const auto at=ActorHelper::GetActorLocation(pawn);
                if(!ToolMapTravelTeleporting(pawn)&&PS::MapTravel::Arrived(job.Landing,{at.X(),at.Y(),at.Z()},job.HalfHeight))
                    ToolMapTravelFinish(std::format("Teleported the selected player via native travel to X {:.0f}, Y {:.0f}, Z {:.0f}.",at.X(),at.Y(),at.Z()),true);
                return true;
            }
            if(step!=PS::MapTravel::Gate::Step::Probe){ToolMapTravelProgress("Preparing","Loading destination terrain...");return true;}
            ToolMapTravelProgress("Checking ground","Checking safe landing and player clearance...");
            try{
                std::map<std::pair<double,double>,double> heights;
                job.Landing=PS::MapLanding::ResolveNear(job.Projection,job.Point,job.Radius,job.HalfHeight,job.NearMarker,[&](PS::MapProjection::Point sample,double top,double bottom){
                    FVector hit{};std::string error;if(!UECustom::UKismetSystemLibrary::FindGroundSurface(activeWorld,FVector(sample.X,sample.Y,top),FVector(sample.X,sample.Y,bottom),{pawn},hit,error))throw std::runtime_error(error.empty()?"No blocking ground surface":error);
                    heights[{sample.X,sample.Y}]=hit.Z();return PS::MapLanding::Position{hit.X(),hit.Y(),hit.Z()};
                });
                job.GroundZ=heights.at({job.Landing.X,job.Landing.Y});
            }catch(const std::exception& error){job.LastGroundError=error.what();return true;}
            PS::SpawnToolRequests::Progress("Ground verified; starting native travel...",{{"MapPoint",{{"GroundAvailable",true},{"Location",{job.Landing.X,job.Landing.Y,job.GroundZ}}}}});
            if(ToolMapTravelTeleporting(pawn)){ToolMapTravelFinish("Another native journey started while preparing this destination; no additional teleport issued.");return true;}
            ActorHelper::FunctionCall rotation(pawn,TEXT("/Script/Engine.Actor:K2_GetActorRotation"));rotation.Invoke();const auto facing=rotation.Result<FRotator>();
            auto* service=job.Service.Get();if(!ToolMapTravelAlive(service))throw std::runtime_error("Native teleport service changed before departure");
            ActorHelper::FunctionCall teleport(service,ToolMapTravelContract(service));
            teleport.Arg(TEXT("PlayerCharacter"),pawn).JsonArg(TEXT("TeleportParams"),{{"LocationName","None"},{"TargetLocation",{{"X",job.Landing.X},{"Y",job.Landing.Y},{"Z",job.Landing.Z}}},
                {"TargetRotation",{{"Pitch",facing.GetPitch()},{"Yaw",facing.GetYaw()},{"Roll",facing.GetRoll()}}},{"bShowLoadingScreen",true},{"bNoCheck",false},{"bIsATest",false},{"TeleportationType","Teleport"},
                {"bVFXOnTeleportStart",true},{"bVFXOnTeleportEnd",true},{"bDropAnyLoadoutExclusiveEquipment",false}});
            if(!job.Gate.Commit(ToolMapTravelTime()))throw std::runtime_error("Map travel preparation expired before departure");
            // The unbound dynamic result delegate is a valid zero-initialized
            // native delegate. Completion is verified from pawn state/location,
            // never inferred from this acceptance Boolean alone.
            teleport.Invoke();if(!teleport.Result<bool>()){ToolMapTravelFinish("The game's native travel service rejected this destination; no fallback position jump was attempted.");return true;}
            ToolMapTravelProgress("Travelling","Native travel in progress; waiting for confirmed arrival...");
        }catch(const std::exception& error){ToolMapTravelFinish(std::string("Map travel stopped: ")+error.what());}
        catch(...){ToolMapTravelFinish("Map travel stopped after an unexpected native error; check the player before retrying.");}
        return true;
    }
}
