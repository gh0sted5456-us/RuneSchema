#pragma once

#include "safetyhook.hpp"
#include <atomic>

namespace DragonWilds {
class CharacterEntryRecovery {
public:
    void Initialize();
    void Shutdown();
    bool IsActive() const noexcept;
    static const char* StatusText() noexcept { return State.load(std::memory_order_acquire); }
    ~CharacterEntryRecovery() { Shutdown(); }

private:
    static void AcceptCharacter(SafetyHookContext&);
    static void AcceptPlayerState(SafetyHookContext&);
    static inline SafetyHookMid AcceptanceHook;
    static inline SafetyHookMid PlayerStateHook;
    static inline std::atomic<const char*> State{"Disabled for this session"};
};
}
