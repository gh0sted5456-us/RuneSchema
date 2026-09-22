#pragma once

#include <string>
#include <unordered_set>

namespace DragonWilds {
    // Per-world first-occurrence budget: bounds both storage and export writes.
    class VendorTraceBudget {
    public:
        static constexpr unsigned Limit = 64;
        bool Exhausted() const { return m_seen.size() >= Limit; }
        bool Admit(const std::string& key) {
            return !Exhausted() && m_seen.insert(key).second;
        }
        void Reset() { m_seen.clear(); }
    private:
        std::unordered_set<std::string> m_seen;
    };
}
