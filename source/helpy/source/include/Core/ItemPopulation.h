#pragma once
#include <string>
#include <algorithm>
namespace PS::ItemPopulation {
struct Icons {size_t Ready=0,Total=0,Unavailable=0;};
struct View {std::string Label;bool Busy=false;float Fraction=0;bool Determinate=false;};
inline View Describe(const std::string& stage,bool hasRows,size_t indexed,const Icons& icons,bool checking,size_t total=0){
    if(stage=="cached-items-verifying")return {"Showing saved cache · checking mounted content...",true};
    if(stage=="searching-live-items")return {hasRows?"Showing matches · checking uncached items...":"Checking uncached items for this search...",true};
    if(stage=="filtering-cache")return {"Searching cached items: "+std::to_string(indexed)+(total?" / "+std::to_string(total):" checked"),true,total?std::min(1.f,float(indexed)/total):0,total!=0};
    if(!hasRows){
        if(stage=="checking-cache")return {"Checking cache and mounted sources...",true};
        if(stage=="reading-catalogue")return {"Loading cached items...",true};
        if(stage=="discovering-items")return {indexed?"Discovering items: "+std::to_string(indexed)+" found":"Discovering items...",true};
        return {"Items ready",false,1};
    }
    if(icons.Total&&icons.Ready+icons.Unavailable<icons.Total)return {"Applying visible icons: "+std::to_string(icons.Ready)+" / "+std::to_string(icons.Total),true,float(icons.Ready)/icons.Total,true};
    if(icons.Unavailable)return {"Items ready · "+std::to_string(icons.Unavailable)+" visible icon(s) unavailable",false,icons.Total?float(icons.Ready)/icons.Total:1};
    return {checking?"Ready · checking source changes in background":"Ready · source checks run quietly in background",false,1};
}
}
