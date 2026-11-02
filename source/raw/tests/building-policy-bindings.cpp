#include "Runtime/BuildingPolicy/BindingTargets.h"
#include <cassert>
#include <map>
#include <array>
using namespace PS::Buildings;
int main(){
 // Captured WinGDK thunks split the prologue, body and epilogue into adjacent chained unwind records.
 for(const auto root:std::array{NativeUnwindPart{0x6697560,0x669756c,202919812},NativeUnwindPart{0x6697d30,0x6697d37,202866612},NativeUnwindPart{0x6697fb0,0x6697fb7,202929072}}){
  const uintptr_t body=root.Begin==0x6697560?107575495:root.Begin==0x6697d30?107577197:107578017;
  const uintptr_t finish=root.Begin==0x6697560?107575523:root.Begin==0x6697d30?107577207:107578027;
  const std::map<uintptr_t,NativeUnwindChain> parts{{root.End,{{root.End,body,1},root}},{body,{{body,finish,2},root}}};
  assert(NativeUnwindEnd(root,[&](uintptr_t end)->std::optional<NativeUnwindChain>{const auto at=parts.find(end);return at==parts.end()?std::nullopt:std::optional{at->second};})==finish);
 }
 // Steam's complete range stays intact. Neighbours from another function cannot extend it.
 const NativeUnwindPart steam{100,900,30};
 assert(NativeUnwindEnd(steam,[](uintptr_t)->std::optional<NativeUnwindChain>{return {};})==900);
 assert(NativeUnwindEnd(steam,[](uintptr_t)->std::optional<NativeUnwindChain>{return NativeUnwindChain{{900,950,40},{10,20,30}};})==900);
 for(const auto range:std::array{NativeUnwindPart{899,950,40},NativeUnwindPart{900,900,40},NativeUnwindPart{900,2200,40}}){
  bool rejected=false;try{NativeUnwindEnd(steam,[&](uintptr_t)->std::optional<NativeUnwindChain>{return NativeUnwindChain{range,steam};});}catch(const std::exception&){rejected=true;}assert(rejected);
 }
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
