#include "Core/ScopedCostSwap.h"
#include <cassert>
#include <stdexcept>
#include <vector>
#include <memory>
int main(){
 std::vector<int> live{4,8,12},saved;bool waived=false;unsigned swaps=0;
 auto exchange=[&]{live.swap(saved);++swaps;};
 {
  PS::ScopedCostSwap playerA(waived,true,exchange);assert(waived&&live.empty()&&saved.size()==3);
  {PS::ScopedCostSwap nestedA(waived,true,exchange);assert(swaps==1);}
  assert(swaps==1&&live.empty());
  {
   PS::ScopedCostSwap playerB(waived,false,exchange);assert(!waived&&live==std::vector<int>({4,8,12})&&saved.empty());
   {PS::ScopedCostSwap againA(waived,true,exchange);assert(waived&&live.empty());}
   assert(!waived&&live.size()==3);
  }
  assert(waived&&live.empty()&&saved.size()==3);
 }
 assert(!waived&&live==std::vector<int>({4,8,12})&&saved.empty());
 try{PS::ScopedCostSwap failedCraft(waived,true,exchange);throw std::runtime_error("native rejection");}catch(const std::runtime_error&){}
 assert(!waived&&live.size()==3&&saved.empty());
 {const auto before=swaps;PS::ScopedCostSwap normalCraft(waived,false,exchange);assert(swaps==before&&live.size()==3);}
 bool failed=false;try{PS::ScopedCostSwap broken(waived,true,[]{throw std::runtime_error("swap failed");});}catch(const std::runtime_error&){failed=true;}
 assert(failed&&!waived&&live.size()==3);
 // Plugin cleanup unwinds outstanding scopes in reverse call order.
 std::vector<std::unique_ptr<PS::ScopedCostSwap>> pending;
 pending.push_back(std::make_unique<PS::ScopedCostSwap>(waived,true,exchange));
 pending.push_back(std::make_unique<PS::ScopedCostSwap>(waived,false,exchange));
 while(!pending.empty())pending.pop_back();
 assert(!waived&&live.size()==3&&saved.empty());
 for(unsigned i=0;i<10000;++i){PS::ScopedCostSwap repeated(waived,true,exchange);assert(live.empty());}
 assert(!waived&&live.size()==3&&saved.empty());
}
