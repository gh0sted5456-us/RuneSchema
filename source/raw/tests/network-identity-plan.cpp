#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Core/NetworkIdentityPlan.h"
#include "Core/NetworkIdentityState.h"

namespace {
void Need(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

int main()
{
    using PS::NetworkIdentityPlan::Candidate;
    const std::vector<Candidate> first{
        {"pWgJ", "/Game/Mods/Bard/Earth_01", 0},
        {"Amo_", "/Game/Mods/Bard/Earth_02", 1},
        {"LRUs", "/Game/Mods/Bard/Earth_03", 2}};
    const std::vector<Candidate> second{first[2], first[0], first[1]};
    const auto left = PS::NetworkIdentityPlan::Canonicalize(first, 43);
    const auto right = PS::NetworkIdentityPlan::Canonicalize(second, 43);
    Need(left.size() == 3 && right.size() == 3, "candidate count changed");
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        Need(left[index].Identity == right[index].Identity,
            "peer discovery order changed canonical identity assignment");
        Need(left[index].AssetPath == right[index].AssetPath,
            "peer discovery order changed canonical asset assignment");
        Need(43 + index <= 65535, "preserved prefix overflowed uint16");
    }

    const std::vector<std::string> stable{"native/A", "native/B",
        left[0].AssetPath, left[1].AssetPath, left[2].AssetPath};
    auto changed = stable;
    std::swap(changed[2], changed[3]);
    Need(PS::NetworkIdentityPlan::Fingerprint(stable)
            != PS::NetworkIdentityPlan::Fingerprint(changed),
        "ordered fingerprint did not detect a peer mapping mismatch");

    bool duplicateRejected = false;
    try
    {
        const std::vector<Candidate> duplicate{
            {"same", "/Game/A", 0}, {"same", "/Game/B", 1}};
        (void)PS::NetworkIdentityPlan::Canonicalize(duplicate, 0);
    }
    catch (const std::runtime_error&)
    {
        duplicateRejected = true;
    }
    Need(duplicateRejected, "duplicate persistence identity was accepted");

    PS::NetworkIdentityState::Reset();
    const auto emptyState = PS::NetworkIdentityState::Fingerprint();
    PS::NetworkIdentityState::Publish("COMBAT-SPELL", "authority-order-a");
    PS::NetworkIdentityState::PublishOwner("COMBAT-SPELL", "Bard", "notes-a");
    const auto firstState = PS::NetworkIdentityState::Fingerprint();
    PS::NetworkIdentityState::Publish("ITEM", "authority-order-b");
    PS::NetworkIdentityState::PublishOwner("ITEM", "Flintlock", "pistol-a");
    const auto completeState = PS::NetworkIdentityState::Fingerprint();
    Need(emptyState != firstState && firstState != completeState,
        "network identity handshake fingerprint ignored a registry lane");
    const auto owners=PS::NetworkIdentityState::OwnerSnapshot();
    Need(owners.contains("Bard") && owners.contains("Flintlock"),
        "owner-scoped network fingerprints were not retained");
    PS::NetworkIdentityState::Reset();
    std::cout << "network identity plan: PASS\n";
    return EXIT_SUCCESS;
}
