#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace DragonWilds {
    inline std::string NormalizePlayerAttributeIdentifier(std::string identifier)
    {
        while (!identifier.empty()
            && (identifier.back() == '\'' || identifier.back() == '"'))
            identifier.pop_back();
        if (const auto quote = identifier.find_first_of("'\""); quote != std::string::npos)
            identifier.erase(0, quote + 1);
        if (const auto slash = identifier.find_last_of("/\\"); slash != std::string::npos)
            identifier.erase(0, slash + 1);
        if (const auto dot = identifier.find_last_of('.'); dot != std::string::npos)
            identifier.erase(0, dot + 1);
        std::transform(identifier.begin(), identifier.end(), identifier.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (identifier.ends_with("_c")) identifier.resize(identifier.size() - 2);
        if (identifier.starts_with("da_attribute_")) identifier.erase(0, 13);
        if (identifier.starts_with("u") && identifier.ends_with("attribute")
            && identifier.size() > 10)
            identifier.erase(0, 1);
        if (identifier.ends_with("attribute")) identifier.resize(identifier.size() - 9);
        if (identifier == "maxcarryweight") identifier = "carryweightmax";
        return identifier;
    }
}
