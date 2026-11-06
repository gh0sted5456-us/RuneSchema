#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
namespace PS::ModMenuColor {
inline std::optional<std::array<float,4>> Decode(std::string_view text){
    if((text.size()!=7&&text.size()!=9)||text.front()!='#')return {};
    const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    std::array<float,4> result{0,0,0,1};
    for(size_t i=1;i<text.size();i+=2){const int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0)return {};result[(i-1)/2]=float(a*16+b)/255.f;}
    return result;
}
inline bool Valid(std::string_view text,bool alpha){return text.size()==(alpha?9u:7u)&&Decode(text).has_value();}
inline std::string Encode(const std::array<float,4>& value,bool alpha){
    std::array<unsigned,4> bytes{};for(size_t i=0;i<bytes.size();++i){if(!std::isfinite(value[i]))return {};bytes[i]=unsigned(std::lround(std::clamp(value[i],0.f,1.f)*255.f));}
    char text[10]{};if(alpha)std::snprintf(text,sizeof(text),"#%02X%02X%02X%02X",bytes[0],bytes[1],bytes[2],bytes[3]);else std::snprintf(text,sizeof(text),"#%02X%02X%02X",bytes[0],bytes[1],bytes[2]);return text;
}
}
