#pragma once
#include <stdexcept>
namespace PS::InspectionTools {
// Bound sparse slot visits separately from live entries.
template<class Valid, class Visit>
unsigned VisitCaptureSlots(int count, int maxIndex, unsigned maxSlots, unsigned maxEntries, Valid valid, Visit visit) {
    if (count < 0 || maxIndex < count || maxIndex < 0 || static_cast<unsigned>(maxIndex) > maxSlots)
        throw std::runtime_error("Invalid sparse layout or MaxSparseSlots exceeded");
    unsigned emitted = 0;
    for (int i = 0; i < maxIndex && emitted < maxEntries; ++i) {
        if (!valid(i)) continue;
        if (!visit(i)) break;
        ++emitted;
    }
    return emitted;
}
}
