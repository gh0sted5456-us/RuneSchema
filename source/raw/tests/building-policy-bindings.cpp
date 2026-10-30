#include "Runtime/BuildingPolicy/BindingTargets.h"
#include <cassert>
#include <map>
#include <array>
using namespace PS::Buildings;
int main(){
 using K=DestructionStepKind;
 // WinGDK's tick calls a query helper, then tail-jumps to the actual stability worker.
 const std::map<uintptr_t,DestructionStep> code{{100,{5,K::Call,400}},{105,{2,K::Other}},{107,{5,K::Jump,900}},{112,{1,K::Return}},{113,{5,K::Call,999}}};
 const auto decode=[&](uintptr_t pc,unsigned){return code.at(pc);};
 assert((DirectBindingTargets(100,512,decode)==std::vector<uintptr_t>{400,900}));
 assert((DirectBindingTargets(100,7,decode)==std::vector<uintptr_t>{400}));
 assert(DirectBindingTargets(112,20,decode).empty());
 assert(DirectBindingTargets(100,4,decode).empty());
 // Internal branches and repeated targets cannot expand the bounded search graph.
 const std::map<uintptr_t,DestructionStep> cyclic{{100,{5,K::Jump,100}},{105,{5,K::Call,400}},{110,{5,K::Call,400}},{115,{1,K::Return}}};
 assert((DirectBindingTargets(100,512,[&](uintptr_t pc,unsigned){return cyclic.at(pc);})==std::vector<uintptr_t>{400}));
 // Captured WinGDK 1.0.1.3 watering call at update+0x316; only RBX/RDI is accepted.
 std::array<unsigned char,28> water{0x48,0x8d,0x54,0x24,0x22,0x88,0x4c,0x24,0x24,0x4c,0x8b,0xc3,0x8b,0xcd,0x88,0x44,0x24,0x23,0x44,0x88,0x6c,0x24,0x22,0xe8,0x25,0xc1,0x01,0x00};
 assert(WaterMessageCall(water)==23);
 water[11]=0xc7;assert(WaterMessageCall(water)==23);
 water[11]=0xc6;assert(WaterMessageCall(water)==0);
 water[11]=0xc3;water[4]=0x23;assert(WaterMessageCall(water)==0);
 water[4]=0x22;assert(WaterMessageCall({water.data(),27})==0);
 std::vector<unsigned char> twice(water.begin(),water.end());twice.insert(twice.end(),water.begin(),water.end());
 bool bad=false;try{WaterMessageCall(twice);}catch(const std::exception&){bad=true;}assert(bad);
}
