#include "Loader/ModOrderPolicy.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
int main() {
    using DragonWilds::ModOrderPolicy::Apply;
    const auto name = [](const auto& s) -> const auto& { return s; };
    std::vector<std::string> entries{"ZZ_B", "NormalB", "AA_B", "NormalA", "aa_A", "zz_A", "AA", "ZZ"};
    if (!Apply(entries, name) || entries != std::vector<std::string>{"AA_B","aa_A","NormalB","NormalA","AA","ZZ","ZZ_B","zz_A"})
        throw std::runtime_error("Prefix partition or stable within-group order failed");
    if (Apply(entries, name)) throw std::runtime_error("Second ordering should be unchanged");
    struct Entry { std::wstring name; bool enabled; };
    std::vector<Entry> states{{L"ZZ_Disabled",false},{L"Middle",true},{L"AA_Disabled",false}};
    Apply(states, [](const auto& e) -> const auto& { return e.name; });
    if (states[0].name != L"AA_Disabled" || states[0].enabled || !states[1].enabled || states[2].enabled)
        throw std::runtime_error("Disabled flags were changed");
    std::vector<std::string> empty; if (Apply(empty,name)) throw std::runtime_error("Empty ordering changed");
    std::vector<std::string> normal{"B","A"}; if (Apply(normal,name) || normal[0] != "B") throw std::runtime_error("Ordinary order changed");
    std::cout << "5 load-order policy checks passed\n";
}
