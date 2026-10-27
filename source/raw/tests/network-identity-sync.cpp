#include "Core/NetworkIdentitySync.h"
#include "Core/NativeWelcomeLayout.h"
#include "Core/NetworkIdentityState.h"
#include <cassert>
#include <iostream>
#ifdef _WIN32
#include "Core/NetworkIdentityWelcome.h"
#endif
using nlohmann::json;
using namespace PS::NetworkIdentitySync;
template<class F> void Refuses(F function) { bool rejected = false; try { function(); } catch (const std::exception&) { rejected = true; } assert(rejected); }
json Document() {
    return {{"kind", "RuneSchemaNetworkIdentityPlan"}, {"version", 1},
        {"registryOwners", {{"example", "definitions-v1"}}}, {"fixedLanes", {{"other", "fixed-v1"}}},
        {"lanes", {{"items", {{"prefixCount", std::size_t{2}}, {"prefixFingerprint", "vanilla-v1"},
            {"entries", json::array({{{"path", "/Example/A.A"}, {"identity", "a"}, {"type", "/Script/Dominion.ItemData"}}, {{"path", "/Example/B.B"}, {"identity", "b"}, {"type", "/Script/Dominion.ItemData"}}})}}}}}};
}
int Checks() {
    using namespace PS::NativeWelcomeLayout;
    assert(Find(0x00a58497,0x0de46000).Address == 0x0476b5ba);
    Refuses([]{Find(0x00a58498,0x0de46000);});
    std::array<unsigned char,23> site{0x48,0x8b,0x8e,0xa8,0x01,0x00,0x00,0x4c,0x8d,0x45,0x90,0x48,0x8b,0xd3,0x48,0x8b,0x01,0xff,0x90,0x60,0x08,0x00,0x00};
    assert(Verify(site) == 0x860);
    site[19] = 0x58; Refuses([&]{Verify(site);});
    assert(Verify(GamePassCode, Find(0x315b2d33, 0x0dbc7000).Code) == 0x860);
    using namespace PS::NetworkIdentityState;
    assert(!RecordNativeEnrollment(0,2,"quest-id"));
    assert(RecordNativeEnrollment(1,2,"quest-id"));
    assert(HasNativeEnrollment(1,2,"quest-id"));
    assert(!HasNativeEnrollment(3,2,"quest-id") && !HasNativeEnrollment(1,3,"quest-id") && !HasNativeEnrollment(1,2,"other-id"));
    Reset(); SetAuthorityOrderProof("verified-authority"); assert(GetAuthorityOrderProof()=="verified-authority");
    assert(!HasNativeEnrollment(1,2,"quest-id"));
    Reset(); assert(GetAuthorityOrderProof().empty());
    auto local = Document(), server = local;
    std::swap(server["lanes"]["items"]["entries"][0], server["lanes"]["items"]["entries"][1]);
    const auto untouched = local.dump();
    assert((Plan(local, server).at("items") == std::vector<std::size_t>{1, 0}));
    assert(local.dump() == untouched);
    std::vector<int> forward{11, 22}, reverse{0, 1};
    const auto beforeForward = forward, beforeReverse = reverse;
    Refuses([&]{Commit([&]{forward = {22, 11}; throw std::runtime_error("interrupted second table");}, []{return true;}, [&]{forward = beforeForward; reverse = beforeReverse;});});
    assert(forward == beforeForward && reverse == beforeReverse);
    Refuses([&]{Commit([&]{forward = {22, 11}; reverse = {1, 0};}, []{return false;}, [&]{forward = beforeForward; reverse = beforeReverse;});});
    assert(forward == beforeForward && reverse == beforeReverse);
    Commit([&]{forward = {22, 11}; reverse = {1, 0};}, [&]{return forward[reverse[0]] == 11 && forward[reverse[1]] == 22;}, [&]{forward = beforeForward; reverse = beforeReverse;});
    assert((forward == std::vector<int>{22, 11}));
    assert((Plan(local, local).at("items") == std::vector<std::size_t>{0, 1}));
    assert(Parse(server.dump()) == server);
    { auto changed=server; changed["registryOwners"]["example"]="older-definition"; assert(Plan(local,changed)==Plan(local,server)); }
    { auto bad=server; bad["fixedLanes"]["unexpected"]="changed"; Refuses([&]{Plan(local,bad);}); }
    { auto bad = server; bad["lanes"]["items"]["prefixCount"] = std::size_t{3}; Refuses([&]{Plan(local, bad);}); }
    { auto bad = server; bad["lanes"]["items"]["prefixFingerprint"] = "other-native-order"; Refuses([&]{Plan(local, bad);}); }
    { auto bad = server; bad["lanes"]["items"]["entries"][0]["identity"] = "wrong"; Refuses([&]{Plan(local, bad);}); }
    { auto bad = server; bad["lanes"]["items"]["entries"][0]["type"] = "/Script/Dominion.RecipeData"; Refuses([&]{Plan(local, bad);}); }
    { auto missing = local; missing["lanes"]["items"]["entries"].erase(0); Refuses([&]{Plan(missing,server);}); }
    auto extras=local;
    extras["lanes"]["items"]["entries"].insert(extras["lanes"]["items"]["entries"].begin(),json{{"path","/ClientOnly/C.C"},{"identity","extra"},{"type","/Script/Dominion.ItemData"}});
    assert((Plan(extras,server).at("items")==std::vector<std::size_t>{2,1,0}));
    // A late join only changes that client's tables; reconnects may use a different authority order.
    const auto authorityBeforeJoins = server.dump();
    auto firstClient = extras, lateClient = extras;
    const auto align = [](json& client, const json& authority) {
        for (const auto& [key, order] : Plan(client, authority)) {
            const auto original = client["lanes"][key]["entries"];
            for (std::size_t i = 0; i < order.size(); ++i) client["lanes"][key]["entries"][i] = original.at(order[i]);
        }
    };
    align(firstClient, server);
    const auto firstClientAfterJoin = firstClient.dump();
    align(lateClient, server);
    assert(server.dump() == authorityBeforeJoins && firstClient.dump() == firstClientAfterJoin);
    assert(lateClient == firstClient);
    assert((Plan(lateClient, server).at("items") == std::vector<std::size_t>{0, 1, 2}));
    align(lateClient, local);
    assert((Plan(lateClient, local).at("items") == std::vector<std::size_t>{0, 1, 2}));
    assert(lateClient["lanes"]["items"]["entries"][2]["identity"] == "extra");
    assert(firstClient.dump() == firstClientAfterJoin && server.dump() == authorityBeforeJoins);
    { auto bad = server; bad["lanes"]["items"]["entries"][0] = bad["lanes"]["items"]["entries"][1]; Refuses([&]{Validate(bad);}); }
    { auto bad = server; bad["lanes"]["items"]["prefixCount"] = std::size_t{65535}; Refuses([&]{Validate(bad);}); }
    { auto bad = server; bad["lanes"]["items"]["entries"][0]["path"] = "/../escape"; Refuses([&]{Validate(bad);}); }
    { auto bad = server; bad["lanes"]["items"]["prefixCount"] = -1; Refuses([&]{Validate(bad);}); }
    Refuses([]{Parse(std::string(MaxDocument + 1, ' '));});
#ifdef _WIN32
    using namespace PS::NetworkIdentityWelcome;
    const auto token = Encode(server); assert(token.size() < MaxWelcome && Decode(token, local) == server);
    assert(Decode(token,extras)==server);
    PS::HelpyCacheSources::Reset(); PS::HelpyCacheSources::SetDisplayName("Example","Example","1.0");
    assert(PS::HelpyCacheSources::Snapshot().Versions.at("example")=="1.0");
    auto versions=server; versions["sourceVersions"]={{"example","2.0"}};
    auto older=local; older["sourceVersions"]={{"example","1.0"}};
    assert(Decode(Encode(versions),older)==versions && Plan(older,versions)==Plan(local,server));
    { auto bad = local; bad["lanes"]["items"]["entries"][0]["identity"] = "wrong"; Refuses([&]{Decode(token, bad);}); }
    { auto bad = local; bad["lanes"]["items"]["entries"][0]["type"] = "/Script/Dominion.RecipeData"; Refuses([&]{Decode(token, bad);}); }
    for (const auto* malformed : {"=AAA", "A===", "AA=A", "!!!!", "AB==", "AAB="}) Refuses([&]{Unbase64(malformed);});
    Refuses([&]{Decode(token.substr(0, token.size() - 1), local);});
    auto damaged = Unbase64(std::string_view(token).substr(WelcomePrefix.size()));
    damaged[0] = damaged[1] = damaged[2] = damaged[3] = 255;
    Refuses([&]{Decode(std::string(WelcomePrefix) + Base64(damaged), local);});
    auto large = Document();
    for (int i = 0; i < 4000; ++i) large["lanes"]["items"]["entries"].push_back({{"path", "/Example/Items/RepeatedPrefix/Item_" + std::to_string(i)}, {"identity", "item-" + std::to_string(i)}, {"type", "/Script/Dominion.ItemData"}});
    assert(Decode(Encode(large), large) == large);
    std::reverse(large["lanes"]["items"]["entries"].begin(), large["lanes"]["items"]["entries"].end());
    assert(Decode(Encode(large), large) == large);
#endif
    std::cout << "Network identity synchronization planning checks passed\n";
    return 0;
}
int main() { try { return Checks(); } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
