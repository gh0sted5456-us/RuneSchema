#pragma once
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace PS {
template<class T, size_t Capacity = 4096>
class UnrealReadinessGate {
public:
    enum class BeginResult { Wait, Start, Overflow };
    void MarkUnrealReady() { unrealReady.store(true, std::memory_order_release); }
    bool IsUnrealReady() const { return unrealReady.load(std::memory_order_acquire); }
    bool Observe(T value) {
        if (active.load(std::memory_order_acquire)) return true;
        std::lock_guard lock(mutex);
        if (active.load(std::memory_order_relaxed)) return true;
        if (failed || !value || seen.contains(value)) return false;
        if (pending.size() == Capacity) { overflow = true; return false; }
        seen.insert(value);
        pending.push_back(value);
        return false;
    }
    BeginResult Begin() {
        if (!unrealReady.load(std::memory_order_acquire)) return BeginResult::Wait;
        std::lock_guard lock(mutex);
        if (started || failed) return BeginResult::Wait;
        if (overflow) { failed = true; pending.clear(); seen.clear(); return BeginResult::Overflow; }
        started = true;
        return BeginResult::Start;
    }
    std::vector<T> Complete() {
        std::lock_guard lock(mutex);
        if (!started || failed || overflow) {
            failed = true; pending.clear(); seen.clear(); return {};
        }
        auto result = std::move(pending);
        std::unordered_set<T>{}.swap(seen);
        active.store(true, std::memory_order_release);
        return result;
    }
    bool IsActive() const { return active.load(std::memory_order_acquire); }
    void Fail() {
        std::lock_guard lock(mutex);
        failed = true; active.store(false, std::memory_order_release);
        pending.clear(); seen.clear();
    }
private:
    std::atomic<bool> unrealReady{false}, active{false};
    std::mutex mutex;
    bool started = false, failed = false, overflow = false;
    std::vector<T> pending;
    std::unordered_set<T> seen;
};
}
