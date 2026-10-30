#pragma once
#include "DestructionTarget.h"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <vector>

namespace PS::Buildings {
template<class Decode>
std::vector<uintptr_t> DirectBindingTargets(uintptr_t start,unsigned limit,Decode decode){
 std::vector<uintptr_t> result;
 for(unsigned at=0;at<limit;){
  const auto step=decode(start+at,limit-at);
  if(!step.length||step.length>limit-at)break;
  if(step.kind==DestructionStepKind::Return)break;
  if(step.kind==DestructionStepKind::Call||(step.kind==DestructionStepKind::Jump&&(step.target<start||step.target-start>=limit))){
   if(!step.target)throw std::runtime_error("Native binding target is absent");
   if(std::find(result.begin(),result.end(),step.target)==result.end())result.push_back(step.target);
   if(result.size()>32)throw std::runtime_error("Native binding has too many targets");
  }
  at+=step.length;
 }
 return result;
}
inline std::size_t WaterMessageCall(std::span<const unsigned char> bytes){
 // The verified Steam/server and WinGDK variants differ only in the subsystem register.
 constexpr unsigned char pattern[]={0x48,0x8d,0x54,0x24,0x22,0x88,0x4c,0x24,0x24,0x4c,0x8b,0xc7,0x8b,0xcd,0x88,0x44,0x24,0x23,0x44,0x88,0x6c,0x24,0x22,0xe8};
 std::size_t found=0;
 for(std::size_t at=0;at+sizeof(pattern)+4<=bytes.size();++at){
  bool match=true;
  for(std::size_t i=0;i<sizeof(pattern);++i)if(i==11?(bytes[at+i]!=0xc7&&bytes[at+i]!=0xc3):bytes[at+i]!=pattern[i]){match=false;break;}
  if(match){if(found)throw std::runtime_error("Native water message path is ambiguous");found=at+sizeof(pattern)-1;}
 }
 return found;
}
}
