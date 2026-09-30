#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace DragonWilds::CharacterCustomizationLayout {
inline constexpr int MaximumColumns = 8;
inline constexpr std::size_t MaximumRowsPerColumn = 12;

inline int RequiredColumns(std::size_t optionCount, int currentColumns)
{
    if (currentColumns < 1 || currentColumns > MaximumColumns)
        throw std::runtime_error(
            "character customization current column count is outside 1..8");
    const auto required = static_cast<int>(std::max<std::size_t>(1,
        (optionCount + MaximumRowsPerColumn - 1) / MaximumRowsPerColumn));
    return std::min(MaximumColumns, std::max(currentColumns, required));
}
}
