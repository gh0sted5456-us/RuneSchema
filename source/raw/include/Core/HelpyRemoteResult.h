#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>
namespace PS::HelpyRemoteResult {
inline constexpr size_t ChunkBytes=1400,MaxParts=128;
inline std::vector<nlohmann::json> Split(nlohmann::json result){
    const auto text=result.dump(-1,' ',true);
    if(text.empty()||text.size()>ChunkBytes*MaxParts)throw std::runtime_error("Remote receipt exceeds its bounded transport");
    const auto count=(text.size()+ChunkBytes-1)/ChunkBytes;std::vector<nlohmann::json> out;
    for(size_t i=0;i<count;++i)out.push_back({{"Part",i},{"Parts",count},{"Data",text.substr(i*ChunkBytes,ChunkBytes)}});
    return out;
}
class Assembly {
    std::vector<std::optional<std::string>> m_parts;
    size_t m_count=0;
public:
    void Reset(){m_parts.clear();m_count=0;}
    std::optional<nlohmann::json> Accept(const nlohmann::json& packet){
        const auto count=packet.at("Parts").get<size_t>(),index=packet.at("Part").get<size_t>();
        const auto& data=packet.at("Data").get_ref<const std::string&>();
        if(!count||count>MaxParts||index>=count||data.empty()||data.size()>ChunkBytes)
            throw std::runtime_error("Invalid bounded remote receipt part");
        if(m_parts.empty())m_parts.resize(count);
        if(m_parts.size()!=count)throw std::runtime_error("Remote receipt part count changed");
        if(m_parts[index]){if(*m_parts[index]!=data)throw std::runtime_error("Conflicting remote receipt part");return {};}
        m_parts[index]=data;if(++m_count!=count)return {};
        std::string joined;for(const auto& part:m_parts)joined+=*part;
        auto result=nlohmann::json::parse(joined,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
            if(depth>16)throw std::runtime_error("Remote receipt nesting exceeds its limit");return true;});
        if(!result.is_object()||result.size()>4||!result.contains("Status")||!result["Status"].is_string())
            throw std::runtime_error("Invalid remote receipt object");
        if(result.contains("GrantResults")&&(!result["GrantResults"].is_array()||result["GrantResults"].size()>64))
            throw std::runtime_error("Invalid remote grant receipt rows");
        Reset();return result;
    }
};
}
