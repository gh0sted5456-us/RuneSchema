#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PS::WorldBuildingRegistryValidation {
struct Entry { std::string id; std::uintptr_t object; };
struct Expected { std::string owner, key, id; std::uintptr_t object; };
struct ReverseEntry { std::uintptr_t object; std::uint16_t index; };
struct InternalEntry { std::string name; std::uintptr_t object; };
enum class ExpectedMode { RequireRegistered, ProtectPendingDefinition };

inline std::unordered_set<std::string> Validate(const std::vector<Entry>& persistence,
    const std::vector<std::uintptr_t>& netIds, const std::vector<Expected>& expected,
    const std::vector<ReverseEntry>& reverse = {},
    const std::vector<InternalEntry>& internal = {},
    ExpectedMode expectedMode = ExpectedMode::RequireRegistered) {
    if (persistence.empty() || netIds.empty())
        throw std::runtime_error("live building registry is empty");
    std::unordered_map<std::string, std::uintptr_t> identities;
    std::unordered_map<std::uintptr_t, std::string> objectIdentities;
    std::unordered_set<std::uintptr_t> objects, network;
    for (const auto& entry : persistence) {
        if (entry.id.empty() || !entry.object || !identities.emplace(entry.id, entry.object).second
            || !objects.insert(entry.object).second
            || !objectIdentities.emplace(entry.object, entry.id).second)
            throw std::runtime_error("live building PersistenceID registry has an invalid or duplicate entry");
    }
    for (auto object : netIds)
        if (!object || !network.insert(object).second || !objects.contains(object))
            throw std::runtime_error("live building NetId registry disagrees with PersistenceID registry");
    if (!reverse.empty() || !internal.empty()) {
        if (reverse.size() != netIds.size())
            throw std::runtime_error("live building registry views are incomplete");
        // Internal-name aliases are auxiliary. Some native builds populate
        // none; completeness is certified by persistence + forward/reverse IDs.
        std::unordered_map<std::uintptr_t, std::uint16_t> reverseIndex;
        for (const auto& entry : reverse)
            if (!entry.object || !reverseIndex.emplace(entry.object, entry.index).second)
                throw std::runtime_error("live building reverse NetId registry has an invalid entry");
        std::unordered_set<std::string> names;
        for (std::size_t index = 0; index < internal.size(); ++index) {
            const auto& entry = internal[index];
            if (!entry.object)
                throw std::runtime_error("live building InternalName registry has a null object at entry "
                    + std::to_string(index));
            if (!objects.contains(entry.object))
                throw std::runtime_error("live building InternalName registry references an object absent from PersistenceIDToDataMap at entry "
                    + std::to_string(index));
            // Native data can have an empty internal name. Its object still
            // has to agree with the persistence and network registries.
            if (!entry.name.empty() && !names.insert(entry.name).second)
                throw std::runtime_error("live building InternalName registry has a duplicate name at entry "
                    + std::to_string(index));
        }
        for (std::size_t index = 0; index < netIds.size(); ++index) {
            const auto found = reverseIndex.find(netIds[index]);
            if (found == reverseIndex.end())
                throw std::runtime_error("live building reverse NetId registry is missing array entry "
                    + std::to_string(index));
            if (found->second != index)
                throw std::runtime_error("live building reverse NetId registry index mismatch at array entry "
                    + std::to_string(index) + ": reverse index "
                    + std::to_string(found->second));
        }
    }
    // Legitimate persistence-only rows exist; keep them in the live set.
    std::unordered_set<std::string> expectedIds;
    std::unordered_set<std::uintptr_t> expectedObjects;
    std::unordered_set<std::string> result;
    for (const auto& [id, object] : identities) result.insert(id);
    for (const auto& entry : expected) {
        if (entry.id.empty() || !entry.object
            || !expectedIds.insert(entry.id).second
            || !expectedObjects.insert(entry.object).second)
            throw std::runtime_error("enabled building definitions have an empty or duplicate identity");
        const auto found = identities.find(entry.id);
        if (found != identities.end()) {
            if (found->second != entry.object || (!network.contains(entry.object)
                    && expectedMode != ExpectedMode::ProtectPendingDefinition))
                throw std::runtime_error("enabled building definition disagrees with live registries: owner='"
                    + entry.owner + "' key='" + entry.key + "' PersistenceID='" + entry.id + "'");
            continue;
        }
        if (expectedMode != ExpectedMode::ProtectPendingDefinition
            || network.contains(entry.object) || objectIdentities.contains(entry.object))
            throw std::runtime_error("enabled building definition is absent from live registries: owner='"
                + entry.owner + "' key='" + entry.key + "' PersistenceID='" + entry.id + "'");
        // Pre-world preflight can precede building registration on clients or
        // dedicated servers. A validated, enabled cooked definition is
        // protected until that lane runs; conflicting live identities fail.
        result.insert(entry.id);
    }
    return result;
}
} // namespace PS::WorldBuildingRegistryValidation
