#pragma once
#include "Core/SpawnCatalogSearch.h"
#include <set>
#include <algorithm>
#include <stdexcept>
namespace PS::SpawnBrowserPreferences {
// Native class identity survives changing loaded/mounted catalog keys. These
// preferences never remove a definition or modify spawn permission/cache data.
inline std::string Identity(const nlohmann::json& row){
    const auto type=SpawnCatalogSearch::Text(row,"Type");
    if(type!="Resource"&&type!="NPC"&&type!="AI")return {};
    auto path=SpawnCatalogSearch::Text(row,"Class");
    if(path.empty())path=SpawnCatalogSearch::Text(row,"Key");
    if(path.empty()||path.size()>2048||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))return {};
    return type+"|"+SpawnCatalogSearch::Fold(path);
}
struct Choices {
    static constexpr size_t Limit=1024;
    std::set<std::string> Hidden;
    static bool Valid(const std::string& id){
        return id.size()<=2057&&(id.starts_with("Resource|")||id.starts_with("NPC|")||id.starts_with("AI|"))
            &&id.back()!='|'&&std::none_of(id.begin(),id.end(),[](unsigned char c){return c<32||c==127;});
    }
    bool Contains(const nlohmann::json& row)const{return Hidden.contains(Identity(row));}
    bool Set(const nlohmann::json& row,bool hide){
        const auto id=Identity(row);if(id.empty())return false;
        if(!hide){Hidden.erase(id);return true;}
        if(Hidden.size()>=Limit&&!Hidden.contains(id))return false;
        Hidden.insert(id);return true;
    }
    bool Visible(const nlohmann::json& row,bool showHidden)const{return showHidden||!Contains(row);}
    nlohmann::json Json()const{return {{"Version",1},{"Hidden",Hidden}};}
    static Choices Parse(const nlohmann::json& document){
        if(!document.is_object()||!document.contains("Version")||!document["Version"].is_number_integer()||document["Version"]!=1
            ||!document.contains("Hidden")||!document["Hidden"].is_array()||document["Hidden"].size()>Limit)throw std::runtime_error("Invalid spawn-browser preferences; existing file retained");
        Choices out;for(const auto& id:document["Hidden"]){
            if(!id.is_string()||!Valid(id.get<std::string>()))throw std::runtime_error("Invalid hidden spawn identity; existing file retained");
            out.Hidden.insert(id.get<std::string>());
        }return out;
    }
};
}
