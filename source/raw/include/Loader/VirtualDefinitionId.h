#pragma once

#include <string>
#include <string_view>

namespace DragonWilds::VirtualDefinitionId {

// Authored definitions are addressed through a virtual namespace rather than
// a filesystem path: ModName:Category/Id. The category and id are deliberately
// opaque so mod authors can organize them without creating runtime folders.
inline bool IsCanonical(std::string_view value) {
    const auto separator=value.find(':');
    if(separator==std::string_view::npos || separator==0 || separator+1>=value.size())return false;
    if(value.find(':',separator+1)!=std::string_view::npos)return false;
    if(value.find("..")!=std::string_view::npos || value.find("//")!=std::string_view::npos || value.substr(0,separator).find('/')!=std::string_view::npos)return false;
    for(std::size_t i=0;i<value.size();++i) {
        const auto c=value[i];
        if(i==separator)continue;
        const bool allowed=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'||c=='.';
        if(!allowed)return false;
    }
    return value[separator+1]!='/' && value.back()!='/' ;
}

inline std::string Qualify(std::string_view owner,std::string_view key) {
    if(key.find(':')!=std::string_view::npos)return std::string(key);
    if(owner.empty())return std::string(key);
    return std::string(owner)+":"+std::string(key);
}

}
