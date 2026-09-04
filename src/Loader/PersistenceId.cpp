#include "Loader/PersistenceId.h"

#include <algorithm>

namespace DragonWilds
{
    bool IsCanonicalPersistenceId(std::string_view value)
    {
        if (value.size() != 22)
            return false;
        if (!std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '-' || character == '_';
        }))
            return false;

        // Sixteen bytes leave only two significant bits in base64url's last
        // sextet. A/Q/g/w are therefore the only canonical final characters.
        const auto finalCharacter = value.back();
        return finalCharacter == 'A' || finalCharacter == 'Q'
            || finalCharacter == 'g' || finalCharacter == 'w';
    }
}

