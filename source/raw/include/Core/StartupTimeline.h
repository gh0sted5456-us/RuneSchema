#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
namespace PS::StartupTimeline {
struct Row {std::string Stage;int64_t ElapsedMs=0;};
inline bool Checkpoint(std::string_view stage){
    for(const auto prefix:{"construct loaders ","config load ","signature scan ","offsets ","early hooks ","UE4SS on_","UE4SS RuneSchema tab ","UE4SS readiness ","PostEngineInit ","GameInstanceInit loaders ","data registrar ","InitCore ","ERROR "})
        if(stage.starts_with(prefix))return true;
    return false;
}
class State {
    std::deque<Row> m_rows;size_t m_dropped=0;bool m_started=false,m_complete=false;
public:
    void Begin(){m_rows.clear();m_dropped=0;m_started=true;m_complete=false;}
    void Record(std::string_view stage,int64_t elapsed){
        if(!m_started||m_complete||elapsed<0||!Checkpoint(stage))return;
        if(!m_rows.empty())elapsed=std::max(elapsed,m_rows.back().ElapsedMs);
        size_t end=std::min(size_t(256),stage.size());
        if(end<stage.size())while(end&&(static_cast<unsigned char>(stage[end])&0xc0)==0x80)--end;
        m_rows.push_back({std::string(stage.substr(0,end)),elapsed});
        if(m_rows.size()>64){m_rows.pop_front();++m_dropped;}
        m_complete=stage.starts_with("GameInstanceInit loaders complete");
    }
    bool Started()const{return m_started;}
    bool Complete()const{return m_complete;}
    size_t Dropped()const{return m_dropped;}
    const auto& Rows()const{return m_rows;}
};
}
