#include "Loader/StringReplacementRules.h"
#include "Loader/UniqueTarget.h"
#include <cassert>
#include <vector>
using nlohmann::json;
using namespace DragonWilds::StringReplacementRules;
template<class F> void Rejects(F operation) {
    bool rejected=false;
    try { operation(); } catch(const std::exception&) { rejected=true; }
    assert(rejected);
}
int main() {
    struct Entry { std::string To, Owner; bool Matched = true; };
    Entry entry;
    Replace(entry, std::string("silver"), std::string("A"));
    Replace(entry, std::string("gold"), std::string("B"));
    assert(entry.To=="gold" && entry.Owner=="B" && !entry.Matched);
    Replace(entry, std::string("silver"), std::string("C"));
    assert(entry.To=="silver" && entry.Owner=="C");
    std::vector<std::string> targets{"A:course", "B:course"};
    assert(DragonWilds::FindUniqueTarget(targets.begin(), targets.end(), [](const auto& s){return s=="B:course";})==targets.begin()+1);
    assert(DragonWilds::FindUniqueTarget(targets.begin(), targets.end(), [](const auto& s){return s=="missing";})==targets.end());
    Rejects([&]{DragonWilds::FindUniqueTarget(targets.begin(), targets.end(), [](const auto& s){return s.ends_with(":course");});});
    assert(Read("coin")=="coin");
    assert(Read(json::array({"", "coin", ""}))=="\r\ncoin\r\n");
    Validate({{"ST_Items", {{"Copper", "Coin"}}}, {"Gold", "Gold coin"}});
    Rejects([]{Read(3);});
    Rejects([]{Read("");});
    Rejects([]{Read(json::array());});
    Rejects([]{Read(json::array({"valid", 3}));});
    Rejects([]{Validate({{"", "coin"}});});
    Rejects([]{Validate({{"ST_Items", {{"Copper", false}}}});});
    Rejects([]{Validate(json::array());});
}
