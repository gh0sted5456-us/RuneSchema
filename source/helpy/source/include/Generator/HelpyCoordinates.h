#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/HelpyMapMotion.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace PS::HelpyCoordinates {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline ProcessExitFuture<Json> Work;inline uint32_t RequestPid=0;
inline Clock::time_point Next{},Updated{};inline std::string Label;
inline bool HasPosition=false;inline double X=0,Y=0,Yaw=0;
inline HelpyMapMotion::Smoother Motion;
inline double Seconds(Clock::time_point time=Clock::now()){return std::chrono::duration<double>(time.time_since_epoch()).count();}
inline void Poll(bool enabled,uint32_t pid,const std::function<bool(const Json&,Json&)>& read,bool minimap=false){
    const auto now=Clock::now();
    if(RequestPid!=pid){Label.clear();HasPosition=false;Motion.Reset();Next={};}
    if(Work.valid()&&Work.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{const auto result=Work.get();Label.clear();HasPosition=false;
            if(enabled&&pid&&RequestPid==pid&&result.value("Available",false)&&result.contains("Location")&&result["Location"].is_array()&&result["Location"].size()==3){
                double point[3]{};bool valid=true;for(int i=0;i<3;++i){valid&=result["Location"][i].is_number();if(valid){point[i]=result["Location"][i].get<double>();valid&=std::isfinite(point[i])&&std::abs(point[i])<=1e9;}}
                if(valid){X=point[0];Y=point[1];Yaw=0;HasPosition=true;char text[128];std::snprintf(text,sizeof(text),"X %.0f   Y %.0f   Z %.0f",point[0],point[1],point[2]);Label=text;Updated=now;
                    if(result.contains("Rotation")&&result["Rotation"].is_array()&&result["Rotation"].size()==3){
                        double rotation[3]{};bool validRotation=true;for(int i=0;i<3;++i){validRotation&=result["Rotation"][i].is_number();if(validRotation){rotation[i]=result["Rotation"][i].get<double>();validRotation&=std::isfinite(rotation[i])&&std::abs(rotation[i])<=1e9;}}
                        if(validRotation){Yaw=rotation[1];std::snprintf(text,sizeof(text),"   |   Pitch %.1f   Yaw %.1f",rotation[0],rotation[1]);Label+=text;}
                    }
                    Motion.Update({X,Y,Yaw},Seconds(now));
                }
            }
        }catch(...){Label.clear();HasPosition=false;}
    }
    if(!enabled||!pid){Label.clear();HasPosition=false;Motion.Reset();Next={};return;}
    if(now-Updated>std::chrono::seconds(2)){Label.clear();HasPosition=false;Motion.Reset();}
    if(!read||Work.valid()||now<Next)return;
    Next=now+std::chrono::milliseconds(minimap?100:500);RequestPid=pid;
    Work=std::async(std::launch::async,[read]{Json result;if(!read({{"Action","LocalCoordinates"}},result))return Json::object();return result;});
}
inline void Shutdown(){if(Work.valid())try{Work.get();}catch(...){}Label.clear();HasPosition=false;Next={};}
}
