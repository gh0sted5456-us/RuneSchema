#include "Core/ProcessEventRoutes.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void Check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
using Routes = PS::ProcessEventRoutes<std::string, std::function<void(int&)>>;
std::vector<std::string> Calls(Routes& routes, const std::string& name, int& value) {
    std::vector<std::string> labels;
    routes.Dispatch(name, [&](const auto& handler, const std::string& label) { labels.push_back(label); handler(value); });
    return labels;
}
}

int main() try {
    {
        Routes routes;
        int value = 0;
        Check(Calls(routes, "Construct", value).empty(), "Empty routes dispatched");
        Check(routes.Add({}, "none", [](int&) {}) == 0, "Route without names was accepted");
        Check(routes.Add({"Construct"}, "none", {}) == 0, "Route without a handler was accepted");

        const auto first = routes.Add({"Construct", "BP_OnOpen"}, "menu", [](int& v) { v += 1; });
        const auto second = routes.Add({"Construct"}, "second", [](int& v) { v *= 10; });
        const auto kill = routes.Add({"OnAIKilled", "OnAIKilled"}, "kill", [](int& v) { v += 100; });
        Check(first && second && kill && first != second, "Routes were not assigned ids");

        // Registration order is kept per name, and a name listed twice runs once.
        Check((Calls(routes, "Construct", value) == std::vector<std::string>{"menu", "second"}) && value == 10, "Order changed");
        value = 0;
        Check((Calls(routes, "OnAIKilled", value) == std::vector<std::string>{"kill"}) && value == 100, "Duplicate name ran twice");
        Check(Calls(routes, "Tick", value).empty(), "Unrouted name dispatched");

        Check(routes.Remove(first) && !routes.Remove(first) && !routes.Remove(0), "Remove result is wrong");
        Check(Calls(routes, "BP_OnOpen", value).empty(), "Removed route still dispatched");
        Check((Calls(routes, "Construct", value) == std::vector<std::string>{"second"}), "Remaining route lost");
        Check(routes.Remove(second) && routes.Remove(kill) && routes.Empty(), "Routes did not empty");
        Check(Calls(routes, "OnAIKilled", value).empty(), "Empty table dispatched");
        Check(routes.RetainedTables() == 0, "Unread tables were retained");
    }
    {
        // A handler removing a later handler for the same call: the later one is skipped.
        Routes routes;
        Routes::Id victim = 0;
        int value = 0;
        routes.Add({"K"}, "remover", [&](int&) { routes.Remove(victim); });
        victim = routes.Add({"K"}, "victim", [](int& v) { v = -1; });
        const auto labels = Calls(routes, "K", value);
        Check(labels == std::vector<std::string>{"remover"} && value == 0, "Removed handler ran in the same dispatch");
        // The table read by that dispatch was kept while it was in use, then freed on the next change.
        Check(routes.RetainedTables() == 1, "Table in use was not retained");
        const auto added = routes.Add({"Other"}, "other", [](int&) {});
        Check(added && routes.RetainedTables() == 0, "Retired tables were not freed");
    }
    {
        // A handler adding a route during dispatch does not run in that dispatch.
        Routes routes;
        int value = 0, added = 0;
        routes.Add({"K"}, "adder", [&](int&) { if (!added++) routes.Add({"K"}, "late", [](int& v) { v = 7; }); });
        Check((Calls(routes, "K", value) == std::vector<std::string>{"adder"}) && value == 0, "Late route ran early");
        Check((Calls(routes, "K", value) == std::vector<std::string>{"adder", "late"}) && value == 7, "Late route missing");
    }
    {
        // Limits.
        Routes routes;
        std::vector<std::string> many(Routes::MaximumNames + 1, "n");
        Check(routes.Add(many, "many", [](int&) {}) == 0, "Too many names accepted");
        for (std::size_t i = 0; i < Routes::MaximumHandlers; ++i)
            Check(routes.Add({"N" + std::to_string(i)}, "h", [](int&) {}) != 0, "Handler within the limit refused");
        Check(routes.Add({"Overflow"}, "h", [](int&) {}) == 0, "Handler beyond the limit accepted");
        int value = 0;
        Check(Calls(routes, "N200", value).size() == 1 && Calls(routes, "Overflow", value).empty(), "Large table routes wrong");
    }
    {
        // Concurrent dispatch while routes change: no lost routed calls for a stable route, no crashes.
        Routes routes;
        std::atomic<long> stable{0};
        routes.Add({"Stable"}, "stable", [&](int&) { stable.fetch_add(1, std::memory_order_relaxed); });
        std::atomic<bool> stop{false};
        std::vector<std::thread> readers;
        long expected[4]{};
        for (int t = 0; t < 4; ++t)
            readers.emplace_back([&, t] {
                int value = 0;
                while (!stop.load()) {
                    routes.Dispatch(std::string("Stable"), [&](const auto& handler, const std::string&) { handler(value); });
                    routes.Dispatch(std::string("Churn"), [&](const auto& handler, const std::string&) { handler(value); });
                    routes.Dispatch(std::string("Tick"), [&](const auto& handler, const std::string&) { handler(value); });
                    ++expected[t];
                }
            });
        for (int i = 0; i < 20000; ++i) {
            const auto id = routes.Add({"Churn", "Other" + std::to_string(i % 7)}, "churn", [](int& v) { ++v; });
            routes.Remove(id);
        }
        stop = true;
        for (auto& reader : readers) reader.join();
        const long total = expected[0] + expected[1] + expected[2] + expected[3];
        Check(stable.load() == total, "Stable route missed calls while other routes changed");
        routes.Add({"Final"}, "final", [](int&) {});
        Check(routes.RetainedTables() == 0, "Tables retained after readers stopped");
    }
    std::cout << "ProcessEvent routing order, removal during dispatch, limits and concurrent table swaps passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
