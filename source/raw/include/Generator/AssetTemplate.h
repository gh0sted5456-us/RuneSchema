#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <sstream>
#include <cctype>
#include <stdexcept>

namespace PS::AssetTemplate {
inline std::string Fold(std::string value) {
    std::string result;
    for(unsigned char c:value)if(c<128 && std::isalnum(c))result+=static_cast<char>(std::tolower(c));
    return result;
}
inline bool Matches(const std::string& query,const std::string& candidate) {
    auto haystack=Fold(candidate);std::istringstream words(query);std::string word;bool any=false;
    while(words>>word) {word=Fold(word);if(word.empty())continue;any=true;if(haystack.find(word)==haystack.npos)return false;}
    return any;
}
inline void Identifier(const std::string& name) {
    if(name.empty() || name.size()>64 || name.front()=='_' || name.back()=='_')throw std::runtime_error("Use 1-64 letters, digits or underscores; start and end with a letter or digit.");
    for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))throw std::runtime_error("Invalid mod or item name.");
    if(name.find("__")!=name.npos)throw std::runtime_error("Use single underscores in names.");
}
inline nlohmann::json Build(const std::string& source,nlohmann::json fields,int mode,
    const std::string& mod,const std::string& name,const std::string& id) {
    using nlohmann::json;
    if(source.empty() || source.front()!='/' || !fields.is_object() || mode<0 || mode>2)throw std::runtime_error("Invalid asset template.");
    fields.erase("InternalName");fields.erase("PersistenceID");
    for(const auto& [key,value]:fields.items())if(key.starts_with('$'))throw std::runtime_error("Template fields cannot contain directives.");
    if(mode==1)return {{"Patch",{{"$Patch",source},{"$Target",fields}}}};
    if(mode==0)return {{source,fields}};
    Identifier(mod);Identifier(name);
    if(id.size()!=22 || std::string("AQgw").find(id.back())==std::string::npos)throw std::runtime_error("Invalid clone PersistenceID.");
    for(unsigned char c:id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))throw std::runtime_error("Invalid clone PersistenceID.");
    fields["$Clone"]=source;fields["InternalName"]=name;fields["PersistenceID"]=id;
    return {{"/Game/RuneSchema/"+mod+"/Items/"+name+"."+name,fields}};
}
}
