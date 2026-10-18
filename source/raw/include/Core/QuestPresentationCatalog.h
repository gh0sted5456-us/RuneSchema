#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <cstdint>
#include <stdexcept>
// Immutable presentation copies for board/Helpy plugins. No native pointers,
// player state, activation, quest acceptance or authority mutation here.
namespace PS::QuestPresentationCatalog {
using Json=nlohmann::json;
inline std::mutex Gate;
inline Json Rows=Json::array();inline uint64_t Revision=0;inline bool Ready=false;
inline void Publish(Json rows){
    if(!rows.is_array()||rows.size()>128)throw std::runtime_error("Quest presentation catalogue exceeds 128 entries");
    std::lock_guard lock(Gate);Rows=std::move(rows);Ready=true;++Revision;
}
inline Json Read(const Json& request){
    if(!request.is_object())throw std::runtime_error("Quest catalogue request must be an object");
    const auto mod=request.value("Mod",std::string{}),key=request.value("Key",std::string{});
    if(mod.size()>128||key.size()>256)throw std::runtime_error("Quest catalogue filter exceeds its bound");
    const auto bounded=[&](const char* field,int fallback,int minimum){
        if(!request.contains(field))return fallback;const auto& value=request.at(field);
        if(!value.is_number_integer()||value<minimum||value>128)throw std::runtime_error("Quest catalogue Limit must be 1..128 and Offset 0..128");
        return value.get<int>();
    };
    const auto limit=bounded("Limit",64,1),offset=bounded("Offset",0,0);
    std::lock_guard lock(Gate);Json result=Json::array();size_t count=0;
    for(const auto& row:Rows){if((!mod.empty()&&row.at("Mod")!=mod)||(!key.empty()&&row.at("Key")!=key))continue;
        if(count>=size_t(offset)&&result.size()<size_t(limit))result.push_back(row);++count;
    }
    return {{"Version",1},{"Ready",Ready},{"Revision",Revision},{"Quests",result},{"Total",count},{"NextOffset",size_t(offset)+result.size()},
        {"HasMore",size_t(offset)+result.size()<count},{"Coverage","Authored quest presentation only; not player progress or acceptance permission"}};
}
}
