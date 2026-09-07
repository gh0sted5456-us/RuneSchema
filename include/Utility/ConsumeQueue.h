#pragma once
#include <type_traits>
#include <utility>
#include <vector>

namespace PS {
template<class T, class Consume>
void ConsumeQueue(std::vector<T>& queue, Consume&& consume) {
    static_assert(std::is_nothrow_move_assignable_v<T>);
    size_t read = 0, kept = 0;
    auto retain = [&] {
        if (kept != read) queue[kept] = std::move(queue[read]);
        ++kept;
    };
    try {
        for (; read < queue.size(); ++read) {
            if (!consume(queue[read])) retain();
        }
    } catch (...) {
        // Preserve the failing entry and untouched suffix for the caller's recovery.
        for (; read < queue.size(); ++read) retain();
        queue.erase(queue.begin() + kept, queue.end());
        throw;
    }
    queue.erase(queue.begin() + kept, queue.end());
}
}
