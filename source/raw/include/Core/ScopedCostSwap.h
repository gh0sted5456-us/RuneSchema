#pragma once
#include <functional>
#include <utility>

namespace PS {
// Nested ineligible calls temporarily see the original costs, then restore their caller's view.
class ScopedCostSwap {
 bool& current;bool previous;std::function<void()> swap;
 void Set(bool value){if(current!=value){swap();current=value;}}
public:
 ScopedCostSwap(bool& state,bool waived,std::function<void()> exchange):current(state),previous(state),swap(std::move(exchange)){Set(waived);}
 ~ScopedCostSwap(){Set(previous);}
 ScopedCostSwap(const ScopedCostSwap&)=delete;
 ScopedCostSwap& operator=(const ScopedCostSwap&)=delete;
};
}
