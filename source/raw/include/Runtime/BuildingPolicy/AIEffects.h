#pragma once
#include "WorldEffects.h"
#include <unordered_set>

namespace PS::Buildings {
class AIEffects {
 using Mode=PS::AIPassivityPolicy::Mode;
 struct Slot {std::unique_ptr<WorldEffects::Effect> effect;WeakObject instigator,source;};
 struct Target {WeakObject actor;std::vector<Slot> slots;};
 struct ProviderState {WeakObject owner;std::unordered_map<UObject*,Target> targets;};
 std::unordered_map<UObject*,ProviderState> states;
 std::function<void(const std::string&)> report;
 float elapsed=0;
 static UClass* Data(UObject* effect){auto* data=Object(effect,L"Data");return data&&data->IsA<UClass>()?static_cast<UClass*>(data):nullptr;}
 static bool Declares(UObject* effect){auto* data=Data(effect);return data&&data->GetFunctionByNameInChain(L"RuneSchemaAIPassivityMode");}
 template<class F>static void Instances(UObject* actor,F visit){
  auto* component=Object(actor,L"GameplayEffectsComponent");if(!Live(component))return;
  std::vector<UObject*> instances;
  for(auto* name:{L"ReplicatedInstances",L"NonReplicatedInstances"}){
   auto* array=CastField<FArrayProperty>(Field(component->GetClassPrivate(),name));auto* inner=array?CastField<FObjectPropertyBase>(array->GetInner()):nullptr;
   if(!inner)throw std::runtime_error("Native AI effect instance array differs");
   auto* raw=array->ContainerPtrToValuePtr<FScriptArray>(component);if(raw->Num()<0||raw->Num()>512||(raw->Num()&&!raw->GetData()))throw std::runtime_error("Native AI effect array exceeds its validated bound");
   FScriptArrayHelper values(array,raw);for(int i=0;i<values.Num();++i)instances.push_back(inner->GetObjectPropertyValue(values.GetRawPtr(i)));
  }
  std::unordered_set<UObject*> seen;
  for(auto* effect:instances)if(Live(effect)&&seen.insert(effect).second&&effect->IsA(Find<UClass>(L"/Script/Dominion.DominionGameplayEffect"))&&Object(effect,L"ComponentOwner")==component)visit(component,effect);
 }
 static Mode InstanceMode(UObject* component,UObject* effect){
  if(!Declares(effect))return Mode::Inherit;
  auto* applied=CastField<FBoolProperty>(Field(effect->GetClassPrivate(),L"RuneSchemaAIPassivityApplied"));
  if(!applied||applied->GetOffset_Internal()<0||applied->GetOffset_Internal()+applied->GetSize()>effect->GetClassPrivate()->GetPropertiesSize())throw std::runtime_error("AI effect applied marker must be an instance bool");
  auto* instanceDefault=effect->GetClassPrivate()->GetClassDefaultObject().Get();
  if(!instanceDefault||applied->GetPropertyValue(applied->ContainerPtrToValuePtr<void>(instanceDefault)))throw std::runtime_error("AI effect applied marker must default to false on its instance class");
  // Cache metadata independently of activation; native flags are read at the query/reconcile boundary.
  auto* type=Data(effect);auto* data=type->GetClassDefaultObject().Get();if(!data)throw std::runtime_error("AI effect data default is unavailable");
  Call immediate(effect,L"GetActivatesImmediately");immediate.Invoke();
  if(Object(data,L"ActivationCurrentValueAttribute")||!immediate.Bool())throw std::runtime_error("AI passivity effect requires immediate main-effect activation");
  Call key(component,L"HandleFromGameplayEffect");key.Object(L"EffectInstance",effect);key.Invoke();
  auto* handle=CastField<FStructProperty>(key.Prop(L"ReturnValue"));
  if(!handle||handle->GetSize()<=0||handle->GetSize()>128||handle->GetStruct()->GetPathName()!=L"/Script/Dominion.DominionGameplayEffectHandle")throw std::runtime_error("AI effect handle contract differs");
  Call member(component,L"HasGameplayEffect");member.Copy(L"Handle",key.Data(L"ReturnValue"));member.Invoke();
  if(!member.Bool())return Mode::Inherit;
  Call mode(data,L"RuneSchemaAIPassivityMode");Check(mode.fn,L"ReturnValue",L"IntProperty",4);CheckParameters(mode.fn,1);mode.Invoke();
  return PS::AIPassivityPolicy::ActiveEffect(*static_cast<int*>(mode.Data(L"ReturnValue")),true,false,false,true,true);
 }
 static UObject* Select(UObject* owner,const wchar_t* function,Vec3 position,int index){
  Call call(owner,function);call.Copy(L"Position",&position);call.Copy(L"Index",&index);call.Invoke();return call.Result();
 }
 static void Clear(Target& target){
  std::string problem;
  for(auto& slot:target.slots)if(slot.effect)try{slot.effect->Remove();slot.effect.reset();}catch(const std::exception& error){if(problem.empty())problem=error.what();}
  if(!problem.empty())throw std::runtime_error(problem);target.slots.clear();
 }
 void Note(const std::string& text){if(report)report(text);}
public:
 struct ModeView {WeakObject instance,component,data;Mode value=Mode::Inherit;};
 static bool AI(UObject* actor){return Live(actor)&&actor->GetWorld()&&actor->IsA(Find<UClass>(L"/Script/Dominion.DominionAICharacter"));}
 static bool Authority(UObject* actor){
  if(!AI(actor))return false;
  Call server(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsServer");server.Object(L"WorldContextObject",actor);server.Invoke();if(!server.Bool())return false;
  Call authority(actor,L"HasAuthority");authority.Invoke();return authority.Bool();
 }
 static bool Authoritative(UObject* actor){
  if(!Authority(actor))return false;
  auto* health=Object(actor,L"HealthComponent");if(!Live(health))return false;Call dead(health,L"IsDead");dead.Invoke();return !dead.Bool();
 }
 static bool HasMode(UObject* actor){
  bool found=false;Instances(actor,[&](UObject*,UObject* effect){if(!found&&!Boolean(effect->GetClassPrivate(),effect,L"bWasRemoved"))found=Declares(effect);});return found;
 }
 static std::vector<ModeView> SampleModes(UObject* actor,const std::function<void(const std::string&)>& note){
  std::vector<ModeView> views;
  Instances(actor,[&](UObject* component,UObject* effect){try{
   const auto mode=InstanceMode(component,effect);if(mode==Mode::Inherit)return;
   ModeView view;view.instance=effect;view.component=component;view.data=Data(effect);view.value=mode;views.push_back(std::move(view));
  }catch(const std::exception& error){if(note)note("AI_EFFECT_MODE_FAILED "+RC::to_string(effect->GetClassPrivate()->GetPathName())+": "+error.what());}});return views;
 }
 static Mode ReadModes(UObject* actor,const std::vector<ModeView>& views,std::vector<UObject*>* friendlyOwners=nullptr){
  Mode result=Mode::Inherit;if(views.empty())return result;
  Instances(actor,[&](UObject* component,UObject* effect){for(const auto& view:views)if(view.instance.Get()==effect&&view.component.Get()==component&&view.data.Get()==Data(effect)){
   const auto mode=PS::AIPassivityPolicy::ActiveEffect(int(view.value),Boolean(effect->GetClassPrivate(),effect,L"RuneSchemaAIPassivityApplied"),Boolean(effect->GetClassPrivate(),effect,L"bIsBlocked"),Boolean(effect->GetClassPrivate(),effect,L"bWasRemoved"),true,true);
   result=PS::AIPassivityPolicy::Merge(result,mode);
   if(friendlyOwners&&mode==Mode::Friendly)friendlyOwners->push_back(effect);
   break;
  }});return result;
 }
 void SetLogger(std::function<void(const std::string&)> logger){report=std::move(logger);}
 void Update(const std::unordered_set<UWorld*>& worlds,const std::vector<UObject*>& actors,float step){
  elapsed+=step;if(elapsed<.5f)return;elapsed=0;
  std::unordered_set<UObject*> presentOwners;
  for(auto* world:worlds)for(auto* owner:Providers(world))if(owner->GetFunctionByNameInChain(L"RuneSchemaAIEffectCount")){
   presentOwners.insert(owner);
   try{
    auto& state=states[owner];if(state.owner.Get()!=owner){for(auto& [_,target]:state.targets)Clear(target);state.targets.clear();state.owner=owner;}
    Call count(owner,L"RuneSchemaAIEffectCount");count.Invoke();const int size=*static_cast<int*>(count.Data(L"ReturnValue"));if(size<0||size>64)throw std::runtime_error("AI effect count must be 0..64");
    std::unordered_set<UObject*> present;
    for(auto* actor:actors)if(actor->GetWorld()==world&&Authoritative(actor)){
     present.insert(actor);auto& target=state.targets[actor];if(target.actor.Get()!=actor){Clear(target);target.actor=actor;}
     if(target.slots.size()!=size){Clear(target);target.slots.resize(size);}
     const auto position=ActorPosition(actor);if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("AI effect position is not finite");
     for(int i=0;i<size;++i){
      auto* data=Select(owner,L"RuneSchemaAIEffectData",position,i);UObject* instigator=nullptr;UObject* source=nullptr;
      if(data){
       if(!data->IsA<UClass>()||!static_cast<UClass*>(data)->IsChildOf(Find<UClass>(L"/Script/Dominion.DominionGameplayEffectData")))throw std::runtime_error("AI provider returned a non-effect class");
       auto* defaults=static_cast<UClass*>(data)->GetClassDefaultObject().Get();
       if(!defaults||Boolean(defaults->GetClassPrivate(),defaults,L"bSingleInstance")||Boolean(defaults->GetClassPrivate(),defaults,L"bCannotBeRemoved"))throw std::runtime_error("Spatial AI effects require removable separate native instances");
       instigator=Select(owner,L"RuneSchemaAIEffectInstigator",position,i);source=Select(owner,L"RuneSchemaAIEffectSource",position,i);
       if(instigator&&(!Live(instigator)||!instigator->IsA(Find<UClass>(L"/Script/Engine.Actor"))||instigator->GetWorld()!=world))throw std::runtime_error("AI effect instigator must be a live actor in the target world or None");
       if(source&&(source->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))||(source->GetWorld()&&source->GetWorld()!=world)))throw std::runtime_error("AI effect source belongs to another world or is being destroyed");
      }
      auto& slot=target.slots[i];
      if(slot.effect&&(slot.effect->data.Get()!=data||slot.instigator.Get()!=instigator||slot.source.Get()!=source||!WorldEffects::Exists(*slot.effect))){slot.effect->Remove();slot.effect.reset();}
      if(data&&!slot.effect){slot.instigator=instigator;slot.source=source;slot.effect=WorldEffects::ApplyTo(actor,data,instigator,source);}
     }
    }
    for(auto it=state.targets.begin();it!=state.targets.end();)if(!present.contains(it->first)){Clear(it->second);it=state.targets.erase(it);}else ++it;
   }catch(const std::exception& error){Note("AI_EFFECT_SELECTION_FAILED "+RC::to_string(owner->GetClassPrivate()->GetPathName())+": "+error.what());}
  }
  for(auto it=states.begin();it!=states.end();)if(!presentOwners.contains(it->first)){
   try{for(auto& [_,target]:it->second.targets)Clear(target);it=states.erase(it);}catch(const std::exception& error){Note(std::string("AI_EFFECT_REMOVE_FAILED ")+error.what());++it;}
  }else ++it;
 }
 void Stop(){for(auto& [_,state]:states)for(auto& [__,target]:state.targets)Clear(target);states.clear();elapsed=0;}
};
}
