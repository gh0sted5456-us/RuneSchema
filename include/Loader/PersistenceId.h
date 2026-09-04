#pragma once

#include <string_view>

namespace DragonWilds
{
    // Dragonwilds persistence IDs are the unpadded base64url encoding of
    // exactly 16 bytes: 22 characters, with canonical zero padding encoded
    // in the final character.
    bool IsCanonicalPersistenceId(std::string_view value);
}

