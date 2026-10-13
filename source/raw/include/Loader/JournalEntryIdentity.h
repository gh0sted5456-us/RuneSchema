#pragma once

#include "Loader/ItemIdentity.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace DragonWilds::JournalEntryIdentity {
inline std::string PersistenceId(const std::string& key,const nlohmann::json& body) {
    if(!body.contains("PersistenceID"))return key; // Preserve existing journal identities.
    const auto& value=body.at("PersistenceID");
    if(!value.is_string() || !IsCanonicalPersistenceId(value.get<std::string>()))
        throw std::runtime_error("Journal PersistenceID must be a canonical 22-character identity");
    return value.get<std::string>();
}
}
