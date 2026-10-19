#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <set>
#include <string>
namespace PS::MappingRetention {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline bool Hex(const std::string& value){return value.size()==16&&value.find_first_not_of("0123456789abcdef")==value.npos;}
inline bool Identity(const Json& value){return value.is_object()&&value.contains("Build")&&value["Build"].is_string()&&Hex(value["Build"].get<std::string>())&&value.contains("Fingerprint")&&value["Fingerprint"].is_string()&&Hex(value["Fingerprint"].get<std::string>());}
inline fs::path File(const fs::path& root,const Json& value){if(!Identity(value))throw std::runtime_error("Invalid generated mapping identity");return root/value.at("Build").get<std::string>()/(value.at("Fingerprint").get<std::string>()+".usmap");}
inline Json Retired(const Json& previous,const Json& current){
    Json result=Json::array();std::set<std::string> seen;const auto add=[&](const Json& row){if(!Identity(row)||!Identity(current)||File({},row)==File({},current))return;const auto key=File({},row).generic_string();if(result.size()<8&&seen.insert(key).second)result.push_back({{"Build",row.at("Build")},{"Fingerprint",row.at("Fingerprint")}});};
    add(previous);if(previous.is_object()&&previous.contains("Retired")&&previous["Retired"].is_array()&&previous["Retired"].size()<=8)for(const auto& row:previous["Retired"])add(row);return result;
}
template<class Validate,class Safe>size_t RemoveRetired(const fs::path& root,const Json& receipt,Validate validate,Safe safe){
    if(!Identity(receipt)||!receipt.contains("Retired")||!receipt["Retired"].is_array()||receipt["Retired"].size()>8)return 0;
    const auto current=File(root,receipt);safe(current);validate(current,receipt.at("Fingerprint").get_ref<const std::string&>());size_t removed=0;
    for(const auto& identity:receipt["Retired"]){if(!Identity(identity))continue;const auto file=File(root,identity);if(file.lexically_normal()==current.lexically_normal())continue;safe(file);
        if(file.parent_path().parent_path().lexically_normal()!=root.lexically_normal()||file.extension()!=L".usmap")throw std::runtime_error("Generated mapping cleanup escaped its owned cache");
        if(fs::is_regular_file(file)&&fs::remove(file))++removed;
    }return removed;
}
}
