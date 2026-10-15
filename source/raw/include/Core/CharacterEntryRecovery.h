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
    static void AcceptCharacter(SafetyHookContext&);
    static inline SafetyHookMid AcceptanceHook;
};
}
