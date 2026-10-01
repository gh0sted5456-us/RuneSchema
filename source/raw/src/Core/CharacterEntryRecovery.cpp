#include "Core/CharacterEntryRecovery.h"

#include "SDK/DragonWildsSignatures.h"
#include "Utility/InlineHook.h"
#include "Utility/Logging.h"

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

    auto* validation = SignatureManager::GetSignature("CharacterSave::Validate");
    auto* playerState = SignatureManager::GetSignature(
        "UPersistenceSubsystem::ProcessPlayerStateLoad");

    const bool validationReady = ValidationHook
        || PS::InstallInlineHook(ValidationHook, validation,
            reinterpret_cast<void*>(&ValidateCharacter));
    const bool playerStateReady = PlayerStateHook
        || PS::InstallInlineHook(PlayerStateHook, playerState,
            reinterpret_cast<void*>(&ProcessPlayerStateLoad));

    if (validationReady && playerStateReady) {
        PS::Log<LogLevel::Normal>(STR(
            "[SAVE-ENTRY][READY] Opt-in character validation and ProcessPlayerStateLoad recovery are active. Native loading still runs; only their final acceptance result is recovered.\n"));
    } else {
        PS::Log<LogLevel::Error>(STR(
            "[DEGRADED][SERVICE:character-entry-recovery] Opt-in character-entry recovery is incomplete (validation={}, player-state={}). RuneSchema pruning remains independent and will never delete against an incomplete registry.\n"),
            validationReady, playerStateReady);
    }
}

void CharacterEntryRecovery::Shutdown()
{
    ValidationHook = {};
    PlayerStateHook = {};
}

bool CharacterEntryRecovery::IsActive() const noexcept
{
    return ValidationHook && PlayerStateHook;
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
    void* first, void* second, void* third, void* fourth)
{
    const bool accepted = PlayerStateHook.call<bool>(
        first, second, third, fourth);
    if (!accepted)
        PS::Log<LogLevel::Warning>(STR(
            "[SAVE-ENTRY][PLAYER-STATE-RECOVERED] ProcessPlayerStateLoad reported failure after running; RuneSchema allowed world entry to continue. No save fields were changed by this recovery hook.\n"));
    return true;
}
}
