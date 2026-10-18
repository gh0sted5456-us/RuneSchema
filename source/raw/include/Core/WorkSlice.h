#pragma once
#include <chrono>
#include <cstddef>
namespace PS {
class WorkSlice {
    std::chrono::steady_clock::time_point m_start;
    std::size_t m_started=0;
public:
    explicit WorkSlice(std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now()):m_start(start){}
    bool MayStart(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) const {
        // Native construction is indivisible; never start a second expensive
        // definition in the same frame, even when the first finished quickly.
        return m_started<1&&now-m_start<std::chrono::milliseconds(2);
    }
    void Started(){++m_started;}
};
}
