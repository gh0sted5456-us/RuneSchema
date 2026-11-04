#pragma once
#include "Policy.h"
#include "SDK/WeakObjectHandle.h"
#include <unordered_map>
#include <chrono>

namespace PS::Buildings {
// Track only menus that actually queried costs; closed menus do no policy work.
class MaterialMenus {
 struct Craft {WeakObjectHandle Controller,API,Panel;bool Allowed=false,Active=false;};
 struct Build {WeakObjectHandle Controller,Panel;std::unordered_map<UObject*,WeakObjectHandle> Slots;std::vector<WeakObjectHandle> Pending;size_t Next=0;bool Allowed=false;};
 std::unordered_map<UObject*,Craft> crafting;
 std::unordered_map<UObject*,Build> building;
 bool refreshing=false;
 static bool Local(UObject* controller){if(!Live(controller)||!controller->GetWorld())return false;Call call(controller,L"IsLocalController");call.Invoke();return call.Bool();}
 static bool Open(UObject* panel){if(!Live(panel))return false;Call active(panel,L"IsActivated");active.Invoke();if(!active.Bool())return false;Call visible(panel,L"IsVisible");visible.Invoke();return visible.Bool();}
 static bool Allowed(UObject* controller,const wchar_t* policy){
  Call pawn(controller,L"K2_GetPawn");pawn.Invoke();auto* player=pawn.Result();if(!Live(player)||player->GetWorld()!=controller->GetWorld())return false;
  Call owner(player,L"GetController");owner.Invoke();if(owner.Result()!=controller)return false;
  Call position(player,L"K2_GetActorLocation");position.Invoke();Check(position.fn,L"ReturnValue",L"StructProperty",sizeof(Vec3));
  return PolicyBool(player->GetWorld(),policy,*static_cast<Vec3*>(position.Data(L"ReturnValue")),false);
 }
 static UObject* OwnedWidget(const wchar_t* name,UObject* controller){
  std::vector<UObject*> widgets;UObjectGlobals::FindAllOf(name,widgets);if(widgets.size()>64)return nullptr;
  for(auto* widget:widgets)if(Live(widget)&&widget->GetWorld()==controller->GetWorld()){
   Call owner(widget,L"GetOwningPlayer");owner.Invoke();if(owner.Result()==controller)return widget;
  }
  return nullptr;
 }
public:
 void Crafting(UObject* controller,UObject* recipe,bool allowed){
  if(refreshing||!Local(controller))return;
  auto found=crafting.find(controller);
  if(found==crafting.end()||found->second.Controller.Get()!=controller){
   if(crafting.size()>=4)return;Craft value;value.Controller.Assign(controller);
   std::vector<UObject*> apis;UObjectGlobals::FindAllOf(L"CraftingUIAPI",apis);if(apis.size()>64)return;
   for(auto* api:apis)if(Live(api)&&api->GetWorld()==controller->GetWorld()&&Object(api,L"PlayerController")==controller){value.API.Assign(api);break;}
   if(!value.API.Get())return;value.Panel.Assign(OwnedWidget(L"WBP_Crafting_MainPanel_C",controller));
   value.Allowed=allowed;found=crafting.insert_or_assign(controller,std::move(value)).first;
  }
  auto& state=found->second;auto* api=state.API.Get();if(!Live(api)||!Live(Object(api,L"CurrentStation"))||Object(api,L"CurrentCraftRecipe")!=recipe)return;
  if(!state.Panel.Get())state.Panel.Assign(OwnedWidget(L"WBP_Crafting_MainPanel_C",controller));
  if(!state.Active)state.Allowed=allowed;state.Active=true;
 }
 void Building(UObject* controller,UObject* slot,bool allowed){
  if(refreshing||!Local(controller)||!Live(slot))return;
  auto* panel=Object(slot,L"BuildingPanel");if(!Live(panel))return;
  auto found=building.find(controller);
  if(found==building.end()||found->second.Controller.Get()!=controller||found->second.Panel.Get()!=panel){
   if(found==building.end()&&building.size()>=4)return;Build value;value.Controller.Assign(controller);value.Panel.Assign(panel);value.Allowed=allowed;found=building.insert_or_assign(controller,std::move(value)).first;
  }
  auto& slots=found->second.Slots;
  if(slots.size()>=8192)std::erase_if(slots,[](const auto& entry){return !entry.second.Get();});
  if(slots.size()<8192||slots.contains(slot))slots.insert_or_assign(slot,WeakObjectHandle(slot));
 }
 void Frame(){
  if(refreshing||building.empty())return;
  const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(1);size_t work=0;
  refreshing=true;struct Reset{bool& flag;~Reset(){flag=false;}} reset{refreshing};
  for(auto& [_,state]:building){
   if(state.Next==state.Pending.size())continue;
   try{
    if(!Open(state.Panel.Get())){state.Pending.clear();state.Next=0;continue;}
    while(state.Next<state.Pending.size()&&work<64&&std::chrono::steady_clock::now()<until){
     auto* slot=state.Pending[state.Next++].Get();if(!Live(slot))continue;++work;Call update(slot,L"SetBuildableOpacity");update.Invoke();
    }
    if(state.Next==state.Pending.size()){state.Pending.clear();state.Next=0;}
   }catch(const std::exception&){state.Pending.clear();state.Next=0;}
   if(work>=64||std::chrono::steady_clock::now()>=until)break;
  }
 }
 template<class Report>void Update(Report report){
  if(refreshing||crafting.empty()&&building.empty())return;refreshing=true;struct Reset{bool& flag;~Reset(){flag=false;}} reset{refreshing};
  for(auto it=crafting.begin();it!=crafting.end();){auto& state=it->second;auto* controller=state.Controller.Get();auto* api=state.API.Get();
   try{
    if(!Local(controller)||!Live(api)||!Live(Object(api,L"CurrentStation"))){it=crafting.erase(it);continue;}
    if(!state.Active||!Open(state.Panel.Get())){it=crafting.erase(it);continue;}
    const bool allowed=Allowed(controller,L"RuneSchemaFreeCraftingMaterials");
    if(allowed!=state.Allowed){auto* recipe=Object(api,L"CurrentCraftRecipe");
     if(Live(recipe)){auto* field=Field(api->GetClassPrivate(),L"CurrentCraftRecipe");auto* data=field->ContainerPtrToValuePtr<void>(api);
      WriteObject(field,data,nullptr);
      try{Call select(api,L"SelectRecipe");select.Object(L"SelectedCraftRecipe",recipe);select.Invoke();}
      catch(...){WriteObject(field,data,recipe);throw;}
      if(!Object(api,L"CurrentCraftRecipe"))WriteObject(field,data,recipe);
     }
     state.Allowed=allowed;
    }
   }catch(const std::exception& error){report(std::string("CRAFTING_MENU_REFRESH_FAILED ")+error.what());it=crafting.erase(it);continue;}
   ++it;
  }
  for(auto it=building.begin();it!=building.end();){auto& state=it->second;auto* controller=state.Controller.Get();
   try{
    if(!Local(controller)||!Open(state.Panel.Get())){it=building.erase(it);continue;}
    const bool allowed=Allowed(controller,L"RuneSchemaFreeBuildingMaterials");
    if(allowed!=state.Allowed){state.Pending.clear();state.Pending.reserve(state.Slots.size());for(const auto& [_,weak]:state.Slots)state.Pending.push_back(weak);state.Next=0;state.Allowed=allowed;}
    std::erase_if(state.Slots,[](const auto& entry){return !entry.second.Get();});
   }catch(const std::exception& error){report(std::string("BUILDING_MENU_REFRESH_FAILED ")+error.what());it=building.erase(it);continue;}
   ++it;
  }
  refreshing=false;
 }
 void Clear(){crafting.clear();building.clear();refreshing=false;}
};
}
