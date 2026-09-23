#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <stdexcept>
namespace PS::PlayerTrace {
inline std::string LowerFilter(std::string value) {
    for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline void ValidateFilterTerms(const nlohmann::json& value) {
    if(!value.is_array() || value.size()>16)throw std::runtime_error("Trace filters require an array of at most 16 terms");
    for(const auto& term:value) {
        if(!term.is_string())throw std::runtime_error("Trace filter terms must be strings");
        const auto& text=term.get_ref<const std::string&>();
        if(text.empty() || text.size()>128 || std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32;})
            || text.find_first_not_of(' ')==text.npos)throw std::runtime_error("Trace filter terms require 1..128 non-control characters");
    }
}
inline nlohmann::json FilterLines(const std::string& input) {
    auto result=nlohmann::json::array();size_t start=0;
    while(start<input.size()) {
        const auto end=input.find('\n',start);auto line=input.substr(start,end==input.npos?input.size()-start:end-start);
        const auto first=line.find_first_not_of(" \t\r"),last=line.find_last_not_of(" \t\r");
        if(first!=line.npos)result.push_back(line.substr(first,last-first+1));
        if(end==input.npos)break;start=end+1;
    }
    ValidateFilterTerms(result);return result;
}
inline std::string FilterText(const nlohmann::json& value) {
    ValidateFilterTerms(value);std::string text;
    for(const auto& term:value){if(!text.empty())text+='\n';text+=term.get<std::string>();}
    return text;
}
struct NameFilters {
    std::vector<std::string> Include,Exclude;
    explicit NameFilters(const nlohmann::json& options=nlohmann::json::object()) {
        const auto parse=[&](const char* key,auto& output){
            if(!options.contains(key))return;
            ValidateFilterTerms(options.at(key));
            for(const auto& term:options.at(key))output.push_back(LowerFilter(term.template get<std::string>()));
        };
        parse("IncludeAny",Include);parse("ExcludeAny",Exclude);
    }
    bool MatchesLower(const std::string& name) const {
        for(const auto& term:Exclude)if(name.find(term)!=name.npos)return false;
        if(Include.empty())return true;
        for(const auto& term:Include)if(name.find(term)!=name.npos)return true;
        return false;
    }
};
}
