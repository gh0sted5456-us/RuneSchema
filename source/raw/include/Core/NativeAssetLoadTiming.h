#pragma once
#include <chrono>
#include <cstdint>
#include <utility>

namespace PS::NativeAssetLoadTiming {
struct Totals {
    std::uint64_t Calls=0,TotalNs=0,NativeNs=0;
    std::uint64_t NativeMs()const{return NativeNs/1000000;}
    std::uint64_t WrapperMs()const{return (TotalNs-NativeNs)/1000000;}
};
inline thread_local Totals* Active=nullptr;
struct Batch {
    Totals Cost;
    Totals* Before=Active;
    Batch(){Active=&Cost;}
    ~Batch(){Active=Before;}
    Batch(const Batch&)=delete;
    Batch& operator=(const Batch&)=delete;
};
// Measure only outer loads in an explicit startup batch. Normal gameplay does
// not sample the clock, and recursive loads are already in the outer duration.
class Call {
    using Clock=std::chrono::steady_clock;
    Totals* m_cost=std::exchange(Active,nullptr);
    Clock::time_point m_start=m_cost?Clock::now():Clock::time_point{},m_native;
    bool m_inNative=false;
public:
    void BeginNative(){if(m_cost){m_native=Clock::now();m_inNative=true;}}
    void EndNative(){if(m_cost&&m_inNative){m_cost->NativeNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-m_native).count();m_inNative=false;}}
    ~Call(){if(m_cost){EndNative();++m_cost->Calls;m_cost->TotalNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-m_start).count();Active=m_cost;}}
    Call()=default;
    Call(const Call&)=delete;
    Call& operator=(const Call&)=delete;
};
}
