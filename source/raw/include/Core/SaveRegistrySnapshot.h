#pragma once
#include <memory>
#include <mutex>
#include <unordered_set>
#include <string>
namespace PS::SaveCleanup {
struct RegistrySnapshot {
    std::unordered_set<std::string> Items,Recipes,Quests;
    bool QuestsComplete=false;
    bool Ready()const{return Items.size()>=500 && Recipes.size()>=300;}
};
inline std::mutex RegistryMutex;
inline std::shared_ptr<const RegistrySnapshot> CurrentRegistry;
inline void PublishRegistry(RegistrySnapshot snapshot) {
    auto next=snapshot.Ready()?std::make_shared<const RegistrySnapshot>(std::move(snapshot)):nullptr;
    std::lock_guard lock(RegistryMutex);CurrentRegistry=std::move(next);
}
inline std::shared_ptr<const RegistrySnapshot> ReadRegistry() {
    std::lock_guard lock(RegistryMutex);return CurrentRegistry;
}
}
