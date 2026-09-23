#include "Core/PatchConflicts.h"
#include <cassert>
using nlohmann::json;
using namespace DragonWilds;
int main() {
    PatchConflicts tracker;
    assert(tracker.Record("recipe:a", json::parse(R"({"Cost":[{"ItemData":"copper","Amount":1}]})"), "A").empty());
    assert(tracker.Record("recipe:a", json::parse(R"({"Cost":[{"ItemData":"silver","Amount":2}]})"), "B").empty());
    assert(tracker.Record("recipe:a", json::parse(R"({"Cost":[{"ItemData":"copper","Chance":0.5}]})"), "B").empty());
    auto conflicts = tracker.Record("recipe:a", json::parse(R"({"Cost":[{"ItemData":"copper","Amount":3}]})"), "C");
    assert(conflicts.size()==1 && conflicts[0].Field=="/Cost/ItemData:copper/Amount");
    assert(conflicts[0].Earlier.starts_with("A ") && conflicts[0].Later.starts_with("C "));
    assert(tracker.Record("recipe:b", json::parse(R"({"Cost":[{"ItemData":"copper","Amount":3}]})"), "C").empty());
    // Equal values are still overlapping authored writes.
    assert(tracker.Record("recipe:a", json::parse(R"({"Cost":[{"ItemData":"copper","Amount":3}]})"), "D").size()==1);
    assert(!tracker.Record("recipe:a", {{"Cost",json::array()}}, "E").empty());
    tracker.Clear();
    auto edit = json::parse(R"({"Drops":{"$Patch":[{"$Index":0,"$Target":{"Amount":1}}]}})");
    assert(tracker.Record("raw:a",edit,"A",false).empty());
    edit["Drops"]["$Patch"][0]["$Index"]=1;
    assert(tracker.Record("raw:a",edit,"B",false).empty());
    edit["Drops"]["$Patch"][0]["$Index"]=0;
    assert(tracker.Record("raw:a",edit,"C",false).size()==1);
    tracker.Clear();
    assert(tracker.Record("a",{{"A/B",1}},"A").empty());
    assert(tracker.Record("a",{{"A",{{"B",1}}}},"B").empty());
    assert(tracker.Record("a",{{"A",nullptr}},"C").size()==1);
    json master=json::parse(R"({"Cost":[{"ItemData":"copper","Amount":1},{"ItemData":"silver","Amount":2}]})");
    JsonLoadOrderMerge::Apply(master,json::parse(R"({"Cost":[{"ItemData":"copper","Amount":9}]})"));
    assert(master["Cost"][0]["Amount"]==9 && master["Cost"][1]["Amount"]==2);
}
