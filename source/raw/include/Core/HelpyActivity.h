#pragma once
#include <atomic>
namespace PS::HelpyActivity {
inline std::atomic<bool> Visible{false};
inline bool BackgroundAllowed(bool explicitJob=false){return explicitJob||Visible.load(std::memory_order_relaxed);}
}
