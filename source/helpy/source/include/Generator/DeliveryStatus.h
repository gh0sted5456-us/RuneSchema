#pragma once
// Whether a Helpy delivery to a recipient will be refused, known before the player clicks Give.
//
// The core decides at delivery time (Runtime/HelpyDeliveryGate.h) from the same flags folder
// and the same HelpyDeliveryFlags::Check, so this only explains a refusal early and offers the
// fix. Two limits follow from the core's rules:
// - Only deliveries to the player's own character are gated; a host giving to someone else is not.
// - World pauses need the loaded world's save GUID, which only the core reads, so they are
//   reported as "may apply" rather than as a block.
#include "Core/HelpyDeliveryFlags.h"
#include "Core/PlayerStatus.h"
#include "Core/RuneSchemaDataRoot.h"
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace PS::DeliveryStatus {
namespace F = PS::HelpyDeliveryFlags;

inline F::Snapshot Snapshot;
inline std::chrono::steady_clock::time_point NextRead{};
// Tests only: a flags folder and a clock other than the shared folder and the system time.
inline std::filesystem::path FolderOverride;
inline int64_t TimeOverride = 0;

inline std::filesystem::path Folder() { return FolderOverride.empty() ? RuneSchemaData::SharedDirectory("HELPY") / "flags" : FolderOverride; }
inline int64_t Now() {
    if (TimeOverride) return TimeOverride;
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
// Display reads are throttled; a decision about a click re-reads first (force).
inline void Refresh(bool force = false) {
    const auto now = std::chrono::steady_clock::now();
    if (!force && now < NextRead) return;
    NextRead = now + std::chrono::seconds(3);
    try { Snapshot = F::Read(Folder(), F::Signature::DefaultVerifier(), Now(), false); }
    catch (...) { Snapshot = {}; }
}

struct Status {
    std::optional<F::Block> Block;      // The delivery will be refused, and why.
    std::vector<std::string> WorldMods; // Mods pausing some world; may apply to the loaded one.
    bool Own = false;                   // The recipient is this player's own character.
};

inline std::string LocalGuid(const nlohmann::json& snapshot) {
    if (const auto* local = PlayerStatus::LocalCharacter(snapshot)) return local->value("Guid", std::string{});
    return {};
}

inline Status For(const std::string& recipientGuid, const std::string& localGuid, bool force = false) {
    Status out;
    const auto recipient = F::NormalizeGuid(recipientGuid), local = F::NormalizeGuid(localGuid);
    if (recipient.empty() || recipient != local) return out;
    out.Own = true;
    Refresh(force);
    out.Block = F::Check(Snapshot, recipient, {});
    for (const auto& flag : Snapshot.Active)
        if (flag.Target == "world" && !F::ExceptionFor(Snapshot, flag.Mod, recipient)) { // An exempt character isn't paused by that mod.
            auto name = F::ModName(Snapshot, flag.Mod);
            if (std::find(out.WorldMods.begin(), out.WorldMods.end(), name) == out.WorldMods.end()) out.WorldMods.push_back(std::move(name));
        }
    return out;
}

// The text shown when a click is refused before it reaches the core.
inline std::string Refusal(const F::Block& block, const std::string& name) {
    if (block.User) return "Helpy deliveries are off for " + (name.empty() ? std::string("this character") : name) + ". Turn them on to give items.";
    const auto reason = block.Message.empty() ? block.ModName + " has paused Helpy deliveries to " + (name.empty() ? std::string("this character") : name) + "." : block.Message;
    return reason + " Only " + block.ModName + " can lift this, or stop honouring it in Settings > Deliveries.";
}

// The player's own switch, the one block the Item Bay can lift in place.
inline bool TurnOn(const std::string& guid, const std::string& name, std::string& message) {
    try {
        F::SetUserDeliveries(Folder(), guid, true, Now(), name);
        Refresh(true);
        message = "Helpy deliveries are on for " + (name.empty() ? std::string("this character") : name) + ".";
        return true;
    } catch (const std::exception& error) { message = error.what(); return false; }
}
}
