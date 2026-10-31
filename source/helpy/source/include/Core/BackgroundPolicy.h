#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
namespace PS::BackgroundPolicy {
inline bool Filename(const std::string& name){
    if(name.empty()||name.size()>255||name.find("..")!=name.npos||name.find_first_of("/\\:")!=name.npos||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))return false;
    auto lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return lower.ends_with(".png")||lower.ends_with(".jpg")||lower.ends_with(".jpeg")||lower.ends_with(".bmp");
}
struct Crop {float Left=0,Top=0,Right=1,Bottom=1;};
inline Crop Cover(float imageWidth,float imageHeight,float viewWidth,float viewHeight){
    Crop result;if(imageWidth<=0||imageHeight<=0||viewWidth<=0||viewHeight<=0)return result;
    const auto image=imageWidth/imageHeight,view=viewWidth/viewHeight;
    if(image>view){const auto span=view/image;result.Left=(1-span)*.5f;result.Right=1-result.Left;}
    else{const auto span=image/view;result.Top=(1-span)*.5f;result.Bottom=1-result.Top;}return result;
}
inline float Strength(float value){return std::clamp(value,0.f,.3f);}
struct Layers {float Base,Image;};
inline Layers TintLayers(float opacity,float strength){
    opacity=std::clamp(opacity,0.f,1.f);const float image=opacity*Strength(strength);
    return {(opacity-image)/(1.f-image),image};
}
}
