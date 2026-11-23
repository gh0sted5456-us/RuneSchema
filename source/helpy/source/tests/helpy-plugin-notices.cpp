#include "Core/HelpyPluginNotices.h"
#include "Core/HelpyNotifications.h"
#include <iostream>
#include <stdexcept>
using namespace PS;
static void Check(bool value, const char* what) { if (!value) throw std::runtime_error(what); }
static bool Rejected(const std::string& owner, const std::string& input) {
    try { HelpyPluginNotices::Parse(owner, input); return false; } catch (...) { return true; }
}
int main() try {
    const auto notice = HelpyPluginNotices::Parse("GrandExchange", R"({"Id":"sale:42","Title":"Listing sold","Detail":"3 Rune bars for 1,200 coins","Open":"GrandExchange.exchange"})");
    Check(notice.Owner == "GrandExchange" && notice.Id == "sale:42" && notice.Route == "GrandExchange.exchange" && notice.Toast, "valid notice");
    Check(!HelpyPluginNotices::Parse("GrandExchange", R"({"Id":"a","Title":"t","Toast":false})").Toast, "toast off");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":"t","Open":"OtherMod.page"})"), "foreign route");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":"t","Open":"GrandExchange."})"), "empty route");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":"t","Open":"GrandExchange.x:y"})"), "route with colon");
    Check(Rejected("GrandExchange", R"({"Id":"a b","Title":"t"})"), "id token");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":""})"), "empty title");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":"line\nbreak"})"), "control characters");
    Check(Rejected("GrandExchange", R"({"Id":"a","Title":"t","Detail":")" + std::string(401, 'x') + R"("})"), "long detail");
    Check(Rejected("GrandExchange", R"(["Id"])"), "not an object");
    Check(Rejected("Bad Owner", R"({"Id":"a","Title":"t"})"), "owner token");
    // Rate: a burst of 16, then about one a second; owners are independent.
    HelpyPluginNotices::Queue queue;
    for (int i = 0; i < 16; ++i) Check(queue.Accept("GrandExchange", R"({"Id":"a","Title":"t"})", 1000), "burst");
    Check(!queue.Accept("GrandExchange", R"({"Id":"a","Title":"t"})", 1000), "over burst");
    Check(queue.Accept("OtherMod", R"({"Id":"a","Title":"t"})", 1000), "independent owner");
    Check(queue.Accept("GrandExchange", R"({"Id":"a","Title":"t"})", 2000), "refill");
    auto drained = queue.Drain(); Check(drained.size() == 18 && queue.Drain().empty(), "drain");
    for (int i = 0; i < 70; ++i) queue.Accept("Owner" + std::to_string(i), R"({"Id":"a","Title":"t"})", 5000);
    Check(queue.Drain().size() == HelpyPluginNotices::MaxPending, "queue cap");
    queue.Accept("GrandExchange", R"({"Id":"a","Title":"t"})", 90000); queue.Accept("OtherMod", R"({"Id":"a","Title":"t"})", 90000);
    queue.Forget("GrandExchange"); drained = queue.Drain(); Check(drained.size() == 1 && drained[0].Owner == "OtherMod", "forget");
    // The notification history keeps the route, replaces by Id, and survives a round trip.
    HelpyNotifications::Store store;
    store.Plugin("GrandExchange", "sale:42", "Listing sold", "3 Rune bars", "GrandExchange.exchange", 10);
    store.Read(store.Notices[0].Id);
    store.Plugin("GrandExchange", "sale:42", "Listing sold", "4 Rune bars", "GrandExchange.exchange", 11);
    Check(store.Notices.size() == 1 && store.Unread() == 1 && store.Notices[0].Detail == "4 Rune bars" && store.Notices[0].Kind == "plugin", "replace by id");
    auto restored = HelpyNotifications::Store::Parse(store.Document());
    Check(restored.Notices[0].Route == "GrandExchange.exchange" && restored.Notices[0].Source == "GrandExchange", "round trip");
    bool refused = false; try { store.Plugin("GrandExchange", "x", "t", "", "OtherMod.page", 12); } catch (...) { refused = true; }
    Check(refused, "store refuses a foreign route");
    auto forged = store.Document(); forged["Notices"][0]["Route"] = "OtherMod.page";
    refused = false; try { HelpyNotifications::Store::Parse(forged); } catch (...) { refused = true; }
    Check(refused, "history refuses a foreign route");
    auto older = store.Document(); older["Notices"][0].erase("Route");
    Check(HelpyNotifications::Store::Parse(older).Notices[0].Route.empty(), "history without routes still loads");
    std::cout << "PASS plugin notices: validation, owner routes, rate, cap, history round trip\n";
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
