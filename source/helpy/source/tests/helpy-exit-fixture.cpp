#include "Core/ProcessExitFuture.h"
#include "Generator/SourceBanners.h"
#include <Windows.h>

extern "C" __declspec(dllexport) bool BeginPendingBanner(){
    const auto started=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    const auto blocked=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!started||!blocked)return false;
    PS::SourceBanners::Worker=std::async(std::launch::async,[started,blocked]{
        SetEvent(started);WaitForSingleObject(blocked,INFINITE);
        return std::map<std::string,PS::HelpyBackgrounds::Pixels>{};
    });
    return WaitForSingleObject(started,2000)==WAIT_OBJECT_0;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID reserved){
    if(reason==DLL_PROCESS_DETACH&&reserved)PS::ProcessExitInProgress.store(true,std::memory_order_relaxed);
    return TRUE;
}
