#pragma once
#include "Destruction.h"
#include "Placement.h"
#include "Support.h"
#include "Damage.h"
#include "Overlay.h"
#include "Utility/Logging.h"
#include <Unreal/Hooks.hpp>
#include <Unreal/AActor.hpp>
#include <unordered_set>

namespace PS::Buildings {
class Bridge {
 struct WorldState { WeakObject manager;std::unique_ptr<PieceEffects> damage; };
 struct OverlayState { WeakObject provider;BuildingGlow overlay; };
 std::unordered_map<UWorld*,WorldState> worlds;
 std::unordered_map<UObject*,OverlayState> overlays;
 Hook::GlobalCallbackId begin=Hook::ERROR_ID,tick=Hook::ERROR_ID,event=Hook::ERROR_ID;
 std::unordered_set<std::string> reported;
 bool bound=false,placementBound=false;
 float elapsed=0;
 float discovery=0;
 void Note(const std::string& message){
  if(message.find("FAILED")!=std::string::npos&&!reported.insert(message).second)return;
  PS::Log<LogLevel::Normal>(STR("[BUILDING-POLICY] {}\n"),PS::ToWideSafe(message));
 }
 void Bind(){
  if(bound)return;
  auto logger=[this](const std::string& message){Note(message);};
  SupportTick::SetLogger(logger);
  try{if(!placementBound){Placement::Install(logger);placementBound=true;}DestructionGuard::Install(logger);}catch(...){DestructionGuard::Clear();throw;}
  bound=true;
  Note("NATIVE_CALLBACKS_BOUND version=1");
 }
 void Add(UObject* owner){if(Register(owner)){try{Bind();}catch(...){providers.pop_back();throw;}Note("BLUEPRINT_PROVIDER "+RC::to_string(owner->GetPathName()));}}
public:
 ~Bridge(){Stop();}
 void Start(){
  if(tick!=Hook::ERROR_ID)return;
  try{Placement::Install([this](const std::string& message){Note(message);});placementBound=true;}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}
  Hook::FCallbackOptions options{};options.OwnerModName=STR("RuneSchema");options.HookName=STR("BuildingPolicyBridge");
  begin=Hook::RegisterBeginPlayPostCallback([this](Hook::TCallbackIterationData<void>&,AActor* owner){try{Add(owner);}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}},options);
  event=Hook::RegisterProcessEventPreCallback([this](Hook::TCallbackIterationData<void>& callback,UObject* object,UFunction* fn,void* params){
   if(fn->GetName()==L"ReceiveBeginPlay"){
    try{Add(object);}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}return;
   }
   if(fn->GetName()!=L"Server_SpawnBuilding")return;
   try{if(Placement::BlockPlacement(object,fn,params))callback.PreventOriginalFunctionCall();}catch(const std::exception& e){Note(std::string("PLACEMENT_REQUEST_FAILED ")+e.what());}
  },options);
  tick=Hook::RegisterEngineTickPostCallback([this](Hook::TCallbackIterationData<void>&,UEngine*,float delta,bool){
   elapsed+=delta;if(elapsed<.1f)return;float step=elapsed;elapsed=0;
   try{
    discovery+=step;
    if(discovery>=1.f){
     discovery=0;std::vector<UObject*> actors;UObjectGlobals::FindAllOf(L"Actor",actors);
     for(auto* actor:actors)if(Live(actor)&&actor->GetFunctionByNameInChain(L"RuneSchemaBuildingPolicyVersion"))Add(actor);
    }
    std::unordered_set<UWorld*> activeWorlds;
    std::vector<UObject*> owners;
    for(auto& p:providers)if(auto* owner=p.owner.Get();Live(owner)){activeWorlds.insert(owner->GetWorld());owners.push_back(owner);}
    for(auto it=worlds.begin();it!=worlds.end();)if(!activeWorlds.contains(it->first))it=worlds.erase(it);else ++it;
    for(auto it=overlays.begin();it!=overlays.end();)if(!Live(it->second.provider.Get())){it->second.overlay.Clear();it=overlays.erase(it);}else ++it;
    for(auto* world:activeWorlds){
     RefreshPolicies(world);UObject* manager=nullptr;std::vector<UObject*> list;UObjectGlobals::FindAllOf(L"GlobalBuildingManager",list);
     for(auto* candidate:list)if(Live(candidate)&&candidate->GetWorld()==world){manager=candidate;break;}
     if(!manager)continue;Call authority(manager,L"HasAuthority");authority.Invoke();
     if(authority.Bool()){
      auto& state=worlds[world];if(state.manager.Get()!=manager){state.manager=manager;state.damage=std::make_unique<PieceEffects>();}
      state.damage->Update(manager,world);SupportTick::Update(manager);
     }
    }
    for(auto* owner:owners)if(owner->GetFunctionByNameInChain(L"RuneSchemaBuildingOverlayMaterial")){
     auto& state=overlays[owner];if(state.provider.Get()!=owner){state.overlay.Clear();state.provider=owner;}state.overlay.Update(owner,step);
    }
   }catch(const std::exception& e){Note(std::string("TICK_FAILED ")+e.what());}
  },options);
 }
 void Stop()noexcept{
  for(auto id:{begin,tick,event})if(id!=Hook::ERROR_ID)Hook::UnregisterCallback(id);begin=tick=event=Hook::ERROR_ID;
  try{DestructionGuard::Clear();Placement::Clear();SupportTick::Clear();for(auto& [_,state]:overlays)state.overlay.Clear();}catch(...){}
  providers.clear();overlays.clear();worlds.clear();bound=placementBound=false;
 }
};
}
