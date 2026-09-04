#pragma once

#include <string>
#include "nlohmann/json.hpp"

namespace DragonWilds
{
    // Validates and canonicalizes the author-facing $Create/$Clone contract.
    // $Create constructs from reflected class defaults. $Clone constructs a
    // fresh object of the source asset's class and copies safe reflected values
    // before authored overrides are applied. Returns true when either directive
    // was present.
    bool NormalizeAssetCreateDirective(nlohmann::json& properties);

    // Runtime-created items are exposed through a deterministic in-memory
    // package so engine tools and other UE4SS mods can discover them by path.
    // Both segments are normalized to Unreal-safe letters/numbers/underscores.
    std::string BuildRuntimeItemObjectPath(
        const std::string& modName, const std::string& internalName);
}

