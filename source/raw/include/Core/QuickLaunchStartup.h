#pragma once
#include <cstdint>
namespace PS::QuickLaunchStartup {
// Bounded frontend readiness checks; no second attempt once native loading begins.
struct Gate {
    uint64_t Armed=0,Next=0;
    void Arm(uint64_t now){Armed=Next=now;}
    bool Expired(uint64_t now)const{return now>=Armed&&now-Armed>=180000;}
    bool Due(uint64_t now,bool armed,bool requested,bool finished){
        if(!armed||requested||finished||Expired(now)||now<Next)return false;
        Next=now+(now-Armed<5000?500:2000);return true;
    }
};
}
