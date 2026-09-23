#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace DragonWilds::VendorCategoryLabel {
// Category titles are literal display labels, never tokens or asset identifiers.
// Do not add a required separator, minimum word count, case folding, or slugging.
inline std::string Validate(std::string_view label)
{
    if (label.empty() || label.size() > 128)
        throw std::runtime_error("Vendor category must contain 1-128 bytes");
    if (label.find('\0') != std::string_view::npos)
        throw std::runtime_error("Vendor category cannot contain an embedded NUL");
    if (label.find_first_not_of(" \t\r\n\f\v") == std::string_view::npos)
        throw std::runtime_error("Vendor category cannot be whitespace-only");
    return std::string(label); // Preserve the author's spelling and spacing.
}

inline void RequireExact(std::string_view expected, std::string_view actual)
{
    if (expected != actual)
        throw std::runtime_error("Vendor category text mismatch: expected '"
            + std::string(expected) + "', read back '" + std::string(actual) + "'");
}
}
