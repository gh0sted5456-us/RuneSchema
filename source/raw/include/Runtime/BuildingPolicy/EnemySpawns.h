#pragma once
#include "WorldEffects.h"
#include "SDK/Helper/ActorHelper.h"
#include <Unreal/AActor.hpp>
#include <Unreal/Rotator.hpp>

namespace PS::Buildings {
class EnemySpawns {
 struct Pending {WeakObject actor;bool hidden=false,collision=false,staged=false;};
 std::vector<Pending> pending;
 bool duplicating=false;
 std::function<void(const std::string&)> report;
 static bool Enemy(UObject* actor){
  auto* library=Find<UObject>(L"/Script/Dominion.Default__DominionAiFunctionLibrary");
  Call closest(library,L"GetClosestPlayer");closest.Object(L"AI",actor);closest.Invoke();
  auto* player=closest.Result();if(!Live(player))player=Find<UClass>(L"/Script/Dominion.DominionPlayerCharacter")->GetClassDefaultObject().Get();
  Call attitude(library,L"GetAttitude");attitude.Object(L"AI",actor);attitude.Object(L"Target",player);attitude.Invoke();
  if(attitude.Prop(L"ReturnValue")->GetSize()!=1)throw std::runtime_error("Native AI attitude contract differs");
  return *static_cast<unsigned char*>(attitude.Data(L"ReturnValue"))==2;
 }
 static void Visibility(UObject* actor,bool hidden,bool collision){
  Call visible(actor,L"SetActorHiddenInGame");visible.Text(L"bNewHidden",hidden?L"True":L"False");visible.Invoke();
  Call collide(actor,L"SetActorEnableCollision");collide.Text(L"bNewActorEnableCollision",collision?L"True":L"False");collide.Invoke();
 }
 static bool Navigation(UObject* source,Vec3& position,double halfHeight,bool ignore){
  if(ignore)return true;
  Call project(Find<UObject>(L"/Script/NavigationSystem.Default__NavigationSystemV1"),L"K2_ProjectPointToNavigation");
  project.Object(L"WorldContextObject",source);project.Copy(L"Point",&position);Vec3 extent{200,200,500};project.Copy(L"QueryExtent",&extent);project.Invoke();
  if(!project.Bool())return false;
  if(project.Prop(L"ProjectedLocation")->GetSize()!=sizeof(Vec3))throw std::runtime_error("AI navigation position differs");
  position=*static_cast<Vec3*>(project.Data(L"ProjectedLocation"));position.z+=halfHeight+1;return true;
 }
 bool Duplicate(UObject* source){
  auto* capsule=Object(source,L"CapsuleComponent");if(!Live(capsule))return false;
  Call radius(capsule,L"GetScaledCapsuleRadius");radius.Invoke();Call height(capsule,L"GetScaledCapsuleHalfHeight");height.Invoke();
  if(radius.Prop(L"ReturnValue")->GetSize()!=4||height.Prop(L"ReturnValue")->GetSize()!=4)throw std::runtime_error("AI capsule size contract differs");
  const double spacing=*static_cast<float*>(radius.Data(L"ReturnValue"))*2+30,halfHeight=*static_cast<float*>(height.Data(L"ReturnValue"));
  if(!std::isfinite(spacing)||spacing<1||spacing>100000||!std::isfinite(halfHeight)||halfHeight<1)return false;
  auto* info=CastField<FStructProperty>(Field(source->GetClassPrivate(),L"SpawnInfo"));if(!info)throw std::runtime_error("Native AI spawn info differs");
  auto* sourceInfo=info->ContainerPtrToValuePtr<void>(source);bool ignore=Boolean(info->GetStruct(),sourceInfo,L"bIgnoreNavmeshRequirement");
  auto origin=ActorPosition(source);
  Call rotation(source,L"K2_GetActorRotation");rotation.Invoke();Call scale(source,L"GetActorScale3D");scale.Invoke();
  if(rotation.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3)||scale.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3))throw std::runtime_error("Native AI transform differs");
  auto angles=*static_cast<Vec3*>(rotation.Data(L"ReturnValue"));auto scaling=*static_cast<Vec3*>(scale.Data(L"ReturnValue"));
  struct Guard {bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}}guard(duplicating);
  for(int attempt=0;attempt<8;++attempt){
   double angle=attempt*3.141592653589793/4;Vec3 position{origin.x+std::cos(angle)*spacing,origin.y+std::sin(angle)*spacing,origin.z};
   if(WorldScalar(source->GetWorld(),L"RuneSchemaEnemySpawnMultiplier",position,1)!=2||!Navigation(source,position,halfHeight,ignore))continue;
   try{
    auto* extra=DragonWilds::ActorHelper::SpawnActor(source->GetWorld(),source->GetClassPrivate(),FVector(position.x,position.y,position.z),FRotator(angles.x,angles.y,angles.z),[&](AActor* actor){
     actor->SetFlags(RF_Transient);Boolean(actor->GetClassPrivate(),actor,L"bSkipSpudStore",true);
     Field(actor->GetClassPrivate(),L"PowerLevel")->CopyCompleteValue_InContainer(actor,source);
     info->CopyCompleteValue_InContainer(actor,source);
     auto* copy=info->ContainerPtrToValuePtr<void>(actor);Boolean(info->GetStruct(),copy,L"bShouldRespawn",false);
     *Field(info->GetStruct(),L"Source")->ContainerPtrToValuePtr<unsigned char>(copy)=3;
     auto* id=Field(info->GetStruct(),L"SpawnSourceId");id->ClearValue(id->ContainerPtrToValuePtr<void>(copy));
     auto* spawnLocation=Field(info->GetStruct(),L"SpawnLocation");if(spawnLocation->GetSize()!=sizeof(Vec3))throw std::runtime_error("Native AI spawn location differs");spawnLocation->CopyCompleteValue(spawnLocation->ContainerPtrToValuePtr<void>(copy),&position);
    },ESpawnActorScaleMethod::OverrideRootScale,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding,FVector(scaling.x,scaling.y,scaling.z));
    if(Live(extra)){if(report)report("ENEMY_SPAWN_BONUS_APPLIED");return true;}
   }catch(const std::exception& error){if(attempt==7&&report)report(std::string("ENEMY_BONUS_PLACEMENT_FAILED ")+error.what());}
  }return false;
 }
