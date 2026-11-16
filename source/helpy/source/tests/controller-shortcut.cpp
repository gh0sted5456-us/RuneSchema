#include "Runtime/HelpyGuideReservation.h"
#include <cassert>
#include <iostream>

int main(){
    using namespace PS::HelpyControllerShortcut;
    Gesture shortcut;assert(!shortcut.Poll(Guide,false,0,true,0));
    assert(shortcut.Poll(Guide,true,0,true,1000));assert(!shortcut.Poll(Guide,true,0,true,2000));
    assert(!shortcut.Native(Guide,true,1100));assert(!shortcut.Poll(Guide,false,0,true,2100));
    assert(shortcut.Native(Guide,true,2200));assert(!shortcut.Poll(Guide,true,0,true,2250));
    assert(!shortcut.Poll(Guide,true,0,false,2300));assert(!shortcut.Poll(Guide,true,0,true,2400));
    assert(!shortcut.Poll(Guide,false,0,true,2500));assert(shortcut.Poll(Guide,true,0,true,2600));
    const WORD chord=XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START;
    assert(!shortcut.Poll(MenuView,false,chord,true,3000));assert(!shortcut.Poll(MenuView,false,XINPUT_GAMEPAD_BACK,true,3100));
    assert(shortcut.Poll(MenuView,false,chord,true,3200));assert(!shortcut.Poll(MenuView,false,chord,true,4000));
    assert(!shortcut.Poll(Disabled,true,chord,true,4500));assert(!shortcut.Native(Disabled,true,5000));
    assert(!shortcut.Native(Guide,false,5500));
    PS::HelpyGuideReservation reservation;assert(!reservation.Start(0,0,nullptr)&&!reservation.Active());
    const auto window=CreateWindowExW(0,L"STATIC",L"Helpy private Guide fixture",0,0,0,1,1,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
    const auto start=[&]{return reservation.Start(GetCurrentProcessId(),GetCurrentThreadId(),window);};
    if(start()){
        const auto deadline=GetTickCount64()+7000;while(reservation.Active()&&GetTickCount64()<deadline)Sleep(20);
        assert(!reservation.Active()); // A stalled or disconnected app must release the reservation without exiting.
        assert(start()&&reservation.Active());
        std::cout<<"Windows GameInput reservation: five-second expiry and restart passed.\n";
    }else std::cout<<reservation.Status<<'\n';
    reservation.Stop();DWORD before=0,after=0;assert(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(unsigned cycle=0;cycle<20;++cycle){(void)start();reservation.Stop();assert(!reservation.Active());}
    assert(GetProcessHandleCount(GetCurrentProcess(),&after)&&after<=before+2);
    assert(!reservation.Start(GetCurrentProcessId(),0,window)&&!reservation.Active());DestroyWindow(window);
}
