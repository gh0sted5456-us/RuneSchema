#include "Core/JsonArrayPatch.h"
#include "Core/JsonPatchDirective.h"
#include <array>
#include <iostream>
#include <stdexcept>
using nlohmann::json;
using namespace DragonWilds;
static int checks;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("Check failed"); }
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } check(failed); }
int main() {
    auto patch = json::parse(R"({"$Patch":[{"$Index":0,"$Target":{"MinToDrop":4}}]})");
    auto edits = JsonArrayPatch::Parse(patch);
    check(edits.size() == 1 && edits[0].Index == 0);
    check(JsonArrayPatch::Select(edits[0], 2, [](int){return false;}) == 0);
    rejects([&]{JsonArrayPatch::Select(edits[0], 0, [](int){return false;});});
    for (const auto& invalid : {
        R"({"$Patch":[]})", R"({"$Patch":{}})",
        R"({"$Patch":[{"$Index":-1,"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Index":0.5,"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Index":18446744073709551615,"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Index":0,"$Match":{"Id":1},"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Index":0,"$Target":{}}]})",
        R"({"$Patch":[{"$Match":{},"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Bogus":0,"$Target":{"X":1}}]})",
        R"({"$Patch":[{"$Index":0,"$Target":{"X":1}}],"Action":"Clear"})"})
        rejects([&]{JsonArrayPatch::Parse(json::parse(invalid));});
    const auto match = JsonArrayPatch::Parse(json::parse(
        R"({"$Patch":[{"$Match":{"SpawnedItemData":"/Game/Test.Test"},"$Target":{"MinimumDropAmount":3}}]})"))[0];
    check(JsonArrayPatch::Select(match, 3, [](int index){return index == 2;}) == 2);
    rejects([&]{JsonArrayPatch::Select(match, 3, [](int){return false;});});
    rejects([&]{JsonArrayPatch::Select(match, 3, [](int){return true;});});
    const std::array<std::string_view,1> protectedFields{"InternalName"};
    const auto outer = JsonPatchDirective::Parse(json::parse(
        R"({"$Patch":"BP_FellableTree_Ash_C","$Target":{"ItemDropOnSplitComponent":{"ItemsToDrop":{"$Patch":[{"$Index":0,"$Target":{"MinToDrop":4}}]}}}})"), protectedFields, "test");
    check(outer.has_value() && outer->Reference == "BP_FellableTree_Ash_C");
    rejects([&]{JsonPatchDirective::Parse(json::parse(R"({"$Patch":"x","$Target":{"InternalName":"bad"}})"),protectedFields,"test");});
    rejects([&]{JsonPatchDirective::Parse(json::parse(R"({"$Patch":"x"})"),protectedFields,"test");});
    json base = {{"Component",{{"Count",2},{"Other",9}}}};
    const auto change = JsonPatchDirective::Parse(json::parse(R"({"$Patch":"x","$Target":{"Component":{"Count":4}}})"),protectedFields,"test");
    JsonPatchDirective::Apply(base,*change);
    check(base["Component"]["Count"] == 4 && base["Component"]["Other"] == 9);
    std::cout << checks << " parser, selector and legacy patch checks passed\n";
}
