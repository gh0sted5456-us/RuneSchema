#pragma once
#include "Policy.h"
#include <unordered_map>
#include <unordered_set>
namespace PS::Buildings {
class PieceEffects {
 struct DamageLock {UScriptStruct* type;void* pointer;bool previous;};
 std::unordered_map<void*,DamageLock> damage;
 std::unordered_map<void*,DamageLock> health;
 UScriptStruct* lightweight=Find<UScriptStruct>(L"/Script/Dominion.LightweightBuildingPiece");
 UScriptStruct* damageType=Find<UScriptStruct>(L"/Script/Dominion.DamageNativeComponent");
 UScriptStruct* healthType=Find<UScriptStruct>(L"/Script/Dominion.HealthNativeComponent");
 void UpdateFlag(Instanced* component,bool covered,const wchar_t* field,std::unordered_map<void*,DamageLock>& locks,std::unordered_set<void*>& present){
  if(!component||!component->data)return;
  present.insert(component->data);auto found=locks.find(component->data);
  if(covered){
   if(found==locks.end())locks.emplace(component->data,DamageLock{component->type,component->data,Boolean(component->type,component->data,field)});
   Boolean(component->type,component->data,field,false);
  }else if(found!=locks.end()){
   Boolean(component->type,component->data,field,found->second.previous);locks.erase(found);
  }
 }
public:
 void Update(UObject* manager,UWorld* world){
  std::unordered_set<void*> present;
  if(Live(manager)){
   auto* stateMap=CastField<FMapProperty>(Field(manager->GetClassPrivate(),L"BuildingPieces"));
   auto* state=stateMap?CastField<FStructProperty>(stateMap->GetValueProp()):nullptr;
   if(!state||stateMap->GetKeyProp()->GetSize()!=4)throw std::runtime_error("Building state map ABI differs");
   auto* client=CastField<FStructProperty>(Field(state->GetStruct(),L"ClientVisible"));
   auto* location=Field(client->GetStruct(),L"Location");
   if(location->GetSize()!=sizeof(Vec3))throw std::runtime_error("Building location ABI differs");
   auto* resolver=Object(manager,L"LightweightBuildingPieceManager");void* iface=nullptr;
   if(Live(resolver))for(auto& implementation:resolver->GetClassPrivate()->GetInterfaces())if(implementation.Class->GetName()==L"BuildingPieceResolver")iface=reinterpret_cast<unsigned char*>(resolver)+implementation.PointerOffset;
   using Resolve=void*(*)(void*,unsigned);
   auto lookup=iface?reinterpret_cast<Resolve>((*static_cast<void***>(iface))[2]):nullptr;
   MapPairs(stateMap,manager,[&](void* key,void* value){
    unsigned id=*static_cast<unsigned*>(key);
    auto* clientValue=client->ContainerPtrToValuePtr<void>(value);
    auto position=*location->ContainerPtrToValuePtr<Vec3>(clientValue);
    if(lookup)if(auto* piece=lookup(iface,id)){
     auto actualId=*static_cast<unsigned*>(Value(lightweight,piece,L"PieceID"));
     if(actualId!=id)throw std::runtime_error("Lightweight piece resolver identity differs");
     if(!Boolean(lightweight,piece,L"bIsPreview")&&!Boolean(lightweight,piece,L"bIsGhosted")){
      bool covered=PolicyBool(world,L"RuneSchemaBlocksBuildingDamage",position,false);
      UpdateFlag(ComponentInHost(lightweight,piece,damageType),covered,L"bCanTakeDamage",damage,present);
      UpdateFlag(ComponentInHost(lightweight,piece,healthType),covered,L"bCanDie",health,present);
     }
    }
   });
  }
  std::vector<UObject*> actors;UObjectGlobals::FindAllOf(L"BaseBuildingActor",actors);
  for(auto* actor:actors)if(Live(actor)&&actor->GetWorld()==world&&!Boolean(actor->GetClassPrivate(),actor,L"bIsPreview")&&!Boolean(actor->GetClassPrivate(),actor,L"bIsGhosted")){
   Call location(actor,L"K2_GetActorLocation");location.Invoke();
   if(location.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3))throw std::runtime_error("Actor position ABI differs");
   auto* component=Object(actor,L"BaseBuildingDamageComponent");
   bool covered=PolicyBool(world,L"RuneSchemaBlocksBuildingDamage",*static_cast<Vec3*>(location.Data(L"ReturnValue")),false);
   if(Live(component))UpdateFlag(SingleComponent(component),covered,L"bCanTakeDamage",damage,present);
   auto* hp=Object(actor,L"HealthComponent");if(Live(hp))UpdateFlag(SingleComponent(hp),covered,L"bCanDie",health,present);
  }
  // Destroyed pieces are forgotten without writing through expired pointers.
  for(auto it=damage.begin();it!=damage.end();)if(!present.contains(it->first))it=damage.erase(it);else ++it;
  for(auto it=health.begin();it!=health.end();)if(!present.contains(it->first))it=health.erase(it);else ++it;
 }
};
}
