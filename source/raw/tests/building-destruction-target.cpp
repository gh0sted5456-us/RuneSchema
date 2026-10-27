#include "Runtime/BuildingPolicy/DestructionTarget.h"
#include <map>
#include <stdexcept>
#include <iostream>
using namespace PS::Buildings;
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main()try{
 using K=DestructionStepKind;using Program=std::map<uintptr_t,DestructionStep>;
 const auto resolve=[](const Program& code){return FindDestructionTarget(100,[&](uintptr_t pc,unsigned){
  const auto it=code.find(pc);if(it==code.end())throw std::runtime_error("Scanned outside function");return it->second;
 });};
 // Steam 1.0.1.2: wrapper -> body -> tail jump. The following function contains an unrelated call.
 Program steam{{100,{5,K::Jump,200}},{200,{3,K::Other}},{203,{2,K::Reason}},{205,{4,K::Other}},{209,{5,K::Jump,900}},{214,{1,K::Return}},{215,{5,K::Call,999}}};
 Need(resolve(steam)==900);
 auto call=steam;call.at(209).kind=K::Call;Need(resolve(call)==900);
 Program gdk{{100,{3,K::Other}},{103,{2,K::Reason}},{105,{5,K::Jump,900}}};Need(resolve(gdk)==900);
 auto gdkCall=gdk;gdkCall.at(105).kind=K::Call;Need(resolve(gdkCall)==900);
 // Missing targets must refuse the hook, never inspect a subsequent function's call.
 auto returned=steam;returned.at(209)={1,K::Return};Need(resolve(returned)==0);
 Need(resolve({{100,{1,K::Return}},{101,{2,K::Reason}},{103,{5,K::Call,999}}})==0);
 Need(resolve({{100,{5,K::Jump,100}}})==0);
 Need(resolve({{100,{0,K::Other}}})==0);
 Need(resolve({{100,{129,K::Other}}})==0);
 Need(resolve({{100,{5,K::Jump,200}},{200,{5,K::Jump,300}}})==0);
 std::cout<<"Destruction binding accepts calls/tail jumps and refuses return-boundary fallthrough\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
