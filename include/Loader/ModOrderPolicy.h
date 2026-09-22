#pragma once
#include <algorithm>
namespace DragonWilds::ModOrderPolicy {
template<class String> bool IsImplicit(const String& name) {
    if (name.size() < 3 || name[2] != '_') return false;
    const auto first = name[0];
    const auto second = name[1];
    const bool aa = (first == 'A' || first == 'a') && (second == 'A' || second == 'a');
    const bool zz = (first == 'Z' || first == 'z') && (second == 'Z' || second == 'z');
    return aa || zz;
}
template<class String> bool ShouldAutoPersist(const String& name) { return !IsImplicit(name); }
template<class String> int Priority(const String& name) {
    if (name.size() >= 3 && name[2] == '_') {
        if ((name[0] == 'A' || name[0] == 'a') && (name[1] == 'A' || name[1] == 'a')) return 0;
        if ((name[0] == 'Z' || name[0] == 'z') && (name[1] == 'Z' || name[1] == 'z')) return 1'000'002;
    }
    int value = 0;
    size_t digits = 0;
    while (digits < name.size() && name[digits] >= '0' && name[digits] <= '9') {
        if (value < 999'999) value = std::min(999'999, value * 10 + static_cast<int>(name[digits] - '0'));
        ++digits;
    }
    if (digits && digits < name.size() && name[digits] == '_') return 1 + value;
    return 1'000'001;
}
template<class Entries, class Name> bool Apply(Entries& entries, Name name) {
    auto less = [&](const auto& a, const auto& b) { return Priority(name(a)) < Priority(name(b)); };
    const bool changed = !std::is_sorted(entries.begin(), entries.end(), less);
    std::stable_sort(entries.begin(), entries.end(), less);
    return changed;
}
}
