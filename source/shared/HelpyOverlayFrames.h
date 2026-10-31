#pragma once
#include <windows.h>
#include <cstdint>
#include <array>
#include <type_traits>

namespace PS::HelpyOverlayFrames {
inline constexpr uint32_t Magic=0x52534f56,Slots=3,InputCapacity=256;
inline constexpr uint32_t MaxWidth=3840,MaxHeight=2160;
enum class InputKind:uint32_t { Key,Character,Mouse,Button,Wheel };
struct Input {InputKind Kind{};uint32_t Code{};int32_t X{},Y{};uint32_t Flags{};};
struct alignas(64) Page {
    uint32_t Signature=Magic,Bytes=sizeof(Page),GamePid{},AppPid{},Width{},Height{};
    uint32_t Left{},Top{};
    uint64_t ReadyHandle{},DoneHandle{};
    std::array<uint64_t,Slots> TextureHandles{};
    volatile LONG PadSequence=0;std::array<uint32_t,4> Pad{};
    volatile LONG ViewportWidth=0,ViewportHeight=0,Opacity=8800;
    alignas(8) volatile LONG64 Heartbeat=0;
    volatile LONG Visible=0,Bound=0,Published=-1,Reading=-1,InputWrite=0,InputRead=0,InputOverflow=0;
    std::array<LONG,Slots> Owner{};
    alignas(8) std::array<LONG64,Slots> Ready{},Consumed{};
    std::array<Input,InputCapacity> Inputs{};
    volatile LONG CursorShape=0,CursorActive=0;
};
static_assert(std::is_standard_layout_v<Page>);
inline LONG Read(volatile LONG& value){return InterlockedCompareExchange(&value,0,0);}
inline LONG64 Read(volatile LONG64& value){return InterlockedCompareExchange64(&value,0,0);}
inline bool Shape(const Page& page,uint32_t gamePid,uint32_t appPid){return page.Signature==Magic&&page.Bytes==sizeof(Page)&&page.GamePid==gamePid&&page.AppPid==appPid&&page.Width>=160&&page.Height>=120&&page.Width<=MaxWidth&&page.Height<=MaxHeight;}
inline bool Alive(Page& page,uint64_t now){const auto heartbeat=uint64_t(Read(page.Heartbeat));return Read(page.Visible)==1&&heartbeat&&now>=heartbeat&&now-heartbeat<=1500;}
// Never overwrite a displayed, newly published, or still GPU-owned surface.
inline int Writable(Page& page,uint64_t completed){
    const auto reading=Read(page.Reading),published=Read(page.Published);
    for(int i=0;i<int(Slots);++i)if(i!=reading&&i!=published&&uint64_t(Read(page.Consumed[i]))<=completed&&InterlockedCompareExchange(&page.Owner[i],1,0)==0)return i;
    return -1;
}
inline int Latest(Page& page,uint64_t completed){const auto slot=Read(page.Published),reading=Read(page.Reading);if(slot>=0&&slot<int(Slots)&&uint64_t(Read(page.Ready[slot]))<=completed&&(slot==reading||InterlockedCompareExchange(&page.Owner[slot],2,0)==0))return slot;return reading>=0&&reading<int(Slots)?reading:-1;}
inline void Presented(Page& page,int slot,uint64_t sequence){InterlockedExchange64(&page.Consumed[slot],sequence);const auto previous=InterlockedExchange(&page.Reading,slot);if(previous>=0&&previous<int(Slots)&&previous!=slot)InterlockedExchange(&page.Owner[previous],0);}
inline bool Push(Page& page,const Input& input){
    const auto written=uint32_t(Read(page.InputWrite)),read=uint32_t(Read(page.InputRead));
    if(written-read>=InputCapacity){InterlockedExchange(&page.InputOverflow,1);InterlockedExchange(&page.Visible,0);return false;}
    page.Inputs[written%InputCapacity]=input;InterlockedExchange(&page.InputWrite,LONG(written+1));return true;
}
inline bool Pop(Page& page,Input& input){
    const auto read=uint32_t(Read(page.InputRead)),written=uint32_t(Read(page.InputWrite));if(read==written)return false;
    if(written-read>InputCapacity){InterlockedExchange(&page.Visible,0);return false;}
    input=page.Inputs[read%InputCapacity];InterlockedExchange(&page.InputRead,LONG(read+1));return true;
}
}