public:
 void SetLogger(std::function<void(const std::string&)> logger){report=std::move(logger);}
 void Begin(UObject* actor){
  if(duplicating||!Live(actor)||!actor->GetWorld()||!HasWorldPolicy(actor->GetWorld(),L"RuneSchemaEnemySpawnMultiplier")||!actor->IsA(Find<UClass>(L"/Script/Dominion.DominionAICharacter")))return;
  for(auto& value:pending)if(value.actor.Get()==actor)return;
  Call authority(actor,L"HasAuthority");authority.Invoke();if(!authority.Bool())return;
  const double factor=WorldScalar(actor->GetWorld(),L"RuneSchemaEnemySpawnMultiplier",ActorPosition(actor),1);if(factor==1)return;
  if(factor!=0&&factor!=2)throw std::runtime_error("Enemy spawn multiplier must be 0, 1, or 2");
  // Wait until construction delegates have assigned player-agent allegiance.
  Pending value;value.actor=actor;value.staged=factor==0;
  if(value.staged){value.hidden=Boolean(actor->GetClassPrivate(),actor,L"bHidden");value.collision=Boolean(actor->GetClassPrivate(),actor,L"bActorEnableCollision");Visibility(actor,true,false);}
  pending.push_back(std::move(value));
 }
 void Update(){
  auto batch=std::move(pending);pending.clear();
  for(auto& entry:batch)if(auto* actor=entry.actor.Get();Live(actor)){
   try{
    const double factor=WorldScalar(actor->GetWorld(),L"RuneSchemaEnemySpawnMultiplier",ActorPosition(actor),1);
    if(factor==0&&Enemy(actor)){DragonWilds::ActorHelper::DestroyActor(static_cast<AActor*>(actor));if(report)report("HOSTILE_SPAWN_SUPPRESSED");continue;}
    if(entry.staged)Visibility(actor,entry.hidden,entry.collision);
    if(factor==2&&Enemy(actor))Duplicate(actor);
   }catch(const std::exception& error){if(entry.staged)try{Visibility(actor,entry.hidden,entry.collision);}catch(...){}if(report)report(std::string("ENEMY_SPAWN_POLICY_FAILED ")+error.what());}
  }
 }
 void Stop(){for(auto& entry:pending)if(entry.staged)if(auto* actor=entry.actor.Get();Live(actor))Visibility(actor,entry.hidden,entry.collision);pending.clear();}
};
}
