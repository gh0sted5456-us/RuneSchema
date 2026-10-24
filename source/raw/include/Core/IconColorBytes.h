#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
namespace PS::IconCapture {
inline std::optional<std::vector<unsigned char>> ColorBytes(std::span<const unsigned char> samples,size_t count,size_t stride,const std::array<int,4>& channels){
    if(!count||count>4096||!stride||stride>64||samples.size()<count*stride)return {};
    for(size_t i=0;i<channels.size();++i){
        if(channels[i]<0||static_cast<size_t>(channels[i])>=stride)return {};
        for(size_t j=0;j<i;++j)if(channels[i]==channels[j])return {};
    }
    std::vector<unsigned char> rgba(count*4);
    for(size_t i=0;i<count;++i)for(size_t channel=0;channel<4;++channel)rgba[i*4+channel]=samples[i*stride+channels[channel]];
    return rgba;
}
}
