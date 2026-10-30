#pragma once
#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <string>
namespace PS::HelpyTrayIcon {
inline constexpr UINT Message=WM_APP+0x524,Open=0x4a11,Exit=0x4a12;
inline UINT RestartMessage(){static const UINT message=RegisterWindowMessageW(L"TaskbarCreated");return message;}
class Icon {
    NOTIFYICONDATAW Data{};std::atomic<bool> Added=false;
public:
    ~Icon(){Remove();}
    bool Ready()const{return Added.load();}
    bool Add(HWND owner,HICON picture,const std::wstring& tip){
        if(Added)return true;Data={};Data.cbSize=sizeof(Data);Data.hWnd=owner;Data.uID=1;Data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_SHOWTIP;Data.uCallbackMessage=Message;Data.hIcon=picture;
        wcsncpy_s(Data.szTip,tip.c_str(),_TRUNCATE);if(!Shell_NotifyIconW(NIM_ADD,&Data))return false;
        Data.uVersion=NOTIFYICON_VERSION_4;if(!Shell_NotifyIconW(NIM_SETVERSION,&Data)){Shell_NotifyIconW(NIM_DELETE,&Data);return false;}Added=true;return true;
    }
    void Recreate(){if(!Data.hWnd)return;Added=false;const auto window=Data.hWnd;const auto icon=Data.hIcon;const std::wstring tip=Data.szTip;Add(window,icon,tip);}
    void Remove(){if(Added.exchange(false))Shell_NotifyIconW(NIM_DELETE,&Data);}
    UINT Menu(LPARAM point){
        const auto menu=CreatePopupMenu();if(!menu)return 0;
        AppendMenuW(menu,MF_STRING,Open,L"Open Helpy");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Exit,L"Exit Helpy");SetMenuDefaultItem(menu,Open,FALSE);
        POINT at{static_cast<short>(LOWORD(point)),static_cast<short>(HIWORD(point))};if(at.x==-1&&at.y==-1)GetCursorPos(&at);
        SetForegroundWindow(Data.hWnd);const auto command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,at.x,at.y,0,Data.hWnd,nullptr);DestroyMenu(menu);PostMessageW(Data.hWnd,WM_NULL,0,0);return command;
    }
};
}
