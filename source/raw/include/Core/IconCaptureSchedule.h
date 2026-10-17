#pragma once
#include <cstdint>
namespace PS::IconCapture {
class Schedule {
    enum class Phase { Warm, Read } m_phase=Phase::Warm;
    std::uint64_t m_started=0,m_due=0;
    unsigned m_attempts=0;
public:
    void Begin(std::uint64_t now){m_phase=Phase::Warm;m_started=now;m_due=now+120;m_attempts=0;}
    bool Expired(std::uint64_t now)const{return now-m_started>=5000;}
    bool CanDraw(std::uint64_t now)const{return !Expired(now)&&m_phase==Phase::Warm&&now>=m_due;}
    bool CanRead(std::uint64_t now)const{return !Expired(now)&&m_phase==Phase::Read&&now>=m_due;}
    // The original Helpy 2.0 capture retried a blank translucent draw with an
    // opaque draw. Keep that fallback across frames rather than redrawing the
    // same invisible alpha six times or forcing synchronous GPU readbacks.
    bool Opaque()const{return m_attempts>0;}
    void Submitted(std::uint64_t now){m_phase=Phase::Read;m_due=now+32;}
    bool Blank(std::uint64_t now){if(++m_attempts>=6||Expired(now))return false;m_phase=Phase::Warm;m_due=now+80*m_attempts;return true;}
};
}
