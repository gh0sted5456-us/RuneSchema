#pragma once

#include "safetyhook.hpp"

namespace DragonWilds {
class CharacterEntryRecovery {
public:
    void Initialize();
    void Shutdown();
    bool IsActive() const noexcept;
    ~CharacterEntryRecovery() { Shutdown(); }

private:
    static bool ValidateCharacter(void*, void*, void*, void*, void*);
    static bool ProcessPlayerStateLoad(void*, void*, void*, void*);

    static inline SafetyHookInline ValidationHook;
    static inline SafetyHookInline PlayerStateHook;
};
}
