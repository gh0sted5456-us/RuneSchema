#include "Core/WorkSlice.h"
#include <cassert>
int main(){
    using namespace std::chrono;const auto begin=steady_clock::time_point{};
    PS::WorkSlice work(begin);assert(work.MayStart(begin));work.Started();assert(!work.MayStart(begin));
    PS::WorkSlice delayed(begin);assert(!delayed.MayStart(begin+milliseconds(2)));
    size_t cursor=0,frames=0;while(cursor<9){PS::WorkSlice frame(begin);size_t constructions=0;while(cursor<9&&frame.MayStart(begin)){frame.Started();++cursor;++constructions;}assert(constructions==1);++frames;}assert(frames==9);
}
