#pragma once
#include <cstdint>
#include <cstddef>
using HMODULE=void*;using LPCWSTR=const wchar_t*;using FARPROC=void(*)();
using HWND=void*;using HHOOK=void*;using DWORD=std::uint32_t;
using WPARAM=std::uintptr_t;using LPARAM=std::intptr_t;using LRESULT=std::intptr_t;
#define CALLBACK
#define TEXT(x) L##x
#define STR(x) L##x
using TCHAR=wchar_t;
struct RECT{long left=0,top=0,right=0,bottom=0;};
struct MEMORY_BASIC_INFORMATION{void* BaseAddress=nullptr;std::size_t RegionSize=0;DWORD State=0,Protect=0;};
constexpr DWORD MEM_COMMIT=0x1000,PAGE_GUARD=0x100,PAGE_NOACCESS=1,PAGE_READWRITE=4,PAGE_WRITECOPY=8,
    PAGE_EXECUTE_READWRITE=0x40,PAGE_EXECUTE_WRITECOPY=0x80,PAGE_READONLY=2;
inline std::uint64_t fakeNow=100;
inline HWND fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{1});
inline bool fakeWindowLive=true,fakeClipFails=false;
inline int fakeClipCalls=0;
inline RECT fakeClip{};
inline std::uint64_t GetTickCount64(){return fakeNow;}
inline HWND GetForegroundWindow(){return fakeWindow;}
inline bool IsWindow(HWND w){return w&&fakeWindowLive;}
inline bool GetClipCursor(RECT* r){*r=fakeClip;return true;}
inline bool ClipCursor(const RECT* r){++fakeClipCalls;if(fakeClipFails)return false;fakeClip=r?*r:RECT{};return true;}
std::size_t VirtualQuery(void*,MEMORY_BASIC_INFORMATION*,std::size_t);
struct POINT {long x=0,y=0;};
struct MSG {HWND hwnd=nullptr;unsigned message=0;WPARAM wParam=0;LPARAM lParam=0;};
constexpr int PM_REMOVE=1,VK_ESCAPE=27,VK_F2=113,VK_CONTROL=17;
constexpr unsigned WM_NULL=0,WM_KILLFOCUS=8,WM_ACTIVATEAPP=28,WM_KEYDOWN=256,WM_KEYUP=257,
    WM_CHAR=258,WM_MOUSEWHEEL=522,WM_LBUTTONDOWN=513,WM_LBUTTONUP=514,WM_LBUTTONDBLCLK=515,
    WM_RBUTTONDOWN=516,WM_RBUTTONUP=517,WM_MBUTTONDOWN=519,WM_MBUTTONUP=520;
inline LRESULT CallNextHookEx(HHOOK,int,WPARAM,LPARAM){return 0;}
inline short GetKeyState(int){return 0;}
inline bool TranslateMessage(MSG*){return true;}
inline bool GetClientRect(HWND,RECT* r){*r={0,0,1920,1080};return true;}
inline unsigned short HIWORD(WPARAM value){return static_cast<unsigned short>((value>>16)&0xffff);}
inline unsigned short LOWORD(LPARAM value){return static_cast<unsigned short>(value&0xffff);}

constexpr DWORD PAGE_EXECUTE=0x10,PAGE_EXECUTE_READ=0x20;
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4,GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT=2;
HMODULE GetModuleHandleW(LPCWSTR);
FARPROC GetProcAddress(HMODULE,const char*);
bool GetModuleHandleExW(DWORD,LPCWSTR,HMODULE*);

inline DWORD fakeThread=42;inline DWORD GetCurrentThreadId(){return fakeThread;}
