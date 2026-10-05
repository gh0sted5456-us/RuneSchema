#pragma once

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

namespace PS::MountedModRegistryOwners {

inline std::mutex& Mutex() {
    static std::mutex value;
    return value;
}

inline std::vector<std::string>& Values() {
    static std::vector<std::string> value;
    return value;
}

inline bool ConventionalName(const std::string& value) {
    return !value.empty() && value.size()<=64
        && std::all_of(value.begin(),value.end(),[](unsigned char c) {
            return std::isalnum(c)||c=='_';
        });
}

inline void Reset() {
    std::scoped_lock lock(Mutex());
    Values().clear();
}

inline bool Remember(const std::string& owner) {
    if(!ConventionalName(owner))return false;
    std::scoped_lock lock(Mutex());
    auto& values=Values();
    if(std::find(values.begin(),values.end(),owner)==values.end())values.push_back(owner);
    return true;
}

inline std::vector<std::string> Snapshot() {
    std::scoped_lock lock(Mutex());
    return Values();
}

}
