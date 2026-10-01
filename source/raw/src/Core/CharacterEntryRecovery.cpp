#include "Core/CharacterEntryRecovery.h"

#include "SDK/DragonWildsSignatures.h"
#include "Utility/InlineHook.h"
#include "Utility/Logging.h"
#include "Unreal/FString.hpp"

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <cwctype>
#include <string>

using namespace RC;

namespace {
bool StandaloneBypassLoaded()
{
    const auto snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W module{};
    module.dwSize = sizeof(module);
    bool found = false;
    if (Module32FirstW(snapshot, &module)) {
        do {
            std::wstring path{module.szExePath};
            std::transform(path.begin(), path.end(), path.begin(),
                [](wchar_t value) { return std::towlower(value); });
            if (path.find(L"\\mods\\corruptcharacterbypass\\")
                != std::wstring::npos) {
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
    return found;
}
}

namespace DragonWilds {
void CharacterEntryRecovery::Initialize()
{
    if (IsActive()) return;

    if (StandaloneBypassLoaded()) {
        PS::Log<LogLevel::Warning>(STR(
            "[SAVE-ENTRY][CONFLICT] Standalone CorruptCharacterBypass is already loaded; RuneSchema will not install duplicate character-entry hooks. Disable the standalone mod before enabling RuneSchema's integrated recovery.\n"));
        return;
    }

    auto* playerState = SignatureManager::GetSignature(
        "UPersistenceSubsystem::ProcessPlayerStateLoad");

    // Install while UE4SS is still in its early-hook phase. Patching this
    // function after game threads can execute it races live native code.
    m_pruner.PrepareForStartup();
    ActiveInstance = this;
    const bool playerStateReady = PlayerStateHook
        || PS::InstallInlineHook(PlayerStateHook, playerState,
            reinterpret_cast<void*>(&ProcessPlayerStateLoad));

    if (playerStateReady) {
        PS::Log<LogLevel::Normal>(STR(
            "[PERSISTENCE-PRUNER][NATIVE-BOUNDARY-READY] ProcessPlayerStateLoad was hooked during early initialization. Character JSON will be checked once, immediately before native hydration. Native acceptance remains unchanged.\n"));
    } else {
        ActiveInstance = nullptr;
        PS::Log<LogLevel::Error>(STR(
            "[DEGRADED][SERVICE:persistence-pruner] The native ProcessPlayerStateLoad boundary could not be installed. RuneSchema will not modify character JSON.\n"));
    }
}

void CharacterEntryRecovery::Shutdown()
{
    ActiveInstance = nullptr;
    ValidationHook = {};
    PlayerStateHook = {};
}

bool CharacterEntryRecovery::IsActive() const noexcept
{
    return PlayerStateHook.operator bool();
}

bool CharacterEntryRecovery::ValidateCharacter(
    void* first, void* second, void* third, void* fourth, void* fifth)
{
    const bool accepted = ValidationHook.call<bool>(
        first, second, third, fourth, fifth);
    if (!accepted)
        PS::Log<LogLevel::Warning>(STR(
            "[SAVE-ENTRY][VALIDATION-RECOVERED] Native character validation rejected the load; RuneSchema allowed the normal load pipeline to continue so persistence pruning can handle unresolved IDs.\n"));
    return true;
}

bool CharacterEntryRecovery::ProcessPlayerStateLoad(
    void* subsystem, std::int32_t result, void* characterInfo,
    RC::Unreal::FString* playerState)
{
    // Steam 100.6 preserves EDX as the 32-bit load result, R8 as character
    // metadata, and R9 as the FString player-state document. Treating EDX as
    // a pointer corrupts the native call.
    if (ActiveInstance && playerState)
        ActiveInstance->m_pruner.PruneCharacterJson(*playerState);

    const bool accepted = PlayerStateHook.call<bool>(
        subsystem, result, characterInfo, playerState);
    if (!accepted) PS::Log<LogLevel::Warning>(STR(
        "[SAVE-ENTRY][NATIVE-REJECTED] ProcessPlayerStateLoad still rejected the character after safe pruning. RuneSchema did not override native acceptance.\n"));
    return accepted;
}
}
