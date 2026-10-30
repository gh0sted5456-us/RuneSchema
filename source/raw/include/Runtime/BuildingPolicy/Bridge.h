#pragma once
#include "Destruction.h"
#include "Placement.h"
#include "Support.h"
#include "Damage.h"
#include "Overlay.h"
#include "WorldEffects.h"
#include "Farming.h"
#include "ResourceYield.h"
#include "EnemySpawns.h"
#include "PlayerPhasing.h"
#include "AdapterState.h"
#include "CraftingCosts.h"
#include "Utility/Logging.h"
#include <Unreal/Hooks.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/World.hpp>
#include <unordered_set>

namespace PS::Buildings {
class Bridge {
 struct WorldState { WeakObject manager;std::unique_ptr<PieceEffects> damage;std::unique_ptr<WorldEffects> effects;AdapterState support,farming; };
 struct OverlayState { WeakObject provider;BuildingGlow overlay; };
 std::unordered_map<UWorld*,WorldState> worlds;
 std::unordered_map<UObject*,OverlayState> overlays;
 Hook::GlobalCallbackId begin=Hook::ERROR_ID,tick=Hook::ERROR_ID,event=Hook::ERROR_ID;
 Hook::GlobalCallbackId afterEvent=Hook::ERROR_ID;
 struct HarvestFrame {UObject* owner;UFunction* function;void* frame;float original;};
 std::vector<HarvestFrame> harvestFrames;
 EnemySpawns enemies;
 MaterialCosts materials;
 CraftingCosts crafting;
 std::unordered_map<UWorld*,std::unique_ptr<PlayerPhasing>> phasing;
 std::unordered_set<std::string> reported;
 bool bound=false,placementBound=false;
 float elapsed=0;
 bool discovered=false;
 float farmElapsed=0;
 void Note(const std::string& message){
  if(message.find("FAILED")!=std::string::npos){
   if(reported.size()>=128||!reported.insert(message).second)return;
   PS::Log<LogLevel::Warning>(STR("[RS-WLD-001] [BUILDING-POLICY] {}\n"),PS::ToWideSafe(message));return;
  }
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
 void Add(UObject* owner){
  if(!Register(owner))return;
  try{Bind();}catch(...){providers.pop_back();throw;}
  try{
   Call server(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsServer");server.Object(L"WorldContextObject",owner);server.Invoke();
   if(server.Bool()){
    if(owner->GetFunctionByNameInChain(L"RuneSchemaFarmProtection"))worlds[owner->GetWorld()].farming.Update(
     [this]{Farming::Install([this](const std::string& message){Note(message);});},
     [this](const char* error){Note(std::string("FARM_BIND_FAILED ")+error+"; optional farm protection is unavailable in this world. Native farming and other policies continue.");});
    if(owner->GetFunctionByNameInChain(L"RuneSchemaResourceYieldMultiplier"))try{ResourceYield::Install([this](const std::string& message){Note(message);});}catch(const std::exception& e){Note(std::string("RESOURCE_BIND_FAILED ")+e.what());}
   }
  }catch(const std::exception& e){Note(std::string("WORLD_AUTHORITY_FAILED ")+e.what());}
  Note("BLUEPRINT_PROVIDER "+RC::to_string(owner->GetPathName()));
 }
public:
 ~Bridge(){Stop();}
 void Start(){
  if(tick!=Hook::ERROR_ID)return;
  try{Placement::Install([this](const std::string& message){Note(message);});placementBound=true;}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}
  Hook::FCallbackOptions options{};options.OwnerModName=STR("RuneSchema");options.HookName=STR("BuildingPolicyBridge");
  enemies.SetLogger([this](const std::string& message){Note(message);});
  begin=Hook::RegisterBeginPlayPostCallback([this](Hook::TCallbackIterationData<void>&,AActor* owner){try{Add(owner);enemies.Begin(owner);}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}},options);
  event=Hook::RegisterProcessEventPreCallback([this](Hook::TCallbackIterationData<void>& callback,UObject* object,UFunction* fn,void* params){
   if(!object||!fn)return;
   static const FName beginName(L"ReceiveBeginPlay",FNAME_Add),harvestName(L"Server_TryHarvestPlot",FNAME_Add),spawnName(L"Server_SpawnBuilding",FNAME_Add);
   const auto name=fn->GetFName();
   if(CraftingCosts::Matches(name))try{crafting.Before(object,fn,params);}catch(const std::exception& e){Note(std::string("CRAFTING_MATERIAL_FAILED ")+e.what());}
   static const FName destroyName(L"CanBeDestroyedByPlayer",FNAME_Add),tryName(L"TrySpawnPieceAtLocation",FNAME_Add),hoverName(L"OnPieceHovered",FNAME_Add),selectedName(L"OnPieceSelected",FNAME_Add);
   if(name==spawnName||name==tryName||name==hoverName||name==selectedName)try{materials.Before(object,fn,params);}catch(const std::exception& e){Note(std::string("BUILDING_MATERIAL_FAILED ")+e.what());}
   if(name==destroyName){
    try{
     auto* input=CastField<FObjectPropertyBase>(Field(fn,L"DestroyingPlayerController"));if(!params||!input)return;
     auto result=OwnDismantlePermission(object,input->GetObjectPropertyValue(input->ContainerPtrToValuePtr<void>(params)));
     if(result){auto* output=Field(fn,L"ReturnValue");if(output->GetSize()!=1)throw std::runtime_error("Building dismantle result contract differs");*output->ContainerPtrToValuePtr<unsigned char>(params)=*result?0:2;callback.PreventOriginalFunctionCall();}
    }catch(const std::exception& e){Note(std::string("DISMANTLE_PERMISSION_FAILED ")+e.what());}return;
   }

   if(name==beginName){
    try{Add(object);}catch(const std::exception& e){Note(std::string("REGISTER_FAILED ")+e.what());}return;
   }
   if(name==harvestName&&fn->GetPathName()==L"/Script/Dominion.FarmCommandComponent:Server_TryHarvestPlot"){
    try{
     Call owner(object,L"GetOwner");owner.Invoke();if(!Live(owner.Result()))return;
     Call authority(owner.Result(),L"HasAuthority");authority.Invoke();if(!authority.Bool())return;
     RefreshPolicies(object->GetWorld());double multiplier=Farming::Harvest(object,fn,params);
     auto* property=CastField<FFloatProperty>(Field(fn,L"YieldMultiplier"));if(!property)throw std::runtime_error("Farm harvest multiplier contract differs");
     auto* value=property->ContainerPtrToValuePtr<float>(params);if(!std::isfinite(*value)||*value<0)return;
     double scaled=*value*multiplier;if(scaled>std::numeric_limits<float>::max())throw std::runtime_error("Farm harvest multiplier overflow");
     if(multiplier!=1){harvestFrames.push_back({object,fn,params,*value});*value=static_cast<float>(scaled);}
    }catch(const std::exception& e){Note(std::string("FARM_HARVEST_FAILED ")+e.what());}return;
   }
   if(name!=spawnName)return;
   try{if(Placement::BlockPlacement(object,fn,params))callback.PreventOriginalFunctionCall();}catch(const std::exception& e){Note(std::string("PLACEMENT_REQUEST_FAILED ")+e.what());}
  },options);
  afterEvent=Hook::RegisterProcessEventPostCallback([this](Hook::TCallbackIterationData<void>&,UObject* owner,UFunction* function,void* frame){
   if(!function)return;
   static const FName beginName(L"ReceiveBeginPlay",FNAME_Add),spawnName(L"Server_SpawnBuilding",FNAME_Add),tryName(L"TrySpawnPieceAtLocation",FNAME_Add),hoverName(L"OnPieceHovered",FNAME_Add),selectedName(L"OnPieceSelected",FNAME_Add);
   const auto name=function->GetFName();
   if(CraftingCosts::Matches(name))crafting.After(owner,function,frame);
   if(name==spawnName||name==tryName||name==hoverName||name==selectedName)materials.After(owner,function,frame);
   if(name==beginName)try{enemies.Begin(owner);}catch(const std::exception& e){Note(std::string("ENEMY_SPAWN_POLICY_FAILED ")+e.what());}
   if(harvestFrames.empty())return;
   for(size_t i=harvestFrames.size();i;--i){auto saved=harvestFrames[i-1];if(saved.owner==owner&&saved.function==function&&saved.frame==frame){*Field(function,L"YieldMultiplier")->ContainerPtrToValuePtr<float>(frame)=saved.original;harvestFrames.erase(harvestFrames.begin()+i-1);break;}}
  },options);
  tick=Hook::RegisterEngineTickPostCallback([this](Hook::TCallbackIterationData<void>&,UEngine*,float delta,bool){
   crafting.OnTick();
   for(auto& [_,state]:phasing)try{state->Input();}catch(const std::exception& e){Note(std::string("PLAYER_PHASE_INPUT_FAILED ")+e.what());}
   elapsed+=delta;if(elapsed<.1f)return;float step=elapsed;elapsed=0;
   try{
    farmElapsed+=step;
    if(!discovered){
     discovered=true;std::vector<UObject*> actors;UObjectGlobals::FindAllOf(L"Actor",actors);
     for(auto* actor:actors)if(Live(actor)&&actor->GetFunctionByNameInChain(L"RuneSchemaBuildingPolicyVersion"))Add(actor);
    }
    std::unordered_set<UWorld*> activeWorlds;
    std::unordered_set<UWorld*> authorityWorlds;
    std::vector<UObject*> owners;
    for(auto& p:providers)if(auto* owner=p.owner.Get();Live(owner)){activeWorlds.insert(owner->GetWorld());owners.push_back(owner);}
    for(auto it=phasing.begin();it!=phasing.end();)if(!activeWorlds.contains(it->first))it=phasing.erase(it);else ++it;
    for(auto* world:activeWorlds)try{if(HasWorldPolicy(world,L"RuneSchemaPlayerPhasing")||phasing.contains(world)){auto& state=phasing[world];if(!state)state=std::make_unique<PlayerPhasing>();state->Update(world,step);}}catch(const std::exception& e){Note(std::string("PLAYER_PHASE_UPDATE_FAILED ")+e.what());}
    for(auto it=worlds.begin();it!=worlds.end();)if(!activeWorlds.contains(it->first))it=worlds.erase(it);else ++it;
    for(auto it=overlays.begin();it!=overlays.end();)if(!Live(it->second.provider.Get())){it->second.overlay.Clear();it=overlays.erase(it);}else ++it;
    for(auto* world:activeWorlds){
     RefreshPolicies(world);auto& state=worlds[world];
     Call server(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsServer");server.Object(L"WorldContextObject",world);server.Invoke();
     if(server.Bool()){
      authorityWorlds.insert(world);
      try{if(!state.effects)state.effects=std::make_unique<WorldEffects>();state.effects->Update(world,step);}catch(const std::exception& e){Note(std::string("PLAYER_EFFECTS_UPDATE_FAILED ")+e.what());}
     }
     auto* manager=state.manager.Get();
     if(!Live(manager)||manager->GetWorld()!=world){
      std::vector<UObject*> list;UObjectGlobals::FindAllOf(L"GlobalBuildingManager",list);
      manager=nullptr;for(auto* candidate:list)if(Live(candidate)&&candidate->GetWorld()==world){manager=candidate;break;}
     }
     if(!manager)continue;
     if(state.manager.Get()!=manager){state.manager=manager;state.damage.reset();state.support={};}
     Call authority(manager,L"HasAuthority");authority.Invoke();
     if(authority.Bool()){
      authorityWorlds.insert(world);
      try{if(!state.damage)state.damage=std::make_unique<PieceEffects>();state.damage->Update(manager,world);}catch(const std::exception& e){Note(std::string("BUILDING_DAMAGE_UPDATE_FAILED ")+e.what());}
      state.support.Update([&]{SupportTick::Update(manager);},[this](const char* error){Note(std::string("SUPPORT_UPDATE_FAILED ")+error+"; optional support override is unavailable for this manager. Native stability and other policies continue.");});
     }
    }
    for(auto* owner:owners)if(owner->GetFunctionByNameInChain(L"RuneSchemaBuildingOverlayMaterial")){
     auto& state=overlays[owner];if(state.provider.Get()!=owner){state.overlay.Clear();state.provider=owner;}state.overlay.Update(owner,step);
    }
    if(farmElapsed>=1.f){farmElapsed=0;if(Farming::Installed()&&!authorityWorlds.empty()){std::vector<UObject*> list;UObjectGlobals::FindAllOf(L"FarmingSubsystem",list);for(auto* subsystem:list)if(Live(subsystem)&&authorityWorlds.contains(subsystem->GetWorld()))Farming::Update(subsystem);}}
    enemies.Update();
   }catch(const std::exception& e){Note(std::string("TICK_FAILED ")+e.what());}
  },options);
 }
 void Stop()noexcept{
  for(auto id:{begin,tick,event,afterEvent})if(id!=Hook::ERROR_ID)Hook::UnregisterCallback(id);begin=tick=event=afterEvent=Hook::ERROR_ID;
  for(auto& [_,state]:worlds)try{if(state.effects)state.effects->Stop();}catch(...){}
  try{enemies.Stop();ResourceYield::Stop();Farming::Stop();DestructionGuard::Clear();Placement::Clear();SupportTick::Clear();for(auto& [_,state]:overlays)state.overlay.Clear();}catch(...){}
   crafting.Clear();materials.Clear();phasing.clear();providers.clear();overlays.clear();worlds.clear();harvestFrames.clear();bound=placementBound=discovered=false;elapsed=farmElapsed=0;
 }
};
}
