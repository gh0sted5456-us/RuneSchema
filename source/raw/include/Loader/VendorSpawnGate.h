#pragma once
#include <algorithm>
#include <cmath>

namespace DragonWilds {
    // Use a bounded initial burst, then slow readiness probes. Cell/asset
    // streaming can finish later than that burst; it is not a permanent failure.
    class VendorScanBudget {
    public:
        static constexpr unsigned Limit = 40;
        static constexpr double DeferredSeconds = 5.0;
        bool Exhausted() const { return m_passes >= Limit; }
        unsigned Passes() const { return m_passes; }
        bool Begin(bool worldReady) {
            if (!worldReady) return false;
            if (Exhausted()) {
                if(m_deferredElapsed<DeferredSeconds)return false;
                m_deferredElapsed=0;return true;
            }
            ++m_passes;
            return true;
        }
        void Advance(double seconds) {
            if(Exhausted()&&std::isfinite(seconds)&&seconds>0)
                m_deferredElapsed=std::min(DeferredSeconds,m_deferredElapsed+std::min(1.0,seconds));
        }
        void ResetForMap() { m_passes = 0; m_deferredElapsed=0; }
    private:
        unsigned m_passes = 0;
        double m_deferredElapsed=0;
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
