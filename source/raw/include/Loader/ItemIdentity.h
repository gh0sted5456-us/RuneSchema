#pragma once
#include <string_view>
namespace DragonWilds {
inline bool IsCanonicalPersistenceId(std::string_view value) {
    if(value.size()!=22)return false;
    for(const char c:value)
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_'))return false;
    const auto last=value.back();
    return last=='A' || last=='Q' || last=='g' || last=='w';
}
}
