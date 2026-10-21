#include "Core/HelpyScrap.h"
#include <cassert>
using namespace PS;using Json=nlohmann::json;
template<class F>void Reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
int main(){
    const Json item={{"Path","/Game/Items/Axe.Axe"},{"Count",6},{"PlayerGuid","11111111222222223333333344444444"},{"Instance",{1,2,3,4}}};
    const HelpyScrap::Recipe recipe{"/Game/Recipes/Axe.Axe",{{"/Game/Items/Wood.Wood",4},{"/Game/Items/Stone.Stone",2}},{{"/Game/Items/Axe.Axe",2}},true};
    const auto plan=HelpyScrap::Plan(item,4,{recipe});
    assert(plan["Refunds"][0]["Count"]==8&&plan["Refunds"][1]["Count"]==4&&plan["Quantity"]==4&&plan["Delivery"]=="WorldDrops");
    assert(!plan.contains("NativeItem")); // Scrap never creates a Delete recovery copy.
    Reject([&]{HelpyScrap::Plan(item,1,{recipe});});Reject([&]{HelpyScrap::Plan(item,8,{recipe});});
    Reject([&]{HelpyScrap::Plan(item,0,{recipe});});Reject([&]{HelpyScrap::Plan(item,2,{});});
    Reject([&]{HelpyScrap::Plan(item,2,{recipe,recipe});});
    auto random=recipe;random.Deterministic=false;Reject([&]{HelpyScrap::Plan(item,2,{random});});
    auto multiple=recipe;multiple.Outputs.push_back({"/Game/Items/Bonus.Bonus",1});Reject([&]{HelpyScrap::Plan(item,2,{multiple});});
    auto overflow=recipe;overflow.Inputs[0].Count=1000000;Reject([&]{HelpyScrap::Plan(item,6,{overflow});});
    auto equipped=item;equipped["Equipped"]=true;Reject([&]{HelpyScrap::Plan(equipped,2,{recipe});});
    HelpyPersonalization::Entries[HelpyPersonalization::ItemKey(item)]={{"Favorite",true}};
    Reject([&]{HelpyScrap::Plan(item,2,{recipe});});
    HelpyPersonalization::Entries=Json::object();HelpyPersonalization::Entries[HelpyPersonalization::StackKey(item)]={{"Locked",true}};
    Reject([&]{HelpyScrap::Plan(item,2,{recipe});});
}
