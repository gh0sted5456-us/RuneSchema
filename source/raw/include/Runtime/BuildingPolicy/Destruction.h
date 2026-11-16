#pragma once
#include "Policy.h"
#include "Placement.h"
#include "DestructionTarget.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <Zydis/Zydis.h>
#include <Windows.h>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>
#include <optional>
namespace PS::Buildings {
// Explicit actor opt-in replaces that actor's player dismantle policy only.
inline std::optional<bool> OwnDismantlePermission(UObject* actor,UObject* controller){
 if(!Live(actor)||!actor->GetFunctionByNameInChain(L"RuneSchemaCanDismantleOwnBuilding"))return std::nullopt;
 if(!Live(controller)||!controller->IsA(Find<UClass>(L"/Script/Dominion.DominionPlayerControllerBase")))return false;
 Call pawn(controller,L"K2_GetPawn");pawn.Invoke();if(!Live(pawn.Result()))return false;
 Call permission(actor,L"RuneSchemaCanDismantleOwnBuilding");Check(permission.fn,L"Player",L"ObjectProperty",sizeof(UObject*));Check(permission.fn,L"ReturnValue",L"BoolProperty",1);permission.Object(L"Player",pawn.Result());permission.Invoke();return permission.Bool();
}
class DestructionGuard {
 static inline std::unique_ptr<PLH::x64Detour> detour;
 static inline uint64_t trampoline=0;
 static inline std::function<void(const std::string&)> report;
 static bool AllowsPlayerDismantle(void* piece,unsigned char reason,UObject* controller){
  if(reason!=1)return false;
  auto result=OwnDismantlePermission(Placement::ActorFor(piece),controller);return result&&*result;
 }
 static bool Destroy(void* piece,unsigned char reason,UObject* controller){
  try{
   if(reason==1||reason==2||reason==3||reason==5){
    // This is the same facade GetWorld slot used by the native destruction routine.
    auto* world=reinterpret_cast<UWorld*(*)(void*)>((*static_cast<void***>(piece))[17])(piece);
    // Preview cleanup can destroy a facade before it has a transform.
    auto* transform=reinterpret_cast<const Transform*(*)(void*)>((*static_cast<void***>(piece))[20])(piece);
    if(transform&&BlocksDestruction(world,transform->location,reason)&&!AllowsPlayerDismantle(piece,reason,controller)){if(report)report("BUILDING_POLICY_DESTRUCTION_BLOCKED reason="+std::to_string(reason));return false;}

   }
  }catch(const std::exception& error){if(report)report(std::string("DEVELOPMENT_DESTRUCTION_FAILED ")+error.what());}
  return reinterpret_cast<bool(*)(void*,unsigned char,UObject*)>(trampoline)(piece,reason,controller);
 }
public:
 static void Install(std::function<void(const std::string&)> logger){
  report=std::move(logger);
  auto* fn=Find<UFunction>(L"/Script/Dominion.BaseBuildingActor:DestroyDueToDeprecation");
  const auto target=FindDestructionTarget(reinterpret_cast<uintptr_t>(fn->GetFuncPtr()),[](uintptr_t address,unsigned available){
   DestructionStep step;ZydisDisassembledInstruction ins;
   if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,address,reinterpret_cast<void*>(address),std::min(available,16u),&ins)))return step;
   step.length=ins.info.length;
   if(ins.info.mnemonic==ZYDIS_MNEMONIC_RET)step.kind=DestructionStepKind::Return;
   else if(ins.info.mnemonic==ZYDIS_MNEMONIC_MOV&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_REGISTER&&ins.operands[0].reg.value==ZYDIS_REGISTER_DL&&ins.operands[1].type==ZYDIS_OPERAND_TYPE_IMMEDIATE&&ins.operands[1].imm.value.u==6)step.kind=DestructionStepKind::Reason;
   else if((ins.info.mnemonic==ZYDIS_MNEMONIC_JMP||ins.info.mnemonic==ZYDIS_MNEMONIC_CALL)&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){
    ZyanU64 destination;if(!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],address,&destination)))return DestructionStep{};
    step.kind=ins.info.mnemonic==ZYDIS_MNEMONIC_JMP?DestructionStepKind::Jump:DestructionStepKind::Call;step.target=destination;
   }
   else if(ins.info.mnemonic==ZYDIS_MNEMONIC_JMP)step.kind=DestructionStepKind::Return;
   return step;
  });
  if(!target||Field(Find<UScriptStruct>(L"/Script/Dominion.BuildingPiece"),L"bIsBeingDestroyed")->GetOffset_Internal()!=176)throw std::runtime_error("Native building destruction path differs");
  detour=std::make_unique<PLH::x64Detour>(target,reinterpret_cast<uint64_t>(&Destroy),&trampoline);if(!detour->hook())throw std::runtime_error("Could not bind native building destruction");
 }
 static void Clear(){if(detour)detour->unHook();detour.reset();}
};
}
