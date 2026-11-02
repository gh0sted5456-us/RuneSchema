#pragma once
#include "Core/HelpyControllerShortcut.h"
#include <GameInput.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <limits>
#include <string>
#include <thread>

namespace PS {
class HelpyGuideReservation {
    HMODULE Library=nullptr;Microsoft::WRL::ComPtr<IGameInput> Input;
    HANDLE App=nullptr,StopEvent=nullptr;std::thread Watcher;GameInputCallbackToken Token=0;
    HWND Window=nullptr;DWORD AppPid=0,Thread=0;std::atomic<bool> Enabled=false;std::atomic<uint64_t> LeaseUntil=0;
    static void CALLBACK Press(GameInputCallbackToken,void* context,IGameInputDevice*,uint64_t,GameInputSystemButtons current,GameInputSystemButtons previous){
        auto& self=*static_cast<HelpyGuideReservation*>(context);
        if(!self.Enabled||!(current&GameInputSystemButtonGuide)||(previous&GameInputSystemButtonGuide))return;
        DWORD foreground=0,owner=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
        if(foreground==GetCurrentProcessId()&&GetWindowThreadProcessId(self.Window,&owner)==self.Thread&&owner==self.AppPid)
            PostThreadMessageW(self.Thread,HelpyControllerShortcut::GuideMessage,GetCurrentProcessId(),LPARAM(GetTickCount64()));
    }
public:
    std::string Status="Guide reservation is disabled";
    bool Active()const{return Enabled.load();}
    ~HelpyGuideReservation(){Stop();}
    void Stop(){
        Enabled=false;if(StopEvent)SetEvent(StopEvent);if(Watcher.joinable())Watcher.join();
        if(Input){Input->SetFocusPolicy(GameInputDefaultFocusPolicy);if(Token){Input->StopCallback(Token);(void)Input->UnregisterCallback(Token,std::numeric_limits<uint64_t>::max());}Input.Reset();}Token=0;
        if(App)CloseHandle(App);if(StopEvent)CloseHandle(StopEvent);App=StopEvent=nullptr;
        if(Library)FreeLibrary(Library);Library=nullptr;
    }
    bool Start(DWORD pid,DWORD thread,HWND window){
        DWORD owner=0;
        if(!pid||!thread||GetWindowThreadProcessId(window,&owner)!=thread||owner!=pid){Stop();Status="Guide reservation needs Helpy's live window";return false;}
        if(Enabled&&pid==AppPid&&thread==Thread&&window==Window){LeaseUntil=GetTickCount64()+5000;return true;}
        Stop();
        try{
            App=OpenProcess(SYNCHRONIZE,FALSE,pid);StopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!App||!StopEvent){Stop();Status="Guide reservation cannot monitor the Helpy process";return false;}
            Library=LoadLibraryExW(L"GameInput.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            using Create=HRESULT(WINAPI*)(IGameInput**);const auto create=Library?reinterpret_cast<Create>(GetProcAddress(Library,"GameInputCreate")):nullptr;
            if(!create||FAILED(create(&Input))){Stop();Status="Windows GameInput is unavailable; SDL shortcut remains available";return false;}
            Window=window;AppPid=pid;Thread=thread;
            if(FAILED(Input->RegisterSystemButtonCallback(nullptr,GameInputSystemButtonGuide,this,Press,&Token))){Stop();Status="Windows Guide callbacks are unavailable; SDL shortcut remains available";return false;}
            Input->SetFocusPolicy(GameInputExclusiveForegroundGuideButton);LeaseUntil=GetTickCount64()+5000;Enabled=true;
            Watcher=std::thread([this]{
                const HANDLE handles[]{App,StopEvent};
                for(;;){const auto now=GetTickCount64(),until=LeaseUntil.load();if(now>=until||WaitForMultipleObjects(2,handles,FALSE,DWORD(std::min<uint64_t>(1000,until-now)))!=WAIT_TIMEOUT)break;}
                Enabled=false;Input->SetFocusPolicy(GameInputDefaultFocusPolicy);
            });
            Status="Guide reserved for focused Dragonwilds in Windows GameInput. Steam may handle its controller independently.";return true;
        }catch(...){Stop();Status="Guide reservation unavailable; SDL shortcut remains available";return false;}
    }
};
}
