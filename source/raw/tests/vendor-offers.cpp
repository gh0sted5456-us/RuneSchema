#include "Loader/VendorOffers.h"
#include "Loader/VendorCategoryGate.h"
#include <cassert>
#include <map>
#include <vector>
#include <algorithm>
using nlohmann::json;
template<class F>void Rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main() {
    using namespace DragonWilds::VendorOffers;
    json orderedOffers=json::array({
        {{"Item","/Tools/late"},{"Currency","/Coin"},{"Price",1},{"Category","Tools"},{"Order",2},{"Tag","late"}},
        {{"Item","/Other/first"},{"Currency","/Coin"},{"Price",1},{"Category","Other"},{"Order",0},{"Tag","other"}},
        {{"Item","/Tools/first"},{"Currency","/Coin"},{"Price",1},{"Category","Tools"},{"Order",0},{"Tag","first"}},
        {{"Item","/Tools/source-order"},{"Currency","/Coin"},{"Price",1},{"Category","Tools"},{"Tag","default"}}
    });
    const auto ordered=OrderedWithinCategories(orderedOffers);
    assert(ordered[0]["Tag"]=="first"&&ordered[1]["Tag"]=="late"&&ordered[2]["Tag"]=="default");
    assert(ordered[3]["Tag"]=="other");
    auto invalidOrder=orderedOffers[0];invalidOrder["Order"]=-1;
    Rejects([&]{(void)Properties(invalidOrder);});
    const auto owner=Owner("TestMod","CowA");
    const auto id=Identity(owner,"0");
    assert(id.size()==22 && std::string("AQgw").find(id.back())!=std::string::npos);
    assert(id==Identity(owner,"0"));
    assert(id!=Identity(Owner("TestMod","CowB"),"0"));
    assert(id!=Identity(Owner("OtherMod","CowA"),"0"));
    assert(id!=Identity(owner,"1"));
    assert(Owner("a:b","c")!=Owner("a","b:c"));
    assert(RecipeObjectName(id)=="RSVendor_"+id+"_Runtime");
    Rejects([&]{RecipeObjectName("");});
    Rejects([&]{RecipeObjectName(std::string(22,'.'));});
    std::vector<std::pair<std::string,std::string>> offers{{Owner("TestMod","Cow"),"0"},{Owner("TestMod","Cow"),"1"},{Owner("TestMod","Granite"),"0"},{Owner("OtherMod","Cow"),"0"}};
    const auto build=[](const auto& definitions){
        std::map<std::string,std::string> names;
        for(const auto& [owner,slot]:definitions) {
            const auto identity=Identity(owner,slot);
            names.emplace(identity,"/Engine/Transient."+RecipeObjectName(identity));
        }
        return names;
    };
    const auto server=build(offers);
    std::reverse(offers.begin(),offers.end());
    assert(build(offers)==server);
    assert(server.size()==offers.size());
    offers.erase(offers.begin()+1);
    for(const auto& [identity,path]:build(offers))assert(server.at(identity)==path);
    json item={{"Item","/Game/Cabbage.Cabbage"},{"Currency","/Game/Coin.Coin"},{"Price",10},{"Count",5},{"Category","Food"}};
    const auto properties=Properties(item);
    assert(properties["ItemsCreated"][0]["ItemData"]==item["Item"]);
    assert(properties["ItemsCreated"][0]["Count"]==5);
    assert(properties["ItemsConsumed"][0]["ItemData"]==item["Currency"]);
    assert(properties["ItemsConsumed"][0]["Count"]==10);
    assert(Category(item)=="Food");
    item.erase("Count");assert(Properties(item)["ItemsCreated"][0]["Count"]==1);
    for(auto invalid:{json(-1),json(1.5),json("1"),json(uint64_t(1)<<63)}) {
        auto bad=item;bad["Price"]=invalid;Rejects([&]{Properties(bad);});
    }
    auto bad=item;bad["Count"]=0;Rejects([&]{Properties(bad);});
    bad=item;bad.erase("Currency");Rejects([&]{Properties(bad);});
    bad=item;bad.erase("Price");Rejects([&]{Properties(bad);});
    bad=item;bad["Item"]="not an asset";Rejects([&]{Properties(bad);});

    // Category gates remove whole offers without changing the category identity
    // or relative order of any surviving offer. Hidden items must never float
    // into the next visible native LabeledRecipes group.
    json gated=json::array({
        {{"Item","/Game/General.General"},{"Currency","/Game/Coin.Coin"},{"Price",1},{"Category","General"},{"Tag","general"}},
        {{"Item","/Game/DayA.DayA"},{"Currency","/Game/Coin.Coin"},{"Price",1},{"Category","Day"},{"Order",2},{"Tag","day-late"}},
        {{"Item","/Game/Night.Night"},{"Currency","/Game/Coin.Coin"},{"Price",1},{"Category","Night"},{"Tag","night"}},
        {{"Item","/Game/DayB.DayB"},{"Currency","/Game/Coin.Coin"},{"Price",1},{"Category","Day"},{"Order",0},{"Tag","day-first"}},
        {{"Item","/Game/Elite.Elite"},{"Currency","/Game/Coin.Coin"},{"Price",1},{"Category","Elite"},{"Tag","elite"}}
    });
    const auto rules=DragonWilds::VendorCategoryGate::Parse(json::array({
        {{"Category","Day"},{"TimeOfDay","Day"}},
        {{"Category","Night"},{"TimeOfDay","Night"}},
        {{"Category","Elite"},{"MinPowerLevel",50}}
    }));
    const auto day=OrderedWithinCategories(DragonWilds::VendorCategoryGate::Filter(
        gated,rules,25,DragonWilds::TimeOfDay::Requirement::Day));
    assert(day.size()==3);
    assert(day[0]["Category"]=="General" && day[0]["Tag"]=="general");
    assert(day[1]["Category"]=="Day" && day[1]["Tag"]=="day-first");
    assert(day[2]["Category"]=="Day" && day[2]["Tag"]=="day-late");
    for(const auto& offer:day)assert(offer["Category"]!="Night" && offer["Category"]!="Elite");
    const auto night=OrderedWithinCategories(DragonWilds::VendorCategoryGate::Filter(
        gated,rules,75,DragonWilds::TimeOfDay::Requirement::Night));
    assert(night.size()==3);
    assert(night[0]["Category"]=="General");
    assert(night[1]["Category"]=="Night");
    assert(night[2]["Category"]=="Elite");
    const auto unknownPower=DragonWilds::VendorCategoryGate::Filter(
        gated,rules,std::nullopt,DragonWilds::TimeOfDay::Requirement::Day);
    assert(std::none_of(unknownPower.begin(),unknownPower.end(),[](const auto& offer){return offer["Category"]=="Elite";}));
}
