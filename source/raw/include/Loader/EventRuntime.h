#pragma once
#include "Loader/EventDefinition.h"
#include "Loader/TimeOfDayRuntime.h"
#include "Loader/EventIdentity.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/World.hpp"
#include "SDK/WeakObjectHandle.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Core/GroundSurface.h"
#include "Core/GroundingRetry.h"
#include "Loader/NativeAssetPrefetch.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <set>

namespace DragonWilds::Events {
class Runtime {
    using UObject=RC::Unreal::UObject;
    using UFunction=RC::Unreal::UFunction;
    using UWorld=RC::Unreal::UWorld;
    using AActor=RC::Unreal::AActor;
    std::vector<RC::StringType> warmPaths;
    double warmElapsed=0;
    struct Handle {
        UObject* Object=nullptr;int32_t Index=-1,Serial=0;
        Handle()=default;
        explicit Handle(UObject* object):Object(object) {
            using namespace RC::Unreal;
            auto* slot=object?FUObjectArray::IndexToObject(object->GetInternalIndex()):nullptr;
            if(!slot || slot->GetUObject()!=object || !slot->IsValid(false))throw std::runtime_error("Event object identity unavailable");
            const auto weak=PS::WeakObject(object);
            if(weak.Get()!=object)throw std::runtime_error("Event object generation unavailable");
            Index=weak.ObjectIndex;Serial=weak.ObjectSerialNumber;
        }
        UObject* Get()const {
            using namespace RC::Unreal;
            auto* slot=Index<0?nullptr:FUObjectArray::IndexToObject(Index);
            if(!slot || slot->GetUObject()!=Object || slot->GetSerialNumber()!=Serial || !slot->IsValid(false))return nullptr;
            if(Object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
            return Object;
        }
    };
    struct Owned {Handle Actor,Controller,Death;std::string SpawnKey;std::set<size_t> HealthStages;};
    Catalog catalog;
    Run run;
    Definition definition;
    Handle participant;
    Handle participantController;
    std::vector<Owned> owned;
    struct ClientArea {Handle Controller;std::vector<Handle> Actors;Definition Event;};
    std::map<std::string,ClientArea> clientAreas;
    double interval=0;
    double clientInterval=0;
    bool cleaning=false;
    bool participantDead=false;
    bool pendingStop=false;
    bool areaShown=false;
    bool weatherActive=false;
    bool wavePending=false,initialWavePending=false;
    PS::Grounding::Retry groundRetry;
    double actionElapsed=0;
    std::set<size_t> completedActions;
    std::string weatherToken;
    static std::string ExpandMessage(std::string text,size_t wave,size_t waves) {
        const auto replace=[&](const std::string& needle,const std::string& value){
            for(size_t at=0;(at=text.find(needle,at))!=std::string::npos;at+=value.size())text.replace(at,needle.size(),value);
        };
        replace("{wave}",std::to_string(wave+1));replace("{waves}",std::to_string(waves));return text;
    }
    bool BossWave() const {
        if(!SpawnManifest || run.Wave()>=definition.Waves.size())return false;
        for(const auto& member:definition.Waves[run.Wave()])try {
            const auto manifest=SpawnManifest(member.Spawn);
            if(manifest.is_object() && manifest.value("boss",std::string{})!="")return true;
        }catch(...) {}
        return false;
    }
    static std::optional<double> HealthPercent(UObject* actor) {
        if(!actor)return std::nullopt;
        UObject* health=nullptr;
        for(const auto* name:{TEXT("HealthComponent"),TEXT("Health Component"),TEXT("BP_Components_Health")})try {
            if(PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),name) && (health=ActorHelper::GetObjectRef(actor,name)))break;
        }catch(...) {}
        if(!health)return std::nullopt;
        try {
            ActorHelper::FunctionCall maximum(health,TEXT("/Script/Dominion.HealthComponent:GetMaxHealth"));maximum.Invoke();
            const double limit=maximum.NumericResult();
            double value=std::numeric_limits<double>::quiet_NaN();
            if(auto* current=CastField<RC::Unreal::FNumericProperty>(PropertyHelper::GetPropertyByName(health->GetClassPrivate(),TEXT("CurrentHealth")))) {
                auto* address=current->ContainerPtrToValuePtr<void>(health);
                value=current->IsFloatingPoint()?current->GetFloatingPointPropertyValue(address)
                    :static_cast<double>(current->GetSignedIntPropertyValue(address));
            } else {
                for(const auto* path:{TEXT("/Script/Dominion.HealthComponent:GetCurrentHealth"),TEXT("/Script/Dominion.HealthComponent:GetHealth")})try {
                    ActorHelper::FunctionCall call(health,path);call.Invoke();value=call.NumericResult();break;
                }catch(...) {}
            }
            if(!std::isfinite(value) || !std::isfinite(limit) || limit<=0)return std::nullopt;
            return std::clamp(value/limit*100.0,0.0,100.0);
        }catch(...) {return std::nullopt;}
    }
    void PresentHealthStages() noexcept {
        for(auto& member:owned) {
            const auto health=HealthPercent(member.Actor.Get());if(!health)continue;
            for(size_t i=0;i<definition.HealthStages.size();++i) {
                const auto& stage=definition.HealthStages[i];
                if(member.HealthStages.contains(i) || (!stage.Spawn.empty() && stage.Spawn!=member.SpawnKey) || *health>stage.Percent)continue;
                member.HealthStages.insert(i);Present(stage.Message);
            }
        }
    }
    void Failure(const std::string& message){if(ReportFailure)ReportFailure(message);else Report(message);}
    void Present(const std::string& text) noexcept {
        if(text.empty() || !Notify)return;
        try {Notify(participantController.Get(),ExpandMessage(text,run.Wave(),definition.Waves.size()),definition.Audience);}
        catch(const std::exception& e){try{Report("announcement skipped: "+std::string(e.what()));}catch(...) {}}
    }
    void HideOwnedArea() noexcept {
        if(!areaShown)return;
        try {if(SetArea)SetArea(participantController.Get(),definition,false);}
        catch(const std::exception& e){try{Failure("area cleanup failed: "+std::string(e.what()));}catch(...) {}}
        areaShown=false;
    }
    void RestoreEventWeather() noexcept {
        if(!weatherActive)return;
        try {if(RestoreWeather)RestoreWeather(participantController.Get(),weatherToken);}
        catch(const std::exception& e){try{Failure("weather restore failed: "+std::string(e.what()));}catch(...) {}}
        weatherActive=false;weatherToken.clear();
    }
    static UFunction* ZeroArgument(const TCHAR* path) {
        using namespace RC::Unreal;
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        ReflectedContract::Frame(fn);
        if(fn->GetReturnProperty())throw std::runtime_error("Event zero-argument function layout changed");
        for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Unexpected event function argument");
        return fn;
    }
    static bool DestroyOwnedActor(const Handle& handle) {
        using namespace RC::Unreal;
        auto* object=handle.Get();
        if(!object)return true;
        if(!object->IsA<AActor>())throw std::runtime_error("Event cleanup target is not an actor");
        auto* destroying=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(
            object->GetClassPrivate(),TEXT("bActorIsBeingDestroyed")));
        if(!destroying || destroying->GetArrayDim()!=1 || destroying->GetElementSize()!=1)
            throw std::runtime_error("Event actor destruction-state contract unavailable");
        if(destroying->GetPropertyValue(destroying->ContainerPtrToValuePtr<void>(object)))return true;
        auto* ai=ActorHelper::ResolveClass(TEXT("/Script/AIModule.AIController"));
        if(ai && object->IsA(ai)) {
            // Controller K2_DestroyActor may intentionally do nothing. The
            // native lifespan path performs destruction on the next world tick.
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Engine.Actor:SetLifeSpan"),false);
            auto* input=function?CastField<FFloatProperty>(function->FindProperty(FName(TEXT("InLifespan"),FNAME_Find))):nullptr;
            size_t count=0;if(function)for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
            ReflectedContract::Frame(function);
            if(function->GetReturnProperty() || count!=1 || !ReflectedContract::Input(input,function)
                || input->GetElementSize()!=sizeof(float) || input->GetArrayDim()!=1)
                throw std::runtime_error("Event controller lifespan contract unavailable");
            ActorHelper::FunctionCall(object,function->GetPathName()).Arg(TEXT("InLifespan"),0.01f).Invoke();
        } else {
            ZeroArgument(TEXT("/Script/Engine.Actor:K2_DestroyActor"));
            ActorHelper::DestroyActor(static_cast<AActor*>(object));
        }
        if(auto* remaining=handle.Get();remaining
            && !destroying->GetPropertyValue(destroying->ContainerPtrToValuePtr<void>(remaining)))
            return false;
        return true;
    }
    bool Cleanup() {
        cleaning=true;
        struct Guard {bool& Value;~Guard(){Value=false;}} guard{cleaning};
        std::string error;
        std::vector<Owned> remaining;
        remaining.reserve(owned.size());
        for(auto& item:owned) {
            bool failed=false;
            auto* actor=item.Actor.Get();
            if(actor&&DisarmDrops)try{DisarmDrops(actor);}catch(const std::exception& e){error=e.what();remaining.push_back(item);continue;}
            if(auto* controller=item.Controller.Get())try {
                auto* pawn=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
                if(!pawn || pawn==actor)failed=!DestroyOwnedActor(item.Controller);
                else error="Owned AI controller changed pawn; controller preserved";
            }catch(const std::exception& e){error=e.what();failed=true;}
            if(actor)try {if(!DestroyOwnedActor(item.Actor))failed=true;}catch(const std::exception& e){error=e.what();failed=true;}
            if(failed)remaining.push_back(item);
        }
        owned=std::move(remaining);
        if(!error.empty())throw std::runtime_error("Event cleanup: "+error);
        return owned.empty();
    }
    void SpawnWave(bool announce=true) {
        auto* player=participant.Get();if(!player)throw std::runtime_error("Event participant unavailable");
        auto* world=player->GetWorld();
        // Resolve the entire wave before the first actor is constructed. A
        // streamed-ground miss must never retry an already half-created wave.
        std::vector<RC::Unreal::FVector> positions;
        std::vector<PS::SpawnFootprint::Reservation> reservations;
        positions.reserve(definition.Waves.at(run.Wave()).size());
        std::set<std::string> validated;
        for(const auto& member:definition.Waves.at(run.Wave())) {
            if(validated.insert(member.Spawn).second)Validate(member.Spawn);
            RC::Unreal::FVector location(member.Position[0],member.Position[1],member.Position[2]);
            if(member.Ground) {
                RC::Unreal::FVector impact{};std::string error;
                PS::Grounding::QueryState groundState;
                const double anchor=member.RelativeGround?ActorHelper::GetActorLocation(static_cast<AActor*>(player)).Z():member.Position[2];
                const auto search=PS::Grounding::SearchRange::Around(anchor);
                // Bare '$' describes terrain at the spawn XY, not the floor
                // under the participant elsewhere. The player's altitude only
                // bounds discovery; it must not narrow the remote floor window.
                const std::optional<double> reference=member.RelativeGround?std::nullopt:std::optional<double>{member.Position[2]};
                const RC::Unreal::FVector start(member.Position[0],member.Position[1],search.Top);
                const RC::Unreal::FVector end(member.Position[0],member.Position[1],search.Bottom);
                if(!UECustom::UKismetSystemLibrary::FindGroundSurface(world,start,end,{static_cast<AActor*>(player)},impact,error,reference,&groundState)){
                    if(!error.empty())throw std::runtime_error("Event ground query failed: "+error);
                    if(groundState.Obstructed&&groundState.FloorReference)
                        impact=RC::Unreal::FVector(member.Position[0],member.Position[1],*groundState.FloorReference);
                    else throw PS::Grounding::SurfacePending();
                }
                // Match /spawns: keep grounded actors 10 cm clear of the
                // blocking surface, with GroundOffset as an author delta.
                location=RC::Unreal::FVector(member.Position[0],member.Position[1],impact.Z()+member.GroundOffset+10.0);
            }
            if(member.Ground){if(!PrepareGrounded)throw std::runtime_error("Event footprint provider unavailable");location=PrepareGrounded(member.Spawn,world,location,static_cast<AActor*>(player),reservations,member.GroundOffset+10.0);}
            positions.push_back(location);
        }
        owned.reserve(definition.Waves.at(run.Wave()).size());
        size_t positionIndex=0;
        for(const auto& member:definition.Waves.at(run.Wave())) {
            auto* actor=Spawn(member.Spawn,definition.Key,world,positions.at(positionIndex++),member.Ground?std::optional<double>{member.GroundOffset+10.0}:std::nullopt);
            if(!actor)throw std::runtime_error("Event spawn returned no actor");
            try {owned.push_back({Handle(actor),{}, {},member.Spawn,{}});}catch(...){ActorHelper::DestroyActor(actor);throw;}
            auto& tracking=owned.back();
            auto* controller=ActorHelper::GetObjectRef(actor,TEXT("Controller"));
            if(!controller) {
                auto* fn=ZeroArgument(TEXT("/Script/Engine.Pawn:SpawnDefaultController"));
                ActorHelper::FunctionCall(actor,fn->GetPathName()).Invoke();
                controller=ActorHelper::GetObjectRef(actor,TEXT("Controller"));
            }
            auto* controllerClass=ActorHelper::ResolveClass(TEXT("/Script/AIModule.AIController"));
            if(!controller || !controllerClass || !controller->IsA(controllerClass)
                || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=actor)throw std::runtime_error("Event AI controller ownership unavailable");
            tracking.Controller=Handle(controller);
            controller->SetFlags(RC::Unreal::RF_Transient);
            auto* death=ActorHelper::GetObjectRef(actor,TEXT("AiDeathComponent"));
            if(!death || death->GetOuterPrivate()!=actor)throw std::runtime_error("Event AI death component ownership unavailable");
            tracking.Death=Handle(death);
        }
        Report(definition.Key+": wave "+std::to_string(run.Wave()+1)+" started ("+std::to_string(owned.size())+" enemies)");
        if(announce)Present(BossWave() && !definition.Text.BossStarted.empty()?definition.Text.BossStarted:definition.Text.WaveStarted);
    }
    bool TrySpawnWave(bool initial) {
        try{SpawnWave(false);}catch(const PS::Grounding::SurfacePending&){
            wavePending=true;initialWavePending=initial;
            if(groundRetry.Missed())Report(definition.Key+": waiting for valid ground and collision-clear spawn space; no wave actors created; retry scheduled");
            return false;
        }
        wavePending=false;initialWavePending=false;groundRetry.Resolved();
        if(initial){
            if(definition.EventArea&&definition.EventArea->Visibility!=AreaVisibility::Never&&SetArea)
                areaShown=SetArea(participantController.Get(),definition,true);
            Present(definition.Text.Started);
        }
        Present(BossWave()&&!definition.Text.BossStarted.empty()?definition.Text.BossStarted:definition.Text.WaveStarted);
        return true;
    }
