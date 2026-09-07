#pragma once
#include <algorithm>
namespace DragonWilds::ModOrderPolicy {
template<class String> int Priority(const String& name) {
    if (name.size() < 3 || name[2] != '_') return 1;
    if ((name[0] == 'A' || name[0] == 'a') && (name[1] == 'A' || name[1] == 'a')) return 0;
    if ((name[0] == 'Z' || name[0] == 'z') && (name[1] == 'Z' || name[1] == 'z')) return 2;
    return 1;
}
template<class Entries, class Name> bool Apply(Entries& entries, Name name) {
    auto less = [&](const auto& a, const auto& b) { return Priority(name(a)) < Priority(name(b)); };
    const bool changed = !std::is_sorted(entries.begin(), entries.end(), less);
    std::stable_sort(entries.begin(), entries.end(), less);
    return changed;
}
}
