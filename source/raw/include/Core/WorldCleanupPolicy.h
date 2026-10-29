#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace PS::WorldCleanupPolicy {
inline constexpr std::int64_t MaximumPayloadBytes=512ll*1024*1024;
inline bool BlanketAtEntry(bool sessionValue,bool savedValue,bool atWorldEntry) {
    return atWorldEntry?savedValue:sessionValue;
}
inline bool LocalSlotEligible(std::wstring_view slot,bool clientLoadingListen) {
    if(clientLoadingListen||slot.empty()||slot.size()>511||slot==L"<unavailable>"
        ||slot==L"EnhancedInputUserSettings"||slot.back()==L'.'||slot.back()==L' ')
        return false;
    for(const auto ch:slot)if(ch<32||std::wstring_view(L"<>:\"/\\|?*").find(ch)!=std::wstring_view::npos)return false;
    return true;
}
inline bool CharacterEntryEligible(bool gameplayState,std::wstring_view world) {
    return gameplayState&&!world.empty()&&world!=L"<unavailable>"
        &&world.find(L"FrontEnd")==world.npos&&world.find(L"MainMenu")==world.npos
        &&world.find(L"Preview")==world.npos&&world.find(L"CharacterSelection")==world.npos;
}
inline bool NativeGameplayMap(std::wstring_view map) {
    return map==L"/Game/Maps/World/L_World";
}
// A finite FMemoryReader limit is valid too. Require the entire original
// payload to be readable; never rewrite a partially consumed or network load.
inline bool ArchiveEligible(std::wstring_view slot,std::uint8_t loadingListen,
    std::int64_t position,std::int64_t limit,std::int64_t count) {
    return !slot.empty()&&slot!=L"<unavailable>"&&loadingListen==0&&position==0
        &&count>=8&&count<=MaximumPayloadBytes&&limit>=count;
}
class MenuWindow {
public:
    using Clock=std::chrono::steady_clock;
    explicit MenuWindow(Clock::time_point start=Clock::now()):deadline(start+std::chrono::seconds(30)){}
    bool MayWrite(Clock::time_point now=Clock::now())const {
        return !closed.load(std::memory_order_acquire)&&now<=deadline;
    }
    void Close(){closed.store(true,std::memory_order_release);}
private:
    Clock::time_point deadline;
    std::atomic<bool> closed{false};
};
}
