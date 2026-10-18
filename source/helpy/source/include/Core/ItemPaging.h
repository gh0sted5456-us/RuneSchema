#pragma once
#include <algorithm>
#include <cstddef>

namespace PS::ItemPaging {
inline constexpr int DefaultSize=20,MaximumSize=100;
inline int Size(int value){return std::clamp(value,1,MaximumSize);}
struct Window {std::size_t First=0,Count=0;bool Previous=false,Next=false;};
inline Window Slice(std::size_t loaded,int page,int size,bool more) {
    const auto width=static_cast<std::size_t>(Size(size));
    const auto first=static_cast<std::size_t>(std::max(0,page))*width;
    const auto count=first<loaded?std::min(width,loaded-first):0;
    return {first,count,page>0,first+count<loaded||more};
}
inline int Resize(int page,int oldSize,int newSize) {
    return static_cast<int>((static_cast<std::size_t>(std::max(0,page))*Size(oldSize))/Size(newSize));
}
inline float CartHeight(std::size_t count,bool advanced){return count==0?(advanced?132.0f:92.0f):(advanced?250.0f:212.0f);}
}
