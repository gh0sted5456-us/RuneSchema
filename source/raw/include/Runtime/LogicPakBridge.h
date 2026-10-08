#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "Runtime/LogicPakContract.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Hooks.hpp"

namespace PS {
class LogicPakBridge {
public:
    ~LogicPakBridge();
    void Start(const std::vector<std::pair<std::filesystem::path, std::string>>& mods);
    void Stop() noexcept;

private:
    struct Candidate {
        LogicPaks::Package Package;
        RC::Unreal::UClass* Class{};
        bool ResolutionAttempted{};
        WeakObjectHandle Actor;
        bool Owned{};
    };

    void Activate(RC::Unreal::UWorld* world);
    void Tick();
    RC::Unreal::AActor* FindExisting(RC::Unreal::UWorld* world, RC::Unreal::UClass* type) const;
    static bool IsSupportedWorld(RC::Unreal::UWorld* world);
    static void InvokeOptional(RC::Unreal::AActor* actor, const RC::Unreal::TCHAR* name);

    std::vector<Candidate> m_candidates;
    LogicPaks::WorldActivationGate m_gate;
    WeakObjectHandle m_world;
    RC::Unreal::Hook::GlobalCallbackId m_beginPlay = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_tick = RC::Unreal::Hook::ERROR_ID;
    unsigned m_tickCadence{};
    bool m_started{};
    bool m_activating{};
};
}
