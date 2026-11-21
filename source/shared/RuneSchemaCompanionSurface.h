#pragma once
#include "RuneSchemaHudFrames.h"
#include <string>
#include <cstring>

// UI transport only. It conveys pixels and input, never game commands or account keys.
namespace PS::CompanionSurface {
inline constexpr uint32_t Magic=0x52534353,Version=1;
inline constexpr ULONG_PTR BindMessage=0x52534342;
inline bool Nonce(const char* value) {
    if(!value||strnlen_s(value,33)!=32)return false;
    for(size_t i=0;i<32;++i)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    return true;
}
inline std::wstring Name(uint32_t host,const char* nonce) {
    if(!host||!Nonce(nonce))return {};
    const auto name="Local\\RuneSchema.Companion."+std::to_string(host)+"."+nonce;
    return {name.begin(),name.end()};
}
struct BindRequest {
    uint32_t Signature=Magic,Bytes=sizeof(BindRequest),HostPid{};
    char Nonce[33]{};
};
inline bool Valid(const BindRequest& request) {
    return request.Signature==Magic&&request.Bytes==sizeof(BindRequest)&&request.HostPid&&Nonce(request.Nonce);
}
struct Frame {uint32_t AppPid{},Width{},Height{};char Nonce[33]{};};
struct alignas(64) Session {
    uint32_t Signature=Magic,Bytes=sizeof(Session),InterfaceVersion=Version,HostPid{};
    uint32_t AdapterLow{},Dpi=96;int32_t AdapterHigh{};
    volatile LONG RequestedWidth=0,RequestedHeight=0,Visible=0,ReturnRequested=0;
    alignas(8) volatile LONG64 HostHeartbeat=0,AppHeartbeat=0;
    volatile LONG FrameSequence=0;
    Frame Current;
};
inline bool Shape(const Session& session,uint32_t host) {
    return session.Signature==Magic&&session.Bytes==sizeof(Session)&&session.InterfaceVersion==Version&&session.HostPid==host;
}
inline bool Alive(volatile LONG64& heartbeat,uint64_t now=GetTickCount64()) {
    const auto value=uint64_t(HudFrames::Read(heartbeat));return value&&now>=value&&now-value<=1500;
}
inline bool ReadFrame(Session& session,Frame& frame) {
    const auto before=HudFrames::Read(session.FrameSequence);if(!before||(before&1))return false;
    frame=session.Current;MemoryBarrier();
    return before==HudFrames::Read(session.FrameSequence)&&frame.AppPid&&Nonce(frame.Nonce)&&
        frame.Width>=160&&frame.Height>=120&&frame.Width<=HudFrames::MaxWidth&&frame.Height<=HudFrames::MaxHeight;
}
inline void Publish(Session& session,const Frame& frame) {
    InterlockedIncrement(&session.FrameSequence);session.Current=frame;MemoryBarrier();InterlockedIncrement(&session.FrameSequence);
}
}
