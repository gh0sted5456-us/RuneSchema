#include "Utility/ConsumeQueue.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

struct Entry {
    int value;
    static inline size_t moves = 0;
    Entry(int v):value(v){}
    Entry(const Entry&) = default;
    Entry(Entry&&) = default;
    Entry& operator=(Entry&& other) noexcept { value = other.value; other.value = -1; ++moves; return *this; }
};
int main() {
    size_t cases = 0;
    for (int size = 0; size <= 9; ++size) {
        for (int mask = 0; mask < (1 << size); ++mask) {
            for (int fail = -1; fail < size; ++fail) {
                std::vector<Entry> actual, expected;
                for (int i=0; i<size; ++i) { actual.emplace_back(i); expected.emplace_back(i); }
                std::vector<int> actualCalls, expectedCalls;
                bool expectedThrow = false, actualThrow = false;
                try {
                    for (auto it = expected.begin(); it != expected.end();) {
                        expectedCalls.push_back(it->value);
                        if (it->value == fail) throw std::runtime_error("fixture");
                        if (mask & (1 << it->value)) it = expected.erase(it); else ++it;
                    }
                } catch (...) { expectedThrow = true; }
                Entry::moves = 0;
                try {
                    PS::ConsumeQueue(actual, [&](Entry& entry) {
                        actualCalls.push_back(entry.value);
                        if (entry.value == fail) throw std::runtime_error("fixture");
                        return bool(mask & (1 << entry.value));
                    });
                } catch (...) { actualThrow = true; }
                assert(Entry::moves <= size);
                assert(expectedThrow == actualThrow && actualCalls == expectedCalls && actual.size() == expected.size());
                for (size_t i=0; i<actual.size(); ++i) assert(actual[i].value == expected[i].value);
                ++cases;
            }
        }
    }
    std::vector<Entry> large;
    large.reserve(10000);
    for (int i=0; i<10000; ++i) large.emplace_back(i);
    auto capacity = large.capacity();
    Entry::moves = 0;
    PS::ConsumeQueue(large, [](Entry&){return true;});
    assert(large.empty() && Entry::moves == 0 && large.capacity() == capacity);
    std::cout << cases << " queue equivalence/exception cases; 10000-entry drain: 0 moves (old: 49995000)\n";
}
