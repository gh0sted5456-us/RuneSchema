#include "Loader/SurgeEvadeRules.h"
#include <cassert>
#include <iostream>
using namespace DragonWilds::SurgeEvadeRules;
using nlohmann::json;
int main() {
    Rules rules;
    Merge(rules, json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs.Legs":true}})"));
    assert(rules.size() == 1);
    Merge(rules, json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs.Legs":true}})"));
    assert(rules.size() == 1); // repeated rules do not stack
    Merge(rules, json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs.Legs":false}})"));
    assert(rules.empty()); // later mod can disable an earlier entry
    const json invalid[]{
        nullptr, json::array(), json::object(),
        json::parse(R"({"SurgeEvadeLegs":[]})"),
        json::parse(R"({"SurgeEvadeLegs":{},"Unknown":true})"),
        json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/*.Legs":true}})"),
        json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs.Legs:Part":true}})"),
        json::parse(R"({"SurgeEvadeLegs":{"/Script/Test.Legs":true}})"),
        json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs":true}})"),
        json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Legs.Legs":1}})")
    };
    for (const auto& doc : invalid) {
        const auto before = rules;
        bool threw = false;
        try { Merge(rules, doc); } catch (...) { threw = true; }
        assert(threw && rules == before);
    }
    json bulk = {{"SurgeEvadeLegs", json::object()}};
    for (int i = 0; i < 64; ++i) bulk["SurgeEvadeLegs"]["/Game/Test/Legs" + std::to_string(i) + ".Legs"] = true;
    Merge(rules, bulk);
    assert(rules.size() == 64);
    bool threw = false;
    try { Merge(rules, json::parse(R"({"SurgeEvadeLegs":{"/Game/Test/Overflow.Legs":true}})")); }
    catch (...) { threw = true; }
    assert(threw && rules.size() == 64);
    std::cout << "Surge rules: duplicate, override, invalid input, atomic rollback and capacity checks passed\n";
}
