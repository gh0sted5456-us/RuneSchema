#pragma once
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Hooks.hpp"
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace RC::Unreal {class AActor;class UWorld;}
namespace PS {
class WorldComponentEnrollment {
public:
    ~WorldComponentEnrollment(){Stop();}
    void Start();
    void Stop() noexcept;
private:
    struct Entry {std::string Owner,Path;WeakObjectHandle Type;std::vector<WeakObjectHandle> SeenOwners;};
    std::vector<Entry> m_entries;
    std::vector<WeakObjectHandle> m_enteredStates;
    WeakObjectHandle m_gameStateType,m_componentType;
    std::set<std::pair<int32_t,int32_t>> m_attaching;
    RC::Unreal::Hook::GlobalCallbackId m_beginPlay=RC::Unreal::Hook::ERROR_ID,m_loadMap=RC::Unreal::Hook::ERROR_ID;
    void Enter(RC::Unreal::UWorld* world);
    void Attach(RC::Unreal::AActor* actor);
};
}
