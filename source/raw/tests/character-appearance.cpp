#include "Core/CharacterAppearance.h"
#include "Core/CookedRegistryShards.h"
#include "Core/StandaloneNetworkRegistry.h"
#include <cassert>
#include <stdexcept>
#undef assert
#define assert(value) ((value)?void():throw std::runtime_error(#value))
template<class F>void Reject(F call){bool failed=false;try{call();}catch(...){failed=true;}assert(failed);}
int main(){
    namespace A=PS::CharacterAppearance;
    namespace N=PS::StandaloneNetworkRegistry;
    using J=A::Json;
    const auto sibling = PS::CookedRegistryShards::ParseJson(R"({
        // Standalone JBM opt-in
        "Entries":[{"Id":"appearance","Kind":"CharacterAppearance","Appearance":{"MaxBytes":4096}}]
    })", true);
    const auto empty = J::array();
    assert(N::Select(sibling, "JBM", true, {}, empty).at("Entries") == sibling.at("Entries"));
    assert(N::Select(sibling, "JBM", true, {"JBM:appearance"}, empty).at("Entries").empty());
    assert(N::Select(sibling, "Other", false, {"JBM:appearance"}, empty).at("Entries").size() == 1);
    auto invalid = sibling;
    invalid["Entries"][0]["Kind"] = "GameplayAuthority";
    Reject([&] { N::Select(invalid, "JBM", false, {}, empty); });
    invalid = sibling;
    invalid["NativeRegistries"] = J::object();
    Reject([&] { N::Select(invalid, "JBM", false, {}, empty); });
    invalid = sibling;
    invalid["SchemaVersion"] = 1.5;
    Reject([&] { N::Select(invalid, "JBM", true, {}, empty); });
    const auto generic = J::parse(R"({"SchemaVersion":1,"Entries":[
        {"Id":"emote_state","Kind":"DataRelay","Relay":{"Channel":"emotes","Broadcast":true,"Snapshot":true}}
    ]})");
    const auto routeEntry = N::Select(generic, "ExampleMod", false, {}, empty).at("Entries").at(0);
    const auto relay = PS::ModDataRelay::Declaration("ExampleMod", routeEntry.at("Relay"));
    assert(relay.Key == "ExampleMod:emotes" && relay.Snapshot && !relay.ClientPublish);
    Reject([&] { N::Select(generic, "ExampleMod", true, {}, empty); });
    const auto merged = J::array({J{{"kind", "DataRelay"}, {"owner", "ExampleMod"}, {"relay", routeEntry.at("Relay")}}});
    assert(N::Select(generic, "ExampleMod", false, {}, merged).at("Entries").empty());
    assert(N::Select(generic, "Other", false, {}, merged).at("Entries").size() == 1);
    invalid = generic;
    invalid["Entries"][0]["Relay"]["ClientPublish"] = true;
    Reject([&] { N::Select(invalid, "ExampleMod", false, {}, merged); });
    const J full = std::vector<J>(PS::ModDataRelay::MaxChannels, merged[0]);
    Reject([&] { N::Select(generic, "Other", false, {}, full); });
    const auto& entry = sibling["Entries"][0];
    const auto siblingRoute = A::Declaration("JBM", entry["Id"].get<std::string>(), entry["Appearance"]);
    const auto largeAppearance = J{{"race", "human"}, {"template", std::string(4003, 'x')}}.dump();
    assert(siblingRoute.Key == "JBM:appearance" && largeAppearance.size() == 4033);
    assert(A::Payload(siblingRoute, largeAppearance) == largeAppearance);
    const auto route=A::Declaration("BodyMod","appearance",{{"MaxBytes",4096},{"Class","/BodyMod/BPC_Consumer.BPC_Consumer_C"}});
    assert(route.Key=="BodyMod:appearance"&&route.Limit==4096);
    assert(A::Declaration("BodyMod","native",J::object()).Class.empty());
    Reject([&]{A::Declaration("Bad:Owner","appearance",J::object());});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Class","/Script/Engine.ActorComponent"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Class","/BodyMod/../Bad.Bad_C"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Function","OnAppearance"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"MaxBytes",-1}});});
    assert(A::Declaration("BodyMod","appearance",nlohmann::json::object()).Limit==4096);
    const auto expanded=A::Declaration("BodyMod","appearance",{{"MaxBytes",A::MaximumBytes}});
    const auto longPayload=std::string("{\"template\":\"")+std::string(12000,'x')+"\"}";
    assert(A::Payload(expanded,longPayload)==longPayload);
    Reject([&]{A::Declaration("BodyMod","appearance",{{"MaxBytes",A::MaximumBytes+1}});});
    Reject([&]{A::Payload(route,"[]");});Reject([&]{A::Payload(route,std::string(4097,' '));});
    Reject([&]{A::Payload(route,"{\"bad\":NaN}");});
    Reject([&]{A::Payload(route,std::string(34,'[')+"0"+std::string(34,']'));});
    A::Ledger ledger;const std::string guid="0123456789abcdef0123456789abcdef";
    const auto first=ledger.Put(route,"real-pawn-serial",guid,"{ \"bodyType\": \"male\", \"muscle\":50 }");
    assert(first.CharacterGuid=="0123456789ABCDEF0123456789ABCDEF"&&first.Revision==1&&ledger.Size()==1);
    assert(ledger.Put(route,"real-pawn-serial",guid,first.Payload).Revision==1);
    const auto next=ledger.Put(route,"real-pawn-serial",guid,"{\"muscle\":75}");assert(next.Revision==2&&ledger.Size()==1);
    A::Recipient recipient;
    assert(!recipient.Wants(next,"other-player",false));
    assert(recipient.Wants(next,"real-pawn-serial",false)&&recipient.Wants(next,"other-player",true));
    recipient.Subject(next.Subject,true);assert(recipient.Wants(next,"other-player",false));
    recipient.Seen[{next.Subject,next.Key}]=next.Revision;assert(!recipient.Wants(next,"other-player",false));
    recipient.Subject(next.Subject,false);assert(!recipient.Wants(next,"other-player",false)&&recipient.Seen.empty());
    recipient.Subject(next.Subject,true);assert(recipient.Wants(next,"other-player",false));
    recipient.CheckPublication(route.Key,1);recipient.Published[route.Key]=1;
    Reject([&]{recipient.CheckPublication(route.Key,1);});Reject([&]{recipient.CheckPublication(route.Key,0);});
    recipient.CheckPublication(route.Key,2);recipient.CheckPublication("other:appearance",1);
    ledger.Put(route,"other-pawn",guid,"{}");assert(ledger.Size()==2);
    const auto removed=ledger.Put(route,"real-pawn-serial",guid,"",true);assert(removed.Removed&&removed.Payload=="{}");
    assert(ledger.Snapshot().at({"real-pawn-serial",route.Key}).Removed);
    const auto before=ledger.StoredBytes();Reject([&]{ledger.Put(route,"real-pawn-serial",std::string(32,'0'),"{}");});assert(ledger.StoredBytes()==before);
    ledger.Remove("real-pawn-serial");assert(ledger.Size()==1&&ledger.StoredBytes()==2);
    for(size_t i=1;i<A::MaximumRecords;++i)ledger.Put(route,"pawn-"+std::to_string(i),guid,"{}");
    Reject([&]{ledger.Put(route,"one-too-many",guid,"{}");});assert(ledger.Size()==A::MaximumRecords);
    ledger.Clear();assert(ledger.Size()==0&&ledger.StoredBytes()==0&&ledger.Put(route,"new-world",guid,"{}").Revision==1);
}
