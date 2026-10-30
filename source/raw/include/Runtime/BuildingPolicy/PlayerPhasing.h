#pragma once
#include "Policy.h"
#include <unordered_map>
#include <unordered_set>
namespace PS::Buildings {
class PlayerPhasing {
 struct State {WeakObject actor,capsule,movement;unsigned char collision=0,mode=0,custom=0;};
 std::unordered_map<UObject*,State> players;
 static void Collision(UObject* capsule,unsigned char value){Call call(capsule,L"SetCollisionEnabled");call.Copy(L"NewType",&value);call.Invoke();}
 static void Mode(UObject* movement,unsigned char value,unsigned char custom=0){Call call(movement,L"SetMovementMode");call.Copy(L"NewMovementMode",&value);call.Copy(L"NewCustomMode",&custom);call.Invoke();}
 static void Restore(State& state){if(auto* capsule=state.capsule.Get();Live(capsule))Collision(capsule,state.collision);if(auto* movement=state.movement.Get();Live(movement))Mode(movement,state.mode,state.custom);}
 static bool Down(UObject* controller,const wchar_t* key){Call call(controller,L"IsInputKeyDown");call.Text(L"Key",(std::wstring(L"(KeyName=")+key+L")").c_str());call.Invoke();return call.Bool();}
public:
 void Clear(){for(auto& [_,state]:players)Restore(state);players.clear();}
 ~PlayerPhasing(){try{Clear();}catch(...){}}
 void Input(){
  for(auto& [_,state]:players)if(auto* actor=state.actor.Get();Live(actor)){
   Call local(actor,L"IsLocallyControlled");local.Invoke();if(!local.Bool())continue;Call controller(actor,L"GetController");controller.Invoke();if(!Live(controller.Result()))continue;
   const int direction=int(Down(controller.Result(),L"SpaceBar"))-int(Down(controller.Result(),L"LeftControl"));if(direction){Vec3 up{0,0,double(direction)};Call input(actor,L"AddMovementInput");input.Copy(L"WorldDirection",&up);input.Text(L"ScaleValue",L"1");input.Invoke();}
  }
 }
 void Update(UWorld* world,float){
  std::unordered_set<UObject*> present;std::vector<UObject*> characters;UObjectGlobals::FindAllOf(L"DominionPlayerCharacter",characters);
  for(auto* actor:characters)if(Live(actor)&&actor->GetWorld()==world){
   Call authority(actor,L"HasAuthority");authority.Invoke();Call local(actor,L"IsLocallyControlled");local.Invoke();if(!authority.Bool()&&!local.Bool())continue;
   auto* health=Object(actor,L"HealthComponent");if(!Live(health))continue;Call dead(health,L"IsDead");dead.Invoke();if(dead.Bool())continue;
   Call location(actor,L"K2_GetActorLocation");location.Invoke();if(!PolicyBool(world,L"RuneSchemaPlayerPhasing",*static_cast<Vec3*>(location.Data(L"ReturnValue")),false))continue;
   auto* capsule=Object(actor,L"CapsuleComponent");auto* movement=Object(actor,L"CharacterMovement");if(!Live(capsule)||!Live(movement))continue;present.insert(actor);auto found=players.find(actor);
   if(found==players.end()){
    State state;state.actor=actor;state.capsule=capsule;state.movement=movement;Call enabled(capsule,L"GetCollisionEnabled");enabled.Invoke();state.collision=*static_cast<unsigned char*>(enabled.Data(L"ReturnValue"));state.mode=*static_cast<unsigned char*>(Value(movement->GetClassPrivate(),movement,L"MovementMode"));state.custom=*static_cast<unsigned char*>(Value(movement->GetClassPrivate(),movement,L"CustomMovementMode"));found=players.emplace(actor,std::move(state)).first;
   }
   Collision(capsule,0);Mode(movement,5);
  }
  for(auto it=players.begin();it!=players.end();)if(!present.contains(it->first)){Restore(it->second);it=players.erase(it);}else ++it;
 }
};
}
