#pragma once
#include "Core/FrameRate.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace PS::HelpyPerformance {
inline FrameRate Counter;
inline FrameRate::Clock::time_point Next{};
inline double UiFps=0,GameFps=0;
inline void PollGame(bool enabled,int refreshMs,const std::function<bool(const nlohmann::json&,nlohmann::json&)>& read){
    if(!enabled){Next={};GameFps=0;return;}
    const auto now=FrameRate::Clock::now();
    const auto interval=std::chrono::milliseconds(std::clamp(refreshMs,500,2000));
    if(now<Next)return;
    Next=now+interval;GameFps=0;
    // This service reads two cached atomics. It never queues work on the game thread.
    nlohmann::json result;
    try{
        if(read&&read({{"Action","FrameRate"}},result)&&result.value("Available",false)){
            const auto fps=result.value("Fps",0.0);if(std::isfinite(fps)&&fps>0)GameFps=fps;
        }
    }catch(...){} // An older or unavailable core cannot stop the UI counter.
}
inline void Poll(bool enabled,int refreshMs,const std::function<bool(const nlohmann::json&,nlohmann::json&)>& read){
    if(!enabled){Counter={};UiFps=0;return;}
    const auto now=FrameRate::Clock::now();
    if(Counter.Active&&now-Counter.Last>std::chrono::seconds(2))UiFps=0;
    if(const auto fps=Counter.Sample(now,std::chrono::milliseconds(std::clamp(refreshMs,500,2000))))UiFps=*fps;
    PollGame(true,refreshMs,read);
}
inline std::string GameLabel(){char text[48];if(GameFps>0)std::snprintf(text,sizeof(text),"Game %.0f FPS",GameFps);else std::snprintf(text,sizeof(text),"Game -- FPS");return text;}
inline std::string Label(bool server){
    char game[32]="--",ui[32]="--",text[96];
    if(GameFps>0)std::snprintf(game,sizeof(game),"%.0f",GameFps);
    if(UiFps>0)std::snprintf(ui,sizeof(ui),"%.0f",UiFps);
    std::snprintf(text,sizeof(text),"%s %s | Helpy %s FPS",server?"Server tick":"Game",game,ui);return text;
}
}
