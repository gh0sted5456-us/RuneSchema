#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace PS {
// Invoke snapshots outside the lock to allow reentrant callbacks.
template<class Callback> class OrderedCallbacks {
public:
    uint64_t Add(const Callback& callback) {
        std::lock_guard lock(m_mutex);
        const auto id = m_next++;
        m_callbacks.emplace(id, callback);
        return id;
    }
    void Remove(uint64_t id) {
        std::lock_guard lock(m_mutex);
        m_callbacks.erase(id);
    }
    std::vector<Callback> Snapshot() {
        std::lock_guard lock(m_mutex);
        std::vector<Callback> result;
        result.reserve(m_callbacks.size());
        for (const auto& [id, callback] : m_callbacks) result.push_back(callback);
        return result;
    }
private:
    std::mutex m_mutex;
    uint64_t m_next = 0;
    std::map<uint64_t, Callback> m_callbacks;
};
}
