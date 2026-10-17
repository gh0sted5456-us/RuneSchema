#pragma once
#include <chrono>
#include <cstddef>
#include <string_view>
namespace PS::BufferedTrace {
class FlushPolicy {
    size_t m_pending=0;std::chrono::steady_clock::time_point m_last{};
public:
    bool Record(std::chrono::steady_clock::time_point now,bool checkpoint=false){
        if(!m_pending)m_last=now;
        if(++m_pending>=64||checkpoint||now-m_last>=std::chrono::milliseconds(100)){m_pending=0;m_last=now;return true;}return false;
    }
    void Reset(){m_pending=0;m_last={};}
};
inline bool Checkpoint(std::string_view stage){
    return stage.starts_with("ERROR ")||stage=="PostEngineInit complete"||stage=="InitCore complete"
        ||stage=="UE4SS on_unreal_init complete"||stage.starts_with("GameInstanceInit loaders complete")||stage=="shutdown complete";
}
}
