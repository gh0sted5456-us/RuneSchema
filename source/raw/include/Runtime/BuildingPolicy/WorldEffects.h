#pragma once
#include "Policy.h"
#include "Core/WorldEffectRules.h"
#include <limits>
#include <array>

namespace PS::Buildings {
inline bool HasWorldPolicy(UWorld* world,const wchar_t* callback){
 for(auto* provider:Providers(world))if(provider->GetFunctionByNameInChain(callback))return true;
 return false;
}
inline double WorldScalar(UWorld* world,const wchar_t* callback,Vec3 position,double neutral=0){
 if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("World policy position is not finite");
 double result=neutral;
 for(auto* provider:Providers(world))if(auto* fn=provider->GetFunctionByNameInChain(callback)){
  CheckPosition(fn);Check(fn,L"ReturnValue",L"DoubleProperty",8);
  Call call(provider,callback);call.Copy(L"Position",&position);call.Invoke();
  double value=*static_cast<double*>(call.Data(L"ReturnValue"));
  const auto combined=PS::WorldEffectRules::Scalar(result,value);
  if(value==0&&neutral==1)return 0;
  result=combined;
 }
 return result;
}
inline Vec3 ActorPosition(UObject* actor){
 Call location(actor,L"K2_GetActorLocation");location.Invoke();
 if(location.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3))throw std::runtime_error("World policy actor position differs");
 return *static_cast<Vec3*>(location.Data(L"ReturnValue"));
}
class WorldEffects {
 struct Effect {
  WeakObject component,data;
  UScriptStruct* type=nullptr;
  std::vector<unsigned char> handle;
  ~Effect(){if(type)type->DestroyStruct(handle.data());}
  void Remove(){if(auto* target=component.Get();Live(target)&&type){Call remove(target,L"RemoveGameplayEffect");remove.Copy(L"Handle",handle.data());remove.Invoke();}}
 };
 struct Player {WeakObject actor,provider;std::vector<std::unique_ptr<Effect>> effects;bool vitality=false;};
 struct Respawn {WeakObject component;std::array<int,4> duration;};
 struct Rested {WeakObject instance;float decay=0;};
 std::unordered_map<UObject*,std::unordered_map<UObject*,Player>> players;
 std::unordered_map<UObject*,Respawn> respawns;
 std::unordered_map<UObject*,Rested> rested;
 float elapsed=0;
 static UObject* EffectData(UObject* provider,Vec3 position,int index){
  Call call(provider,L"RuneSchemaPlayerEffectData");CheckPosition(call.fn);Check(call.fn,L"Index",L"IntProperty",4);
  auto* result=CastField<FClassProperty>(call.Prop(L"ReturnValue"));
  if(!result||result->GetSize()!=8)throw std::runtime_error("World policy effect class return differs");
  call.Copy(L"Position",&position);call.Copy(L"Index",&index);call.Invoke();auto* data=call.Result();
  if(data&&(!data->IsA<UClass>()||!static_cast<UClass*>(data)->IsChildOf(Find<UClass>(L"/Script/Dominion.DominionGameplayEffectData"))))throw std::runtime_error("World policy returned a non-effect class");
  return data;
 }
 static std::unique_ptr<Effect> Apply(UObject* provider,UObject* player,UObject* data){
  auto* component=Object(player,L"GameplayEffectsComponent");if(!Live(component))throw std::runtime_error("Player effect component is absent");
  Call create(component,L"InstantiateGameplayEffect");create.Object(L"DataClass",data);create.Text(L"bForceNewInstance",L"True");create.Invoke();
  auto* property=CastField<FStructProperty>(create.Prop(L"ReturnValue"));
  if(!property||property->GetStruct()->GetPathName()!=L"/Script/Dominion.DominionGameplayEffectHandle"||property->GetSize()>128)throw std::runtime_error("Native effect handle contract differs");
  auto effect=std::make_unique<Effect>();effect->component=component;effect->data=data;effect->handle.resize(property->GetSize());
  effect->type=property->GetStruct();effect->type->InitializeStruct(effect->handle.data());property->CopyCompleteValue(effect->handle.data(),create.Data(L"ReturnValue"));
  Call apply(component,L"ApplyGameplayEffect");apply.Object(L"Instigator",player);apply.Object(L"Source",provider);apply.Copy(L"Handle",effect->handle.data());apply.Text(L"bIgnoreChanceToApply",L"True");apply.Invoke();
  if(!apply.Bool()){effect->Remove();throw std::runtime_error("Native player effect application was rejected");}
  return effect;
 }
 static bool Exists(Effect& effect){
  auto* component=effect.component.Get();if(!Live(component))return false;
  Call call(component,L"HasGameplayEffect");call.Copy(L"Handle",effect.handle.data());call.Invoke();return call.Bool();
 }
 static void ClampCharge(UObject* player){
  auto* component=Object(player,L"SpecialChargeComponent");if(!Live(component))return;
  Call current(component,L"GetSpecialCharge");current.Invoke();Call maximum(component,L"GetMaxSpecialCharge");maximum.Invoke();
  if(current.Prop(L"ReturnValue")->GetSize()!=4||maximum.Prop(L"ReturnValue")->GetSize()!=4)throw std::runtime_error("Native special charge contract differs");
  float amount=*static_cast<float*>(current.Data(L"ReturnValue"))-*static_cast<float*>(maximum.Data(L"ReturnValue"));
  if(std::isfinite(amount)&&amount>0){Call decrease(component,L"DecreaseSpecialCharge");decrease.Copy(L"Amount",&amount);decrease.Invoke();}
 }
 static void Clear(Player& player){
  for(auto& effect:player.effects)if(effect)effect->Remove();player.effects.clear();
  if(player.vitality)if(auto* actor=player.actor.Get();Live(actor))ClampCharge(actor);player.vitality=false;
 }
 static void Refill(UObject* player,const wchar_t* componentName,const wchar_t* current,const wchar_t* maximum,const wchar_t* add,const wchar_t* amount){
  auto* component=Object(player,componentName);if(!Live(component))return;
  Call now(component,current);now.Invoke();Call max(component,maximum);max.Invoke();
  if(now.Prop(L"ReturnValue")->GetSize()!=4||max.Prop(L"ReturnValue")->GetSize()!=4)throw std::runtime_error("Native survival value contract differs");
  float delta=*static_cast<float*>(max.Data(L"ReturnValue"))-*static_cast<float*>(now.Data(L"ReturnValue"));
  if(std::isfinite(delta)&&delta>0){Call call(component,add);call.Copy(amount,&delta);call.Invoke();}
 }
 static void Needs(UObject* player){
  Refill(player,L"HydrationComponent",L"GetCurrentHydration",L"GetMaxHydration",L"AddHydration",L"HydrationToAdd");
  Refill(player,L"SustenanceComponent",L"GetCurrentSustenance",L"GetMaxSustenance",L"AddSustenance",L"SustenanceToAdd");
 }
 static float& RestDecay(UObject* instance){
  auto* field=CastField<FFloatProperty>(Field(instance->GetClassPrivate(),L"WellRestedDecayRate"));
  if(!field||field->GetSize()!=4)throw std::runtime_error("Native rested decay contract differs");
  return *field->ContainerPtrToValuePtr<float>(instance);
 }
 void Rest(UWorld* world,const std::vector<UObject*>& characters){
  std::unordered_set<UObject*> active;
  for(auto* player:characters)if(Live(player)&&player->GetWorld()==world&&WorldScalar(world,L"RuneSchemaPlayerVitality",ActorPosition(player))>0){
   Call authority(player,L"HasAuthority");authority.Invoke();if(!authority.Bool())continue;
   auto* health=Object(player,L"HealthComponent");if(!Live(health))continue;Call dead(health,L"IsDead");dead.Invoke();if(dead.Bool())continue;
   Needs(player);
   active.insert(player);auto found=rested.find(player);
   if(found!=rested.end())if(auto* instance=found->second.instance.Get();Live(instance)){
    auto& decay=RestDecay(instance);if(!std::isfinite(decay)||decay<0)throw std::runtime_error("Native rested decay is invalid");
    if(decay>0)found->second.decay=decay;decay=0;continue;
   }
   if(found!=rested.end())rested.erase(found);
   auto* component=Object(player,L"TrackedStatusEffectsComponent");if(!Live(component))continue;
   auto* array=CastField<FArrayProperty>(Field(component->GetClassPrivate(),L"TrackedStatusEffectInstances"));
   auto* object=array?CastField<FObjectPropertyBase>(array->GetInner()):nullptr;
   if(!object)throw std::runtime_error("Native tracked effect array differs");
   auto* values=array->ContainerPtrToValuePtr<FScriptArray>(component);if(values->Num()<0||values->Num()>64)throw std::runtime_error("Native tracked effect count differs");
   FScriptArrayHelper items(array,values);UObject* instance=nullptr;
   for(int i=0;i<values->Num();++i){auto* value=object->GetObjectPropertyValue(items.GetRawPtr(i));if(Live(value)&&value->IsA(Find<UClass>(L"/Script/Dominion.TrackedStatusEffectInstanceWellRested"))){instance=value;break;}}
   if(!instance)continue;
   auto* rest=Object(player,L"PlayerRestComponent");if(!Live(rest))continue;
   auto* durationField=CastField<FFloatProperty>(Field(rest->GetClassPrivate(),L"WellRestedDurationSeconds"));if(!durationField)throw std::runtime_error("Native rest duration differs");
   float duration=*durationField->ContainerPtrToValuePtr<float>(rest);if(!std::isfinite(duration)||duration<=0||duration>86400)throw std::runtime_error("Native rest duration is invalid");
   Call trigger(component,L"TriggerWellRested");trigger.Copy(L"Duration",&duration);trigger.Invoke();
   float decay=RestDecay(instance);if(!std::isfinite(decay)||decay<=0)throw std::runtime_error("Native rested decay is invalid");
   Rested saved;saved.instance=instance;saved.decay=decay;rested.emplace(player,std::move(saved));RestDecay(instance)=0;
  }
  for(auto it=rested.begin();it!=rested.end();)if(!active.contains(it->first)){
   if(auto* instance=it->second.instance.Get();Live(instance)&&RestDecay(instance)==0)RestDecay(instance)=it->second.decay;
   it=rested.erase(it);
  }else ++it;
 }
 static std::pair<UStruct*,void*> Duration(UObject* component){
  UStruct* type=component->GetClassPrivate();void* pointer=component;
  for(auto* name:{L"SpawnSettings",L"ResourceRespawnSettings",L"RespawnTime",L"RespawnDuration"}){
   auto* property=CastField<FStructProperty>(Field(type,name));if(!property)throw std::runtime_error("Resource respawn duration contract differs");
   pointer=property->ContainerPtrToValuePtr<void>(pointer);type=property->GetStruct();
  }
  for(auto* name:{L"Days",L"Hours",L"Minutes",L"Seconds"})if(Field(type,name)->GetSize()!=4)throw std::runtime_error("Resource duration scalar differs");
  return {type,pointer};
 }
 void Resources(UWorld* world){
  if(respawns.empty()&&!HasWorldPolicy(world,L"RuneSchemaResourceRespawnMultiplier"))return;
  std::vector<UObject*> list;UObjectGlobals::FindAllOf(L"ResourceRespawnComponent",list);
  std::unordered_set<UObject*> present;
  for(auto* component:list)if(Live(component)&&component->GetWorld()==world){
   Call owner(component,L"GetOwner");owner.Invoke();auto* actor=owner.Result();if(!Live(actor))continue;
   double factor=WorldScalar(world,L"RuneSchemaResourceRespawnMultiplier",ActorPosition(actor),1);
   auto found=respawns.find(component);
   if(factor==1&&found==respawns.end())continue;
   auto [type,pointer]=Duration(component);present.insert(component);
   const wchar_t* fields[]={L"Days",L"Hours",L"Minutes",L"Seconds"};
   if(found==respawns.end()){
    Respawn saved;saved.component=component;
    for(int i=0;i<4;++i)saved.duration[i]=*static_cast<int*>(Value(type,pointer,fields[i]));
    found=respawns.emplace(component,std::move(saved)).first;
   }
   if(factor==1){for(int i=0;i<4;++i)*static_cast<int*>(Value(type,pointer,fields[i]))=found->second.duration[i];respawns.erase(found);continue;}
   if(factor<1||factor>1000)throw std::runtime_error("Resource respawn factor differs");
   const auto& d=found->second.duration;
   int64_t total=((int64_t(d[0])*24+d[1])*60+d[2])*60+d[3];
   if(total<0||total>std::numeric_limits<int>::max())throw std::runtime_error("Resource respawn duration is outside contract");
   int seconds=total==0?0:std::max(1,int(std::ceil(double(total)/factor)));
   for(int i=0;i<3;++i)*static_cast<int*>(Value(type,pointer,fields[i]))=0;
   *static_cast<int*>(Value(type,pointer,L"Seconds"))=seconds;
  }
  for(auto it=respawns.begin();it!=respawns.end();)if(!Live(it->second.component.Get()))it=respawns.erase(it);else ++it;
 }
public:
 ~WorldEffects(){try{Stop();}catch(...){}}
 void Update(UWorld* world,float step){
  bool effects=false,vitality=false,resources=false;
  for(auto* provider:Providers(world)){
   effects|=provider->GetFunctionByNameInChain(L"RuneSchemaPlayerEffectCount")!=nullptr;
   vitality|=provider->GetFunctionByNameInChain(L"RuneSchemaPlayerVitality")!=nullptr;
   resources|=provider->GetFunctionByNameInChain(L"RuneSchemaResourceRespawnMultiplier")!=nullptr;
  }
  if(!effects&&!vitality&&!resources&&players.empty()&&respawns.empty()&&rested.empty())return;
  elapsed+=step;if(elapsed<.5f)return;elapsed=0;
  std::vector<UObject*> characters;if(effects||vitality||!players.empty()||!rested.empty())UObjectGlobals::FindAllOf(L"DominionPlayerCharacter",characters);
  std::unordered_set<UObject*> presentProviders;
  for(auto* provider:Providers(world))if(provider->GetFunctionByNameInChain(L"RuneSchemaPlayerEffectCount")){
   presentProviders.insert(provider);Call count(provider,L"RuneSchemaPlayerEffectCount");Check(count.fn,L"ReturnValue",L"IntProperty",4);count.Invoke();int size=*static_cast<int*>(count.Data(L"ReturnValue"));
   if(size<0||size>64)throw std::runtime_error("World policy effect count must be 0..64");
   auto& states=players[provider];std::unordered_set<UObject*> present;
   for(auto* player:characters)if(Live(player)&&player->GetWorld()==world){
    Call authority(player,L"HasAuthority");authority.Invoke();if(!authority.Bool())continue;
    auto* health=Object(player,L"HealthComponent");if(!Live(health))continue;Call dead(health,L"IsDead");dead.Invoke();if(dead.Bool())continue;
    present.insert(player);auto position=ActorPosition(player);auto& state=states[player];
    if(state.actor.Get()!=player||state.provider.Get()!=provider){Clear(state);state.actor=player;state.provider=provider;}
    if(state.effects.size()!=size){Clear(state);state.effects.resize(size);}
    for(int i=0;i<size;++i){
     auto* data=EffectData(provider,position,i);auto& effect=state.effects[i];
     if(effect&&(effect->data.Get()!=data||!Exists(*effect))){effect->Remove();effect.reset();}
     if(data&&!effect)effect=Apply(provider,player,data);
    }
    bool vitality=WorldScalar(world,L"RuneSchemaPlayerVitality",position)>0;
    if(state.vitality&&!vitality)ClampCharge(player);state.vitality=vitality;
   }
   for(auto it=states.begin();it!=states.end();)if(!present.contains(it->first)){Clear(it->second);it=states.erase(it);}else ++it;
  }
  for(auto it=players.begin();it!=players.end();)if(!presentProviders.contains(it->first)){for(auto& [_,player]:it->second)Clear(player);it=players.erase(it);}else ++it;
  if(vitality||!rested.empty())Rest(world,characters);Resources(world);
 }
 void Stop(){
  for(auto& [_,saved]:rested)if(auto* instance=saved.instance.Get();Live(instance)&&RestDecay(instance)==0)RestDecay(instance)=saved.decay;rested.clear();
  for(auto& [_,states]:players)for(auto& [__,player]:states)Clear(player);players.clear();
  for(auto& [_,saved]:respawns)if(auto* component=saved.component.Get();Live(component)){
   auto [type,pointer]=Duration(component);const wchar_t* fields[]={L"Days",L"Hours",L"Minutes",L"Seconds"};
   for(int i=0;i<4;++i)*static_cast<int*>(Value(type,pointer,fields[i]))=saved.duration[i];
  }
  respawns.clear();
 }
};
}
