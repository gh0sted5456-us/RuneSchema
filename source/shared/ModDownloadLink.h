#pragma once
#include <algorithm>
#include <string_view>

namespace PS::ModDownloadLink {
inline bool Valid(std::string_view url){
    const size_t start=url.starts_with("https://")?8:url.starts_with("http://")?7:0;
    if(!start||url.size()>2048||std::any_of(url.begin(),url.end(),[](unsigned char c){return c<=32||c==127;})
        ||url.find_first_of("\\\"<>")!=url.npos)return false;
    const auto host=url.substr(start,url.find_first_of("/?#",start)-start);
    return !host.empty()&&host.find('@')==host.npos;
}
}
