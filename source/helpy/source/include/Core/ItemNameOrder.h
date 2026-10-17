#pragma once
#include <string>
#include <cctype>
#include <utility>
namespace PS::ItemNameOrder {
// ASCII case-insensitive A-Z; UTF-8 bytes are preserved, not locale-mutated.
inline std::pair<std::string,std::string> Key(std::string name,std::string path){
    const auto first=name.find_first_not_of(" \t\r\n\f\v"),last=name.find_last_not_of(" \t\r\n\f\v");
    name=first==std::string::npos?std::string{}:name.substr(first,last-first+1);
    for(auto& ch:name)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');
    return {std::move(name),std::move(path)};
}
}
