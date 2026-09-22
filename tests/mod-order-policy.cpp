#include "Loader/ModOrderPolicy.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
int main() {
    using DragonWilds::ModOrderPolicy::Apply;
    using DragonWilds::ModOrderPolicy::IsImplicit;
    using DragonWilds::ModOrderPolicy::ShouldAutoPersist;
    const auto name = [](const auto& s) -> const auto& { return s; };
    std::vector<std::string> entries{"ZZ_B", "NormalB", "01_B", "AA_B", "NormalA", "00_First", "aa_A", "01_A", "zz_A", "AA", "ZZ"};
    if (!Apply(entries, name) || entries != std::vector<std::string>{"AA_B","aa_A","00_First","01_B","01_A","NormalB","NormalA","AA","ZZ","ZZ_B","zz_A"})
        throw std::runtime_error("Prefix partition or stable within-group order failed");
    if (Apply(entries, name)) throw std::runtime_error("Second ordering should be unchanged");
    struct Entry { std::wstring name; bool enabled; };
    std::vector<Entry> states{{L"ZZ_Disabled",false},{L"Middle",true},{L"AA_Disabled",false}};
    Apply(states, [](const auto& e) -> const auto& { return e.name; });
    if (states[0].name != L"AA_Disabled" || states[0].enabled || !states[1].enabled || states[2].enabled)
        throw std::runtime_error("Disabled flags were changed");
    std::vector<std::string> empty; if (Apply(empty,name)) throw std::runtime_error("Empty ordering changed");
    std::vector<std::string> normal{"B","A"}; if (Apply(normal,name) || normal[0] != "B") throw std::runtime_error("Ordinary order changed");
    std::vector<std::wstring> numeric{L"10_Tenth",L"2_Second",L"002_AlsoSecond",L"NoPrefix"};
    Apply(numeric,name);
    if(numeric != std::vector<std::wstring>{L"2_Second",L"002_AlsoSecond",L"10_Tenth",L"NoPrefix"})
        throw std::runtime_error("Numeric prefix ordering failed");
    if (!IsImplicit(std::string{"AA_First"}) || !IsImplicit(std::string{"aa_First"})
        || !IsImplicit(std::wstring{L"ZZ_Last"}) || !IsImplicit(std::wstring{L"zz_Last"}))
        throw std::runtime_error("AA_/ZZ_ implicit prefixes were not recognized");
    if (IsImplicit(std::string{"AA"}) || IsImplicit(std::string{"ZZZ_Last"})
        || IsImplicit(std::string{"01_First"}) || IsImplicit(std::string{"Normal"}))
        throw std::runtime_error("Ordinary names were treated as implicit");
    if (ShouldAutoPersist(std::string{"AA_First"}) || ShouldAutoPersist(std::string{"ZZ_Last"})
        || !ShouldAutoPersist(std::string{"Normal"}))
        throw std::runtime_error("Implicit persistence policy failed");
    std::cout << "9 load-order policy checks passed\n";
}
