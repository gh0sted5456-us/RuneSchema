#pragma once

#include <cstdint>
#include "safetyhook.hpp"
#include "Core/PersistencePruner.h"

namespace RC::Unreal { class FString; }

namespace DragonWilds {
class CharacterEntryRecovery {
public:
    void Initialize();
    void Shutdown();
    bool IsActive() const noexcept;
    ~CharacterEntryRecovery() { Shutdown(); }

private:
    static bool ValidateCharacter(void*, void*, void*, void*, void*);
    static bool ProcessPlayerStateLoad(void*, std::int32_t, void*,
        RC::Unreal::FString*);

    static inline SafetyHookInline ValidationHook;
    static inline SafetyHookInline PlayerStateHook;
    static inline CharacterEntryRecovery* ActiveInstance = nullptr;
    PS::PersistencePruner m_pruner;
};
}
