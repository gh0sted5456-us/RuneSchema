#pragma once
#include "Generator/AppearanceSignatures.h"
#include <algorithm>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace PS::AppearanceResolver {
struct Section { size_t offset, size; bool executable; };
inline unsigned Hex(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    throw std::runtime_error("Invalid appearance signature encoding");
}
inline std::vector<uint8_t> Decode(std::string_view value) {
    if(value.size()%2)throw std::runtime_error("Invalid appearance signature length");
    std::vector<uint8_t> result(value.size()/2);
    for(size_t i=0;i<result.size();++i)result[i]=static_cast<uint8_t>((Hex(value[i*2])<<4)|Hex(value[i*2+1]));
    return result;
}
inline size_t Resolve(std::span<const uint8_t> image,std::span<const Section> sections,
                      const AppearanceSignatures::Signature& signature) {
    const auto code=Decode(signature.code),mask=Decode(signature.mask);
    if(code.empty() || code.size()!=mask.size() || signature.hookOffset>=code.size())
        throw std::runtime_error("Invalid appearance signature contract");
    size_t anchor=0,length=0;
    for(size_t i=0;i<mask.size();) {
        if(mask[i]!=255) { ++i;continue; }
        const auto start=i;
        while(i<mask.size() && mask[i]==255)++i;
        if(i-start>length){anchor=start;length=i-start;}
    }
    if(length<8)throw std::runtime_error("Appearance signature anchor too short");
    auto inSection=[&](size_t target,bool executable) {
        for(const auto& s:sections)
            if(s.offset<=image.size() && s.size<=image.size()-s.offset
               && (!executable || s.executable) && target>=s.offset && target-s.offset<s.size)return true;
        return false;
    };
    size_t found=0,count=0;
    for(const auto& section:sections) {
        if(section.offset>image.size() || section.size>image.size()-section.offset)
            throw std::runtime_error("Invalid appearance image section");
        if(!section.executable || section.size<code.size())continue;
        const auto* begin=image.data()+section.offset;
        const auto* end=begin+section.size;
        for(auto* search=begin;search<end;) {
            const auto* match=std::search(search,end,code.data()+anchor,code.data()+anchor+length);
            if(match==end)break;
            search=match+1;
            if(static_cast<size_t>(match-begin)<anchor)continue;
            const auto* candidate=match-anchor;
            if(static_cast<size_t>(end-candidate)<code.size())continue;
            bool valid=true;
            for(size_t i=0;i<code.size();++i)if(mask[i] && candidate[i]!=code[i]){valid=false;break;}
            if(!valid)continue;
            const auto rva=static_cast<size_t>(candidate-image.data());
            for(size_t i=0;i<signature.targetCount;++i) {
                const auto& target=signature.targets[i];
                if(target.offset>code.size() || code.size()-target.offset<4)
                    throw std::runtime_error("Invalid appearance relocation contract");
                int32_t displacement;std::memcpy(&displacement,candidate+target.offset,4);
                const auto destination=static_cast<int64_t>(rva)+static_cast<int64_t>(target.offset)+static_cast<int64_t>(target.next)+displacement;
                if(destination<0 || !inSection(static_cast<size_t>(destination),target.executable)){valid=false;break;}
            }
            if(valid) { found=rva+signature.hookOffset;if(++count>1)break; }
        }
        if(count>1)break;
    }
    if(count!=1)throw std::runtime_error(std::string("Appearance hook unavailable or ambiguous: ")+signature.name);
    return found;
}
}
