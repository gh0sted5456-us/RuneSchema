#pragma once

namespace DragonWilds {
    // Readiness scans are bounded only after a local gameplay world exists.
    // Waiting in menus/loading screens must not consume the per-map budget.
    class VendorScanBudget {
    public:
        static constexpr unsigned Limit = 40;
        bool Exhausted() const { return m_passes >= Limit; }
        unsigned Passes() const { return m_passes; }
        bool Begin(bool worldReady) {
            if (!worldReady || Exhausted()) return false;
            ++m_passes;
            return true;
        }
        void ResetForMap() { m_passes = 0; }
    private:
        unsigned m_passes = 0;
    };

    // One actor-construction attempt per definition per map. Asset readiness
    // checks happen before Begin(); neither failure nor GC re-arms the gate.
    class VendorSpawnGate {
    public:
        bool Pending() const { return !m_attempted; }
        bool Begin() {
            if (m_attempted) return false;
            m_attempted = true;
            return true;
        }
        void ResetForMap() { m_attempted = false; }
    private:
        bool m_attempted = false;
    };
}
