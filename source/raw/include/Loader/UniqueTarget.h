#pragma once
#include <stdexcept>

namespace DragonWilds {
template<class Iterator, class Predicate>
Iterator FindUniqueTarget(Iterator begin, Iterator end, Predicate matches) {
    auto found = end;
    for (auto it = begin; it != end; ++it) {
        if (!matches(*it)) continue;
        if (found != end) throw std::runtime_error("ambiguous target");
        found = it;
    }
    return found;
}
}
