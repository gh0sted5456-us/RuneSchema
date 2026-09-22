#pragma once
#include "Generator/AppearanceResolver.h"

namespace PS::AppearanceResolver {
// Generic template helpers can have byte-identical copies. Resolve one only
// through a uniquely validated caller, then validate its complete callee body.
inline size_t ResolveCalled(std::span<const uint8_t> image,std::span<const Section> sections,
    const AppearanceSignatures::Signature& caller,size_t callOffset,
    const AppearanceSignatures::Signature& callee) {
    if(caller.hookOffset || callee.hookOffset)throw std::runtime_error("Native call requires function-entry contracts");
    const auto start=Resolve(image,sections,caller);
    const auto callerSize=caller.code.size()/2;
    if(callOffset>callerSize || callerSize-callOffset<5 || image[start+callOffset]!=0xe8)
        throw std::runtime_error("Native call instruction changed");
    int32_t displacement;std::memcpy(&displacement,image.data()+start+callOffset+1,4);
    const auto target=static_cast<int64_t>(start)+static_cast<int64_t>(callOffset)+5+displacement;
    const auto code=Decode(callee.code),mask=Decode(callee.mask);
    if(target<0 || code.empty() || code.size()!=mask.size()
        || static_cast<uint64_t>(target)>image.size() || code.size()>image.size()-static_cast<size_t>(target))
        throw std::runtime_error("Native callee bounds invalid");
    const auto covered=[&](size_t address,size_t length,bool executable) {
        return std::any_of(sections.begin(),sections.end(),[&](const auto& section){
            return section.offset<=image.size() && section.size<=image.size()-section.offset
                && (!executable || section.executable) && address>=section.offset
                && address-section.offset<=section.size && length<=section.size-(address-section.offset);
        });
    };
    const auto rva=static_cast<size_t>(target);
    if(!covered(rva,code.size(),true))throw std::runtime_error("Native callee is not executable");
    for(size_t i=0;i<code.size();++i)if(mask[i] && image[rva+i]!=code[i])
        throw std::runtime_error("Native callee signature changed");
    for(size_t i=0;i<callee.targetCount;++i) {
        const auto& reference=callee.targets[i];
        if(reference.offset>code.size() || code.size()-reference.offset<4)
            throw std::runtime_error("Native callee relocation invalid");
        int32_t relative;std::memcpy(&relative,image.data()+rva+reference.offset,4);
        const auto destination=target+static_cast<int64_t>(reference.offset)+static_cast<int64_t>(reference.next)+relative;
        if(destination<0 || !covered(static_cast<size_t>(destination),1,reference.executable))
            throw std::runtime_error("Native callee target outside image");
    }
    return rva;
}
}
