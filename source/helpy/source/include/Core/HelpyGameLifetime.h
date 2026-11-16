#pragma once
#include <windows.h>
#include <cstdint>

namespace PS {
class HelpyGameLifetime {
    HANDLE Process=nullptr;uint32_t Pid=0;bool Ended=false;
public:
    HelpyGameLifetime()=default;
    HelpyGameLifetime(const HelpyGameLifetime&)=delete;
    HelpyGameLifetime& operator=(const HelpyGameLifetime&)=delete;
    ~HelpyGameLifetime(){if(Process)CloseHandle(Process);}
    void Observe(uint32_t pid){
        if(!pid||pid==Pid)return;
        const auto process=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!process)return;
        if(Process)CloseHandle(Process);Process=process;Pid=pid;Ended=false;
    }
    bool Exited(){
        if(!Process||Ended||WaitForSingleObject(Process,0)!=WAIT_OBJECT_0)return false;
        Ended=true;return true;
    }
    // Retain one handle until reattachment so Windows cannot recycle its PID.
    HANDLE WaitHandle()const{return Ended?nullptr:Process;}
};
}
