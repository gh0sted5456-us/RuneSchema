#pragma once
#include <array>
#include <cstddef>

namespace DragonWilds::VendorAcknowledgement {
    struct FieldRange { std::size_t Offset, Size; };
    inline bool ValidLayout(std::size_t total, const std::array<FieldRange, 3>& fields) {
        if (!total || total > 64) return false;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto a = fields[i];
            if (!a.Size || a.Offset > total || a.Size > total - a.Offset) return false;
            for (std::size_t j = 0; j < i; ++j) {
                const auto b = fields[j];
                if (a.Offset < b.Offset + b.Size && b.Offset < a.Offset + a.Size) return false;
            }
        }
        return true;
    }
    inline bool PrimaryPress(bool release, bool secondary) { return !release && !secondary; }
}
