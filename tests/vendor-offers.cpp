#include "Loader/VendorOffers.h"
#include <cassert>
#include <map>
#include <vector>
#include <algorithm>
using nlohmann::json;
template<class F>void Rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main() {
    using namespace DragonWilds::VendorOffers;
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
}
