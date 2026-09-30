#include "Core/CharacterEntryRecovery.h"

#include "SDK/DragonWildsSignatures.h"
#include "Utility/InlineHook.h"
#include "Utility/Logging.h"

using namespace RC;

namespace DragonWilds {
void CharacterEntryRecovery::Initialize()
{
    if (IsActive()) return;

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
            "[SAVE-ENTRY][READY] Character validation and ProcessPlayerStateLoad recovery are active. Native loading still runs; only their final acceptance result is recovered.\n"));
    } else {
        PS::Log<LogLevel::Error>(STR(
            "[DEGRADED][SERVICE:character-entry-recovery] Mandatory character-entry recovery is incomplete (validation={}, player-state={}). RuneSchema pruning remains independent and will never delete against an incomplete registry.\n"),
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
