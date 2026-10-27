#pragma once
#include <cstdint>

namespace PS::Buildings {
enum class DestructionStepKind { Other, Reason, Call, Jump, Return };
struct DestructionStep {
 unsigned length=0;
 DestructionStepKind kind=DestructionStepKind::Other;
 uintptr_t target=0;
};
template<class Decode>
uintptr_t FindDestructionTarget(uintptr_t start,Decode decode){
 // A wrapper may jump to a body; that body must never scan into the next function.
 for(unsigned path=0;path<2;++path){
  const unsigned limit=path?256:128;bool reason=false;uintptr_t body=0;
  for(unsigned at=0;at<limit;){
   const auto step=decode(start+at,limit-at);
   if(!step.length||step.length>limit-at||step.kind==DestructionStepKind::Return)return 0;
   if(step.kind==DestructionStepKind::Reason)reason=true;
   if(step.kind==DestructionStepKind::Call||step.kind==DestructionStepKind::Jump){
    if(reason)return step.target;
    if(step.kind==DestructionStepKind::Jump){body=step.target;break;}
   }
   at+=step.length;
  }
  if(path||!body||body==start)return 0;
  start=body;
 }
 return 0;
}
}
