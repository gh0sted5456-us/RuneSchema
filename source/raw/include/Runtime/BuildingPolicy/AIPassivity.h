#pragma once
#include "AIEffects.h"
#include "NativeMethod.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <array>
#include <atomic>
#include <deque>
#include <unordered_set>

namespace PS::Buildings {
// Optional server-only policies wrap current native answers; no world settings or AI data are changed.
class AIPassivity {
 using Mode=PS::AIPassivityPolicy::Mode;
 struct Actor {WeakObject object;std::vector<WeakObject> effectLockers;bool locked=false,keep=true;};
 std::deque<Actor> actors;
 std::unordered_set<UObject*> enabledActors;
 AIEffects effects;
 bool effectsCatchup=false,effectsRescan=false,stopping=false;
 std::unordered_map<UObject*,std::vector<AIEffects::ModeView>> modeCache;
 std::unordered_set<UWorld*> discovered;
 std::function<void(const std::string&)> report;
 std::atomic<DWORD> gameThread{0};
 UObject* locker=nullptr;
 WeakObject setting;
 FProperty* settingTag=nullptr;
 float retry=0;
 bool querying=false,reportedLockLoss=false;
 static inline AIPassivity* current=nullptr;
 static inline std::array<std::unique_ptr<PLH::x64Detour>,3> hooks;
 static inline std::array<uint64_t,3> originals{};
 using DataQuery=bool(*)(UObject*,UObject*);
 using TagQuery=bool(*)(UObject*,const void*);
 using AttitudeQuery=unsigned char(*)(UObject*,UObject*);
 void Note(const std::string& text){if(report)report(text);}
 bool EligibleThread()const{return GetCurrentThreadId()==gameThread.load(std::memory_order_acquire)&&!querying;}
 static bool AI(UObject* actor){return AIEffects::AI(actor);}
 static UObject* ContextAI(UObject* context){
  if(!Live(context))return nullptr;if(AI(context))return context;
  if(context->IsA(Find<UClass>(L"/Script/Engine.ActorComponent"))){Call owner(context,L"GetOwner");owner.Invoke();context=owner.Result();}
  if(Live(context)&&context->IsA(Find<UClass>(L"/Script/AIModule.AIController"))){Call pawn(context,L"K2_GetPawn");pawn.Invoke();context=pawn.Result();}
  return AI(context)?context:nullptr;
 }
 using ModeCache=std::unordered_map<UObject*,std::vector<AIEffects::ModeView>>;
 Mode Evaluate(UObject* actor,const ModeCache& cache,std::vector<UObject*>* friendlyOwners=nullptr){
  Mode result=Mode::Inherit;if(const auto found=cache.find(actor);found!=cache.end())result=AIEffects::ReadModes(actor,found->second,friendlyOwners);
  if(!HasWorldPolicy(actor->GetWorld(),L"RuneSchemaAIPassivity"))return result;
  const auto position=ActorPosition(actor);
  if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("AI policy position is not finite");
  for(auto* owner:Providers(actor->GetWorld()))if(owner->GetFunctionByNameInChain(L"RuneSchemaAIPassivity"))try{
   Call call(owner,L"RuneSchemaAIPassivity");call.Copy(L"Position",&position);call.Invoke();
   result=PS::AIPassivityPolicy::Merge(result,PS::AIPassivityPolicy::Read(*static_cast<int*>(call.Data(L"ReturnValue"))));
  }catch(const std::exception& error){Note(std::string("AI_PASSIVITY_PROVIDER_FAILED ")+RC::to_string(owner->GetPathName())+": "+error.what());}
  return result;
 }
 Mode Query(UObject* actor){
  if(!EligibleThread()||!enabledActors.contains(actor)||!AI(actor))return Mode::Inherit;
  struct Guard{bool& flag;Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}}guard(querying);
  return Evaluate(actor,modeCache);
 }
 static bool ByData(UObject* context,UObject* data){
  const auto native=reinterpret_cast<DataQuery>(originals[0])(context,data);auto* self=current;
  if(!self||!self->EligibleThread())return native;
  try{if(data!=self->setting.Get())return native;if(auto* actor=ContextAI(context))return PS::AIPassivityPolicy::Passive(native,self->Query(actor));}
  catch(const std::exception& error){self->Note(std::string("AI_PASSIVITY_QUERY_FAILED ")+error.what());}return native;
 }
 static bool ByTag(UObject* context,const void* tag){
  const auto native=reinterpret_cast<TagQuery>(originals[1])(context,tag);auto* self=current;
  if(!self||!self->EligibleThread())return native;
  try{auto* data=self->setting.Get();if(!Live(data)||!tag||!self->settingTag->Identical(self->settingTag->ContainerPtrToValuePtr<void>(data),tag,0))return native;
   if(auto* actor=ContextAI(context))return PS::AIPassivityPolicy::Passive(native,self->Query(actor));}
  catch(const std::exception& error){self->Note(std::string("AI_PASSIVITY_QUERY_FAILED ")+error.what());}return native;
 }
 static unsigned char Attitude(UObject* actor,UObject* target){
  const auto native=reinterpret_cast<AttitudeQuery>(originals[2])(actor,target);auto* self=current;
  if(!self||!self->EligibleThread())return native;
  try{if(!AI(actor)||!Live(target)||!target->IsA(Find<UClass>(L"/Script/Dominion.DominionPlayerCharacter"))||target->GetWorld()!=actor->GetWorld())return native;
   return PS::AIPassivityPolicy::Attitude(native,self->Query(actor),true);}
  catch(const std::exception& error){self->Note(std::string("AI_ATTITUDE_QUERY_FAILED ")+error.what());}return native;
 }
 static void StaticQuery(UFunction* fn){
  if(!fn->HasAnyFunctionFlags(FUNC_Native)||!fn->HasAnyFunctionFlags(FUNC_Static))throw std::runtime_error("AI query must be a native static function");
  int count=0;for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++count;
  if(count!=3)throw std::runtime_error("AI query parameter count differs");
 }
 void Install(){
  if(hooks[0])return;gameThread.store(GetCurrentThreadId(),std::memory_order_release);
  auto* data=Find<UObject>(L"/Game/Gameplay/DifficultySettings/AI/DA_DifficultySetting_DisableAggressiveAI.DA_DifficultySetting_DisableAggressiveAI");
  auto* tag=CastField<FStructProperty>(Field(data->GetClassPrivate(),L"SettingTag"));
  if(!tag||tag->GetStruct()->GetPathName()!=L"/Script/GameplayTags.GameplayTag")throw std::runtime_error("Native passive setting tag differs");
  auto* byData=Find<UFunction>(L"/Script/Dominion.DifficultyFunctionLibrary:GetSettingValueAsBoolByData");
  auto* byTag=Find<UFunction>(L"/Script/Dominion.DifficultyFunctionLibrary:GetSettingValueAsBool");
  auto* attitude=Find<UFunction>(L"/Script/Dominion.DominionAiFunctionLibrary:GetAttitude");
  for(auto* fn:{byData,byTag}){StaticQuery(fn);Check(fn,L"WorldContextObject",L"ObjectProperty",sizeof(UObject*));Check(fn,L"ReturnValue",L"BoolProperty",1);}
  Check(byData,L"Setting",L"ObjectProperty",sizeof(UObject*));
  Check(byTag,L"SettingTag",L"StructProperty",tag->GetSize());
  auto* input=CastField<FStructProperty>(Field(byTag,L"SettingTag"));
  if(!input||input->GetStruct()!=tag->GetStruct()||!input->HasAnyPropertyFlags(CPF_ReferenceParm))throw std::runtime_error("Native passive query requires a referenced gameplay tag");
  StaticQuery(attitude);Check(attitude,L"AI",L"ObjectProperty",sizeof(UObject*));Check(attitude,L"Target",L"ObjectProperty",sizeof(UObject*));Check(attitude,L"ReturnValue",L"ByteProperty",1);
  auto* disable=Find<UFunction>(L"/Script/Dominion.DominionAICharacter:SetAttacksDisabled");Check(disable,L"bDisableAttacks",L"BoolProperty",1);Check(disable,L"Locker",L"ObjectProperty",sizeof(UObject*));
  const std::array<uintptr_t,3> targets{NativeMethod(byData,1),NativeMethod(byTag,1),NativeMethod(attitude,1)};
  if(std::unordered_set<uintptr_t>(targets.begin(),targets.end()).size()!=targets.size())throw std::runtime_error("Native AI query implementations overlap");
  std::array<uint64_t,3> replacements{reinterpret_cast<uint64_t>(&ByData),reinterpret_cast<uint64_t>(&ByTag),reinterpret_cast<uint64_t>(&Attitude)};
  setting=data;settingTag=tag;current=this;
  try{for(size_t i=0;i<hooks.size();++i){hooks[i]=std::make_unique<PLH::x64Detour>(targets[i],replacements[i],&originals[i]);if(!hooks[i]->hook())throw std::runtime_error("Native AI query hook could not bind");}}
  catch(...){for(auto& hook:hooks){if(hook)hook->unHook();hook.reset();}current=nullptr;throw;}
  Note("AI_PASSIVITY_BOUND version=1; native query overrides are scoped to authoritative AI actors");
 }
 static void EffectLock(UObject* actor,UObject* owner,bool disabled){
  if(!Live(owner))throw std::runtime_error("AI effect lock owner is unavailable");
  Call call(actor,L"SetAttacksDisabled");call.Text(L"bDisableAttacks",disabled?L"True":L"False");call.Object(L"Locker",owner);call.Invoke();
  if(disabled){Call actual(actor,L"AreAttacksDisabled");actual.Invoke();if(!actual.Bool())throw std::runtime_error("Native owned attack lock was not confirmed after application");}
 }
 void Lock(UObject* actor,bool disabled){
  if(disabled&&!locker){
   FStaticConstructObjectParameters parameters(Find<UClass>(L"/Script/CoreUObject.Object"),nullptr);parameters.SetFlags=RF_Transient;
   locker=UObjectGlobals::StaticConstructObject<UObject*>(parameters);if(!locker)throw std::runtime_error("AI policy lock owner could not be created");locker->SetRootSet();
  }
  if(!locker)return;
  EffectLock(actor,locker,disabled);
 }
public:
 void SetLogger(std::function<void(const std::string&)> logger){report=std::move(logger);effects.SetLogger([this](const std::string& text){Note(text);});}
 void EffectEvent(UObject* owner,UFunction* function,void* frame){
  if(stopping||GetCurrentThreadId()!=gameThread.load(std::memory_order_acquire))return;
  // Native effect application can hold component locks; defer all reflected work.
  if(owner&&owner->GetFunctionByNameInChain(L"RuneSchemaAIPassivityMode"))effectsRescan=true;
 }
 void After(UObject* actor,UFunction* function,void* frame){
  if(!hooks[0]||!frame||!EligibleThread()||!AI(actor))return;
  static const FName name(L"IsAiPassive",FNAME_Add);if(function->GetFName()!=name)return;
  const auto mode=Query(actor);if(mode==Mode::Inherit)return;
  int count=0;for(auto* p:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++count;
  auto* result=CastField<FBoolProperty>(Field(function,L"ReturnValue"));
  if(count!=1||!result||!result->HasAnyPropertyFlags(CPF_ReturnParm)||result->GetSize()!=1||result->GetOffset_Internal()<0||result->GetOffset_Internal()+result->GetSize()>function->GetParmsSize())throw std::runtime_error("AI passivity getter return contract differs");
  auto* value=result->ContainerPtrToValuePtr<void>(frame);result->SetPropertyValue(value,PS::AIPassivityPolicy::Passive(result->GetPropertyValue(value),mode));
 }
 void Begin(UObject* actor){
  if(stopping||!AI(actor))return;
  if(!HasWorldPolicy(actor->GetWorld(),L"RuneSchemaAIPassivity")&&!HasWorldPolicy(actor->GetWorld(),L"RuneSchemaAIEffectCount")&&!AIEffects::HasMode(actor))return;
  if(!AIEffects::Authority(actor))return;
  for(const auto& saved:actors)if(saved.object.Get()==actor)return;Actor saved;saved.object=actor;actors.push_back(std::move(saved));
 }
 void Update(const std::unordered_set<UWorld*>& worlds,float step){
  gameThread.store(GetCurrentThreadId(),std::memory_order_release);
  if(hooks[0]&&!Live(setting.Get()))Stop();
  if(!effectsCatchup||effectsRescan){
   if(!UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,L"/Script/Dominion.DominionAICharacter"))return;
   effectsCatchup=true;effectsRescan=false;std::vector<UObject*> existing;UObjectGlobals::FindAllOf(L"DominionAICharacter",existing);
   for(auto* actor:existing)try{Begin(actor);}catch(const std::exception& error){Note(std::string("AI_EFFECT_CATCHUP_FAILED ")+error.what());}
  }
  std::unordered_set<UWorld*> active;
  for(auto* world:worlds)if(HasWorldPolicy(world,L"RuneSchemaAIPassivity")||HasWorldPolicy(world,L"RuneSchemaAIEffectCount"))active.insert(world);
  for(auto it=discovered.begin();it!=discovered.end();)if(!active.contains(*it))it=discovered.erase(it);else ++it;
  for(auto* world:active)if(discovered.insert(world).second){std::vector<UObject*> existing;UObjectGlobals::FindAllOf(L"DominionAICharacter",existing);for(auto* actor:existing)if(Live(actor)&&actor->GetWorld()==world)Begin(actor);}
  std::vector<UObject*> targets;for(const auto& entry:actors)if(auto* actor=entry.object.Get();Live(actor))targets.push_back(actor);
  ModeCache next;std::unordered_set<UObject*> eligible;
  {
   struct Guard {bool& value;Guard(bool& flag):value(flag){value=true;}~Guard(){value=false;}} guard(querying);
   effects.Update(active,targets,step);
   for(auto* actor:targets)try{if(AIEffects::Authoritative(actor)){next.emplace(actor,AIEffects::SampleModes(actor,[this](const std::string& text){Note(text);}));eligible.insert(actor);}}catch(const std::exception& error){Note(std::string("AI_EFFECT_SAMPLE_FAILED ")+error.what());}
  }
  bool needsHooks=false;for(auto* actor:targets)if(HasWorldPolicy(actor->GetWorld(),L"RuneSchemaAIPassivity")||AIEffects::HasMode(actor)){needsHooks=true;break;}
  if(needsHooks&&!hooks[0]){retry-=step;if(retry<=0){retry=1;try{Install();}catch(const std::exception& error){Note(std::string("AI_PASSIVITY_BIND_FAILED ")+error.what());}}}
  size_t applied=0,released=0;const auto count=actors.size();
  for(size_t i=0;i<count;++i){auto* actor=actors[i].object.Get();if(!Live(actor)){actors[i].keep=false;continue;}
   try{
    std::vector<UObject*> owners;bool wanted=false;{
     struct Guard {bool& value;Guard(bool& flag):value(flag){value=true;}~Guard(){value=false;}} guard(querying);
     wanted=hooks[0]&&eligible.contains(actor)&&Evaluate(actor,next,&owners)==Mode::Friendly;
    }
    if(!wanted)owners.clear();
    auto& saved=actors[i];bool lost=false;
    if(wanted&&(saved.locked||!saved.effectLockers.empty())){Call actual(actor,L"AreAttacksDisabled");actual.Invoke();lost=!actual.Bool();}
    if(lost&&!reportedLockLoss){reportedLockLoss=true;Note("AI_ATTACK_LOCK_RECONCILED native lock state changed; restoring only owned lockers");}
    // Native effect instances remain alive through their component; do not share a transient global owner.
    for(auto* owner:owners){
     const bool held=std::any_of(saved.effectLockers.begin(),saved.effectLockers.end(),[&](const WeakObject& item){return item.Get()==owner;});
     if(!held){WeakObject item;item=owner;saved.effectLockers.push_back(item);}
     if(!held||lost){EffectLock(actor,owner,true);++applied;}
    }
    const bool legacyWanted=wanted&&owners.empty();
    if(legacyWanted&&(!saved.locked||lost)){Lock(actor,true);saved.locked=true;++applied;}
    // Acquire replacements before retiring old owners; foreign lockers are never released.
    for(auto it=saved.effectLockers.begin();it!=saved.effectLockers.end();){auto* owner=it->Get();
     if(std::find(owners.begin(),owners.end(),owner)!=owners.end()){++it;continue;}
     if(Live(owner))EffectLock(actor,owner,false);it=saved.effectLockers.erase(it);++released;
    }
    if(!legacyWanted&&saved.locked){Lock(actor,false);saved.locked=false;++released;}
    saved.keep=active.contains(actor->GetWorld())||AIEffects::HasMode(actor)||saved.locked||!saved.effectLockers.empty();
   }catch(const std::exception& error){Note(std::string("AI_PASSIVITY_UPDATE_FAILED ")+error.what());}
  }
  // Commit only after native setters/readbacks; scheduled ProcessEvent observers see a completed state.
  modeCache=std::move(next);enabledActors=std::move(eligible);
  std::erase_if(actors,[](const Actor& saved){return !saved.keep||!Live(saved.object.Get());});
  if(applied||released)Note("AI_FRIENDLY_ATTACK_LOCKS applied="+std::to_string(applied)+" released="+std::to_string(released));
 }
 void Stop()noexcept{
  stopping=true;bool held=false;
  for(auto& saved:actors)if(auto* actor=saved.object.Get();Live(actor)){
   for(auto& item:saved.effectLockers)if(auto* owner=item.Get();Live(owner))try{EffectLock(actor,owner,false);}catch(...){held=true;}
   if(saved.locked)try{Lock(actor,false);saved.locked=false;}catch(...){held=true;}
  }
  try{effects.Stop();}catch(const std::exception& error){try{Note(std::string("AI_EFFECT_REMOVE_FAILED ")+error.what());}catch(...){}}
  if(!held&&locker&&locker->IsRootSet())locker->ClearRootSet();else if(held)try{Note("AI_PASSIVITY_RESTORE_FAILED an owned native attack lock could not be released");}catch(...){}
  locker=nullptr;actors.clear();enabledActors.clear();modeCache.clear();discovered.clear();effectsCatchup=effectsRescan=false;
  for(auto& hook:hooks){if(hook)hook->unHook();hook.reset();}if(current==this)current=nullptr;setting=WeakObject{};settingTag=nullptr;retry=0;stopping=false;reportedLockLoss=false;
 }
};
}
