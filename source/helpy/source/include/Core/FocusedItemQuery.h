#pragma once
#include <map>
#include <string>
#include <nlohmann/json.hpp>
namespace PS::FocusedItemQuery {
class Misses {
    std::string Scope;std::map<std::string,bool> HadRows;
public:
    bool NeedsLive(const std::string& scope,const std::string& query,const nlohmann::json& page){
        if(Scope!=scope){Scope=scope;HadRows.clear();}
        if(query.empty())return false;
        if(HadRows.size()>=64&&!HadRows.contains(query))HadRows.erase(HadRows.begin());
        auto& seen=HadRows[query];seen=seen||(page.contains("Rows")&&page["Rows"].is_array()&&!page["Rows"].empty());
        return !seen&&!page.value("HasMore",true);
    }
};
}
