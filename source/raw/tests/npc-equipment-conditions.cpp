#include "Loader/NpcEquipmentConditions.h"
#include "Loader/NpcIdentityPayload.h"
#include <cassert>
#include <chrono>
using namespace DragonWilds;
using Json=nlohmann::json;
template<class F> bool Rejects(F fn){try{fn();return false;}catch(const std::exception&){return true;}}
int main() {
    const std::string world="1234567890ABCDEF1234567890ABCDEF",otherWorld="234567890ABCDEF1234567890ABCDEF1";
    const Json rule={{"Id","helmet"},{"QuestCompleted","final"},{"Policy","WorldEverCompleted"},
        {"Equipment",{{"Head","/Test/Helmet.Helmet"}}}};
    Json source={{"Type","Human"},{"EquipmentWhen",Json::array({rule})}};
    const auto rules=NpcEquipment::Parse("Test",source);
    assert(rules.size()==1 && rules[0].Quest=="Test:final");
    const Json base={{"Body","/Test/Body.Body"},{"Legs","/Test/Legs.Legs"}};
    assert(NpcEquipment::Overlay(base,rules,{})==base);
    const auto equipped=NpcEquipment::Overlay(base,rules,{"helmet"});
    assert(equipped.at("Body")==base.at("Body") && equipped.at("Legs")==base.at("Legs") && equipped.at("Head")=="/Test/Helmet.Helmet");
    assert(Rejects([&]{NpcEquipment::Overlay(base,rules,{"unknown"});}));
    assert(Rejects([&]{NpcEquipment::Overlay(base,rules,{"helmet","helmet"});}));
    for(const auto* policy:{"PerPlayer","AnyConnectedPlayer","","World"}) {
        auto bad=source;bad["EquipmentWhen"][0]["Policy"]=policy;assert(Rejects([&]{NpcEquipment::Parse("Test",bad);}));
    }
    auto bad=source;bad["EquipmentWhen"][0].erase("Policy");assert(Rejects([&]{NpcEquipment::Parse("Test",bad);}));
    bad=source;bad["Type"]="AI";assert(Rejects([&]{NpcEquipment::Parse("Test",bad);}));
    bad=source;bad["EquipmentWhen"][0]["Equipment"]={{"MainHand","/Test/Sword.Sword"}};assert(Rejects([&]{NpcEquipment::Parse("Test",bad);}));
    bad=source;auto duplicate=rule;duplicate["Id"]="other";bad["EquipmentWhen"].push_back(duplicate);
    assert(Rejects([&]{NpcEquipment::Parse("Test",bad);})); // Conflicting slots are not order-dependent.
    const auto fixture=std::filesystem::current_path()/"npc-equipment-fixtures"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=fixture/"worlds"/world/"npc-equipment.json";
    assert(!NpcEquipment::Latched(NpcEquipment::Read(path,world),"Test:npc",rules[0]));
    const auto saved=NpcEquipment::Latch(path,world,"Test:npc",rules[0]);
    assert(NpcEquipment::Latched(saved,"Test:npc",rules[0]));
    assert(NpcEquipment::Latch(path,world,"Test:npc",rules[0])==saved); // Idempotent; no reward callback exists.
    assert(NpcEquipment::Latched(NpcEquipment::Read(path,world),"Test:npc",rules[0])); // Restart/disconnected completer.
    assert(!NpcEquipment::Latched(NpcEquipment::Read(fixture/"worlds"/otherWorld/"npc-equipment.json",otherWorld),"Test:npc",rules[0]));
    assert(Rejects([&]{NpcEquipment::Read(path,otherWorld);}));
    assert(Rejects([&]{NpcEquipment::Read(path,"../world");}));
    auto rebound=rules[0];rebound.Quest="Test:other";assert(Rejects([&]{NpcEquipment::Latch(path,world,"Test:npc",rebound);}));
    PS::ConfigFiles::Write(path,"broken");
    assert(Rejects([&]{NpcEquipment::Latch(path,world,"Test:npc",rules[0]);}));
    assert(PS::ConfigFiles::Read(path)=="broken");
    const auto original=NpcIdentity::Encode("Test","npc","fingerprint");
    const auto replicated=NpcIdentity::WithEquipment(original,{"helmet"});
    assert(NpcIdentity::Decode(replicated).EquipmentRules==std::vector<std::string>{"helmet"});
    auto cue=Json::parse(replicated);cue["cueRevision"]=uint64_t(3);cue["cue"]={{"Pose",{{"Preset","Relaxed"}}}};
    const auto preserved=NpcIdentity::Decode(NpcIdentity::WithEquipment(cue.dump(),{}));
    assert(preserved.CueRevision==3 && !preserved.Cue.empty() && preserved.EquipmentRules.empty());
    assert(Rejects([&]{NpcIdentity::WithEquipment(original,{"helmet","helmet"});}));
    assert(Rejects([&]{NpcIdentity::WithEquipment(original,{"/Asset/Injection"});}));
    std::filesystem::remove_all(fixture);
}
