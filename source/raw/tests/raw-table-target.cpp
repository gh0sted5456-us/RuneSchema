#include "Core/RawTableTarget.h"
#include <cassert>
using namespace PS::RawTableTarget;
template<class F>bool Rejects(F&& f){try{f();return false;}catch(...){return true;}}
int main(){
    assert(Qualified("/Game/Gameplay/Items/LootDropTables/DT_LootDropTable.DT_LootDropTable"));assert(!Qualified("DT_LootDropTable"));
    assert(CommonParents({"Platform","Loot_Common","Extra_Common"})==std::vector<size_t>({1,2}));
    assert(Rejects([&]{CommonParents({"Platform"});}));
    Cursor cursor;std::vector<size_t> calls;
    cursor.Consume(2,[&](size_t i){calls.push_back(i);cursor.Consume(2,[&](size_t){assert(false);});});
    assert(calls==std::vector<size_t>({0,1}));
    cursor.Consume(2,[&](size_t){assert(false);});
    cursor.Consume(3,[&](size_t i){calls.push_back(i);});assert(calls==std::vector<size_t>({0,1,2}));
    assert(Rejects([&]{cursor.Consume(1,[](size_t){});}));
    Cursor failed;assert(Rejects([&]{failed.Consume(1,[](size_t){throw std::runtime_error("partial mutation");});}));
    failed.Consume(1,[](size_t){assert(false);}); // Never replay an additive partial mutation automatically.
}
