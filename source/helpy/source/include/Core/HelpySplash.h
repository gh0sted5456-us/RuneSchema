#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace PS::HelpySplash {
inline constexpr std::array Tips{
    "Tip: Import an icon cache once; matching icons are reused next launch.",
    "Tip: Mods can provide cache packs in their own /cache folder.",
    "Tip: Cached items stay browsable while mounted content is checked.",
    "Tip: A green cart check verifies the item; delivery still checks inventory.",
    "Tip: Hover the loading wheel to see exactly what Helpy is doing.",
    "Tip: Choose which workspaces appear on Home in Settings.",
    "Tip: Minimize detached windows to Helpy's taskbar without losing your work.",
    "Tip: Initial Assets includes map filters; item-icon caches stay separate."
};
inline size_t TipIndex(double elapsed){if(!std::isfinite(elapsed)||elapsed<0)return 0;return static_cast<size_t>(std::fmod(std::floor(elapsed/7.0),static_cast<double>(Tips.size())));}
// Deliberate hold/crossfade/hold/crossfade, not a spinning logo or a cut.
inline constexpr double HoldSeconds=4.0,BlendSeconds=4.0,CycleSeconds=2*(HoldSeconds+BlendSeconds);
inline constexpr double ExitSeconds=.65,RevealSeconds=.18;
inline float Smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
enum class ExitKind {Dragonwilds,RuneSchema};
struct ExitFrame {float Progress=0,Travel=0,Fade=1,RuneOpacity=0,GameOpacity=1;bool Finished=false;};
struct Exit {
    bool Active=false;ExitKind Kind=ExitKind::Dragonwilds;double Started=0;float InitialBlend=0;
    bool Begin(double now,float blend){
        if(Active)return false;Active=true;Started=now;InitialBlend=std::clamp(blend,0.f,1.f);Kind=InitialBlend>=.5f?ExitKind::RuneSchema:ExitKind::Dragonwilds;return true;
    }
    ExitFrame At(double now,bool animate=true)const{
        const float t=animate?static_cast<float>(std::clamp((now-Started)/ExitSeconds,0.0,1.0)):1;
        const float settle=Smooth(t/.2f);const float rune=Kind==ExitKind::RuneSchema?InitialBlend+(1-InitialBlend)*settle:InitialBlend*(1-settle);
        return {t,1-(1-t)*(1-t)*(1-t),1-Smooth(t),rune,1-rune,t>=1};
    }
};
// Grow the three artwork interiors back to their original outlines. Eroded
// seeds break thin shadow contacts between the R and bracket, without vertical
// cuts through diagonal strokes or inward tips. Done once at texture upload.
struct Part {int X=0,Y=0,Width=0,Height=0;std::vector<std::uint8_t> Pixels;};
inline std::array<Part,3> SplitLogo(const std::vector<std::uint8_t>& rgba,int width,int height){
    std::array<Part,3> result{};if(width<1||height<1||width>2048||height>2048||rgba.size()!=static_cast<size_t>(width)*height*4)return result;
    const size_t count=static_cast<size_t>(width)*height;bool transparent=false;
    for(size_t i=3;i<rgba.size();i+=4)if(rgba[i]==0){transparent=true;break;}
    const auto visible=[&](size_t i){return rgba[i*4+3]!=0&&(transparent||rgba[i*4]+rgba[i*4+1]+rgba[i*4+2]>3);};
    const auto strong=[&](int x,int y){if(x<0||x>=width||y<0||y>=height)return false;const auto i=(static_cast<size_t>(y)*width+x)*4;return rgba[i+3]>96&&std::max({rgba[i],rgba[i+1],rgba[i+2]})>32;};
    const int radius=std::min(width,height)>128?2:1;
    const auto interior=[&](size_t i){const int x=static_cast<int>(i%width),y=static_cast<int>(i/width);return strong(x,y)&&strong(x-radius,y)&&strong(x+radius,y)&&strong(x,y-radius)&&strong(x,y+radius);};
    const auto classify=[&](double x){return static_cast<std::uint8_t>(x/width<.25?0:x/width>.8?2:1);};
    std::vector<std::uint8_t> labels(count,255);std::vector<size_t> component,queue;std::array<std::vector<size_t>,3> seeds;
    for(size_t first=0;first<count;++first){
        if(labels[first]!=255||!interior(first))continue;component.clear();component.push_back(first);labels[first]=254;double sumX=0;
        for(size_t at=0;at<component.size();++at){const auto i=component[at];const int x=static_cast<int>(i%width),y=static_cast<int>(i/width);sumX+=x;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int nx=x+dx,ny=y+dy;if(nx<0||nx>=width||ny<0||ny>=height)continue;const auto next=static_cast<size_t>(ny)*width+nx;
                if(labels[next]==255&&interior(next)){labels[next]=254;component.push_back(next);}}
        }
        auto& seed=seeds[classify(sumX/component.size())];if(component.size()>seed.size())seed=component;
    }
    std::fill(labels.begin(),labels.end(),255);
    for(size_t p=0;p<3;++p)for(const auto i:seeds[p]){labels[i]=static_cast<std::uint8_t>(p);queue.push_back(i);}
    for(size_t at=0;at<queue.size();++at){const auto i=queue[at];const int x=static_cast<int>(i%width),y=static_cast<int>(i/width);
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int nx=x+dx,ny=y+dy;if(nx<0||nx>=width||ny<0||ny>=height)continue;const auto next=static_cast<size_t>(ny)*width+nx;
            if(labels[next]==255&&visible(next)){labels[next]=labels[i];queue.push_back(next);}}
    }
    std::array<int,3> left{width,width,width},top{height,height,height},right{},bottom{};
    // Preserve detached flecks and very small artwork too.
    for(size_t first=0;first<count;++first){
        if(labels[first]!=255||!visible(first))continue;component.clear();component.push_back(first);labels[first]=254;double sumX=0;
        for(size_t at=0;at<component.size();++at){const auto i=component[at];const int x=static_cast<int>(i%width),y=static_cast<int>(i/width);sumX+=x;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int nx=x+dx,ny=y+dy;if(nx<0||nx>=width||ny<0||ny>=height)continue;const auto next=static_cast<size_t>(ny)*width+nx;
                if(labels[next]==255&&visible(next)){labels[next]=254;component.push_back(next);}}
        }
        const auto part=classify(sumX/component.size());for(const auto i:component)labels[i]=part;
    }
    for(size_t i=0;i<count;++i)if(labels[i]<3){const auto p=labels[i];const int x=static_cast<int>(i%width),y=static_cast<int>(i/width);left[p]=std::min(left[p],x);right[p]=std::max(right[p],x+1);top[p]=std::min(top[p],y);bottom[p]=std::max(bottom[p],y+1);}
    for(size_t p=0;p<3;++p)if(right[p]>left[p]){auto& part=result[p];part.X=left[p];part.Y=top[p];part.Width=right[p]-left[p];part.Height=bottom[p]-top[p];part.Pixels.resize(static_cast<size_t>(part.Width)*part.Height*4);
        for(int y=0;y<part.Height;++y)for(int x=0;x<part.Width;++x){const auto i=static_cast<size_t>(part.Y+y)*width+part.X+x;if(labels[i]==p)std::copy_n(rgba.data()+i*4,4,part.Pixels.data()+(static_cast<size_t>(y)*part.Width+x)*4);}
    }
    return result;
}
inline float Blend(double elapsed,bool animate=true){
    if(!animate)return 1;
    if(!std::isfinite(elapsed)||elapsed<0)elapsed=0;
    const auto phase=std::fmod(elapsed,CycleSeconds);
    const auto smooth=[](double value){const auto t=static_cast<float>(value);return t*t*(3-2*t);};
    if(phase<HoldSeconds)return 0;
    if(phase<HoldSeconds+BlendSeconds)return smooth((phase-HoldSeconds)/BlendSeconds);
    if(phase<2*HoldSeconds+BlendSeconds)return 1;
    return 1-smooth((phase-(2*HoldSeconds+BlendSeconds))/BlendSeconds);
}
}
