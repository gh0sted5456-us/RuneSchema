#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace PS::ItemPreviewRequest {
inline nlohmann::json Item=nlohmann::json::object();
inline bool Open=false,Requested=false;
inline uint64_t Revision=0;
inline bool Valid(const nlohmann::json& row){
    const auto path=row.value("Path",std::string{});
    return !path.empty()&&path.size()<=1024&&path.front()=='/'&&path.find_first_of("\r\n\t'")==path.npos;
}
inline void Show(const nlohmann::json& row){if(!Valid(row))return;Item=row;Open=true;Requested=false;++Revision;}
}