public:
    std::string SpawnForActor(UObject* actor)const {
        if(run.Active())for(const auto& item:owned)if(item.Actor.Get()==actor)return item.SpawnKey;
        return {};
    }
    void RequireSpawn(const std::string& event,const std::string& spawn)const {
        const auto& encounter=catalog.Find(event);
        for(const auto& wave:encounter.Waves)for(const auto& member:wave)if(member.Spawn==spawn)return;
        throw std::runtime_error("Quest SpawnID does not belong to the specified event");
    }
    std::string EventForActor(UObject* actor)const {
        if(run.Active())for(const auto& item:owned)if(item.Actor.Get()==actor)return definition.Key;
        return {};
    }
    void ConfirmVictim(UObject* actor) {
        if(!run.Active() || cleaning || !actor)return;
        for(size_t i=0;i<owned.size();++i)if(owned[i].Actor.Get()==actor){
            if(run.Death(i))Report(definition.Key+": wave "+std::to_string(run.Wave()+1)+" confirmed kills: "+std::to_string(run.ConfirmedDeaths())+"/"+std::to_string(definition.Waves.at(run.Wave()).size()));
            return;
        }
    }
    bool Active()const{return run.Active() || pendingStop || !owned.empty();}
    bool Running(const std::string& key,UWorld* world)const{
        auto* player=participant.Get();
        return run.Active() && definition.Key==key && player && player->GetWorld()==world;
    }
    bool NeedsTick()const{return Active() || !clientAreas.empty();}
    std::function<void(const std::string&)> Validate;
    std::function<Json(const std::string&)> SpawnManifest;
    Json NetworkManifest(const std::string& key)const {
        if(!SpawnManifest)throw std::runtime_error("Event spawn manifest provider unavailable");
        return BuildNetworkManifest(catalog.Find(key),SpawnManifest);
    }
    std::function<AActor*(const std::string&,const std::string&,UWorld*,const RC::Unreal::FVector&,std::optional<double>)> Spawn;
    std::function<RC::Unreal::FVector(const std::string&,UWorld*,const RC::Unreal::FVector&,AActor*,std::vector<PS::SpawnFootprint::Reservation>&,double)> PrepareGrounded;
    std::function<void(UObject*)> DisarmDrops;
    std::function<void(UObject*,const std::string&,Scope)> Notify;
    std::function<void(AActor*,const Json&)> ApplyActorCue;
    std::function<bool(UObject*,const Definition&,bool)> SetArea;
    std::function<std::string(UObject*,const std::string&)> BeginWeather;
    std::function<void(UObject*,const std::string&)> RestoreWeather;
    std::function<void(const std::string&)> Report=[](const std::string&){};
    std::function<void(const std::string&)> ReportFailure;
    void Load(const std::string& mod,const Json& data){catalog.Load(mod,data);}
    void Require(const std::string& key)const{catalog.Find(key);}
    void ObserveClientReplica(AActor* actor,const std::string& eventKey) {
        using namespace RC::Unreal;
        if(!actor || !actor->GetWorld() || eventKey.empty())return;
        const auto next=catalog.Find(eventKey);
        if(!next.EventArea || next.EventArea->Visibility==AreaVisibility::Never)return;
        auto found=clientAreas.find(eventKey);
        if(found==clientAreas.end()) {
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
            if(!type)throw std::runtime_error("Local controller class unavailable for event area");
            TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
            UObject* localController=nullptr;
            for(auto* candidate:controllers)if(candidate && candidate->GetWorld()==actor->GetWorld()
                && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
                ActorHelper::FunctionCall local(candidate,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
                if(local.Result<bool>()){if(localController)throw std::runtime_error("Local event controller is ambiguous");localController=candidate;}
            }
            if(!localController)throw std::runtime_error("Local event controller unavailable");
            if(next.EventArea && next.EventArea->Visibility!=AreaVisibility::Never && SetArea)SetArea(localController,next,true);
            found=clientAreas.emplace(eventKey,ClientArea{Handle(localController),{},next}).first;
        }
        if(std::none_of(found->second.Actors.begin(),found->second.Actors.end(),[&](const auto& handle){return handle.Get()==actor;}))
            found->second.Actors.emplace_back(actor);
    }
    std::string Start(const std::string& key,UObject* player) {
        if(Active())return "An encounter is already running. Finish or cancel it first.";
        if(!owned.empty())Cleanup();
        if(!player || !player->GetWorld() || !SpawnRuntime::GetGameMode(player) || !Validate || !Spawn)throw std::runtime_error("Event requires a local authoritative world and spawn loader");
        const auto next=catalog.Find(key);
        if(!TimeOfDay::Allows(player,next.Time))return std::string("This encounter is only available during the ")+(next.Time==TimeOfDay::Requirement::Day?"day.":"night.");
        if(next.Weather && (!BeginWeather || !RestoreWeather))throw std::runtime_error("Event weather runtime is unavailable; capture the Weather-Lifecycle diagnostic before enabling this event");
        const auto center=ActorHelper::GetActorLocation(static_cast<AActor*>(player));
        PS::AssetPreload::Plan<RC::StringType> prefetch;
        for(const auto& wave:next.Waves)for(const auto& member:wave) {
            const double x=member.Position[0]-center.X(),y=member.Position[1]-center.Y(),z=(member.RelativeGround?center.Z():member.Position[2])-center.Z();
            if(x*x+y*y+z*z>20000.0*20000.0)throw std::runtime_error("Event spawn must be within 200 meters of its participant");
            // Check all template references without synchronously loading
            // every future boss/class during the quest-acceptance callback.
            if(SpawnManifest){const auto manifest=SpawnManifest(member.Spawn);if(manifest.contains("class")&&manifest["class"].is_string())prefetch.Add(RC::to_generic_string(manifest["class"].get<std::string>()));}
            else Validate(member.Spawn);
        }
        ZeroArgument(TEXT("/Script/Engine.Pawn:SpawnDefaultController"));
        ZeroArgument(TEXT("/Script/Dominion.DeathComponent:OnHandleDeath"));
        ZeroArgument(TEXT("/Script/Dominion.AiDeathComponent:Despawn"));
        ZeroArgument(TEXT("/Script/Dominion.DominionAICharacter:BP_OnDeath"));
        auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
        if(!controller || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player)throw std::runtime_error("Event participant controller ownership unavailable");
        const Handle playerHandle(player),controllerHandle(controller);
        definition=next;participant=playerHandle;participantController=controllerHandle;participantDead=false;pendingStop=false;interval=0;actionElapsed=0;completedActions.clear();areaShown=false;weatherActive=false;weatherToken.clear();wavePending=false;initialWavePending=false;groundRetry={};run.Start(definition);
        warmPaths=prefetch.Paths();warmElapsed=0;
        try {
            if(definition.Weather){weatherToken=BeginWeather(controller,*definition.Weather);weatherActive=true;}
            PrefetchCookedAssets(prefetch);
            // Give native asynchronous I/O a bounded head start. No enemy is
            // constructed in the dialogue/quest acceptance callback.
            wavePending=true;initialWavePending=true;
        }catch(...){run.Fail();HideOwnedArea();RestoreEventWeather();try{Cleanup();}catch(const std::exception& e){Failure(e.what());}throw;}
        return "Encounter accepted. Preparing enemies and destination ground; talk to me again to cancel.";
    }
    std::string Cancel(const std::string& key,UObject* requester) {
        if(definition.Key!=key || (!run.Active() && owned.empty()))return "That encounter is not running.";
        auto* player=participant.Get();
        auto* controller=participantController.Get();
        if(!requester || requester!=player || !controller || !requester->GetWorld()
            || controller->GetWorld()!=requester->GetWorld() || !SpawnRuntime::GetGameMode(requester)
            || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=requester
            || ActorHelper::GetObjectRef(requester,TEXT("Controller"))!=controller)
            return "Only the player who started this encounter can cancel it.";
        run.Cancel();pendingStop=false;wavePending=false;groundRetry={};Present(definition.Text.Cancelled);HideOwnedArea();RestoreEventWeather();const bool removed=Cleanup();Report(definition.Key+": cancelled");
        return removed?"Encounter cancelled. Its temporary enemies have been removed.":"Encounter cancelled. Temporary enemy cleanup is finishing.";
    }
    void Reset() {
        run.Cancel();pendingStop=false;wavePending=false;initialWavePending=false;groundRetry={};HideOwnedArea();RestoreEventWeather();Cleanup();participant={};participantController={};interval=0;actionElapsed=0;completedActions.clear();
        for(auto& [key,area]:clientAreas)try{if(SetArea)SetArea(area.Controller.Get(),area.Event,false);}catch(...){}
        clientAreas.clear();clientInterval=0;
    }
    void OnDespawn(UObject* source,UFunction* function) {
        if(!run.Active() || cleaning || !source || !function)return;
        static const RC::Unreal::FName despawn(TEXT("Despawn"),RC::Unreal::FNAME_Add),destroy(TEXT("K2_DestroyActor"),RC::Unreal::FNAME_Add);
        const auto name=function->GetNamePrivate();if(name!=despawn && name!=destroy)return;
        const auto path=function->GetPathName();
        if(path!=TEXT("/Script/Dominion.AiDeathComponent:Despawn") && path!=TEXT("/Script/Engine.Actor:K2_DestroyActor"))return;
        for(size_t i=0;i<owned.size();++i)if(!run.IsDead(i) &&
            ((path==TEXT("/Script/Dominion.AiDeathComponent:Despawn") && source==owned[i].Death.Get())
             || (path==TEXT("/Script/Engine.Actor:K2_DestroyActor") && source==owned[i].Actor.Get()))) {
            if(DisarmDrops)DisarmDrops(owned[i].Actor.Get());
            run.Missing(i);pendingStop=true;return;
        }
    }
    void OnDeath(UObject* source,UFunction* function) {
        if(!run.Active() || cleaning || !source || !function)return;
        const auto name=function->GetNamePrivate();
        static const RC::Unreal::FName deathName(TEXT("OnHandleDeath"),RC::Unreal::FNAME_Add),actorName(TEXT("BP_OnDeath"),RC::Unreal::FNAME_Add),healthName(TEXT("OnDeathEvent"),RC::Unreal::FNAME_Add);
        if(name!=deathName && name!=actorName && name!=healthName)return;
        const auto path=function->GetPathName();
        if(path==TEXT("/Script/Dominion.HealthComponent:OnDeathEvent")) {
            auto* actor=source->GetOuterPrivate();
            if(actor && ActorHelper::GetObjectRef(actor,TEXT("HealthComponent"))==source){
                if(actor==participant.Get())participantDead=true;
                else ConfirmVictim(actor);
            }
            return;
        }
        if(path!=TEXT("/Script/Dominion.DeathComponent:OnHandleDeath") && path!=TEXT("/Script/Dominion.DominionAICharacter:BP_OnDeath"))return;
        if(path==TEXT("/Script/Dominion.DeathComponent:OnHandleDeath") && source->GetOuterPrivate()==participant.Get()){participantDead=true;return;}
        for(size_t i=0;i<owned.size();++i) {
            auto* actor=owned[i].Actor.Get();
            if(actor && ((path==TEXT("/Script/Dominion.DominionAICharacter:BP_OnDeath") && source==actor)
                || (path==TEXT("/Script/Dominion.DeathComponent:OnHandleDeath") && source==owned[i].Death.Get() && source->GetOuterPrivate()==actor))) {
                if(run.Death(i))Report(definition.Key+": wave "+std::to_string(run.Wave()+1)+" confirmed kills: "+std::to_string(run.ConfirmedDeaths())+"/"+std::to_string(definition.Waves.at(run.Wave()).size()));
                return;
            }
        }
    }
    void Tick(double delta) {
        if(!std::isfinite(delta) || delta<0)return;
        if(!clientAreas.empty()) {
            clientInterval+=delta;
            if(clientInterval>=0.25) {
                clientInterval=0;
                for(auto it=clientAreas.begin();it!=clientAreas.end();) {
                    std::erase_if(it->second.Actors,[](const Handle& handle){return !handle.Get();});
                    if(it->second.Actors.empty()) {
                        try{if(SetArea)SetArea(it->second.Controller.Get(),it->second.Event,false);}catch(const std::exception& e){Failure("client area cleanup failed: "+std::string(e.what()));}
                        it=clientAreas.erase(it);
                    } else ++it;
                }
            }
        }
        if(pendingStop){pendingStop=false;Present(definition.Text.Failed);HideOwnedArea();RestoreEventWeather();Cleanup();Failure(definition.Key+": despawn is not a kill; encounter stopped");return;}
        if(!run.Active()) {
            if(owned.empty())return;
            if(!std::isfinite(delta) || delta<0)return;
            interval+=delta;
            if(interval<0.25)return;
            interval=0;
            Cleanup();
            return;
        }
        run.Tick(delta);interval+=delta;if(!wavePending)actionElapsed+=delta;else {groundRetry.Advance(delta);warmElapsed+=delta;}
        std::string stopReason=run.Active()?"":"timeout or invalid elapsed time";
        if(participantDead){run.Cancel();stopReason="participant death";}
        if(run.Active() && definition.DespawnTime!=TimeOfDay::Requirement::Any
            && TimeOfDay::Allows(participantController.Get(),definition.DespawnTime)) {
            run.Cancel();pendingStop=false;Present(definition.Text.Completed);HideOwnedArea();RestoreEventWeather();
            Cleanup();Report(definition.Key+": actors retired by DespawnTimeOfDay");return;
        }
        if(run.Active() && interval<0.25)return;
        interval=0;
        for(size_t actionIndex=0;actionIndex<definition.Actions.size();++actionIndex) {
            const auto& action=definition.Actions[actionIndex];
            if(completedActions.contains(actionIndex) || actionElapsed<action.AfterSeconds)continue;
            bool matched=false;
            for(auto& item:owned)if(item.SpawnKey==action.Spawn) {
                if(auto* actor=item.Actor.Get()) {
                    if(!ApplyActorCue)throw std::runtime_error("Event actor pose runtime is unavailable");
                    ApplyActorCue(static_cast<AActor*>(actor),action.Cue);matched=true;
                }
            }
            if(matched){completedActions.insert(actionIndex);Report(definition.Key+": applied delayed actor action to "+action.Spawn);}
        }
        PresentHealthStages();
        auto* player=participant.Get();
        auto* controller=participantController.Get();
        if(!player){run.Cancel();stopReason="participant handle invalid";}
        else if(!controller){run.Cancel();stopReason="participant controller handle invalid";}
        else if(ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player || ActorHelper::GetObjectRef(player,TEXT("Controller"))!=controller){run.Cancel();stopReason="participant possession changed";}
        if(run.Active())for(size_t i=0;i<owned.size();++i)if(!owned[i].Actor.Get() && !run.IsDead(i)){run.Missing(i);stopReason="enemy "+std::to_string(i+1)+" handle invalid before confirmed death";break;}
        try {
            if(!run.Active()){Present(definition.Text.Failed);HideOwnedArea();RestoreEventWeather();Cleanup();Failure(definition.Key+": stopped: "+stopReason+"; no completion credited");return;}
            if(wavePending){
                if(initialWavePending&&warmElapsed<2.0){
                    const bool loading=std::any_of(warmPaths.begin(),warmPaths.end(),[](const auto& path){auto* object=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,path.c_str(),false);return !object||object->HasAnyFlags(static_cast<RC::Unreal::EObjectFlags>(RC::Unreal::RF_NeedLoad|RC::Unreal::RF_NeedPostLoad|RC::Unreal::RF_NeedInitialization));});
                    if(loading)return;
                }
                // Cell loading can outlive the initial retry burst. The
                // encounter timeout/cancellation bounds this wait; a ground
                // miss must not permanently suppress an otherwise valid wave.
                if(groundRetry.PlacementReady())TrySpawnWave(initialWavePending);
                return;
            }
            if(run.WaveComplete()) {
                if(!Cleanup())return;
                run.Advance();
                if(run.Active()){groundRetry={};TrySpawnWave(false);}else {Present(definition.Text.Completed);HideOwnedArea();RestoreEventWeather();Report(definition.Key+": complete");}
            }
        }catch(const std::exception& error){run.Fail();Present(definition.Text.Failed);HideOwnedArea();RestoreEventWeather();try{Cleanup();}catch(const std::exception& cleanup){Failure(cleanup.what());}Failure(definition.Key+": failed: "+error.what());}
    }
};
}
