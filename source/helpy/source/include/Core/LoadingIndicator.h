#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace PS::LoadingIndicator {
struct Point {float X,Y;};
struct StartupFrame {float RuneAlpha,Angle;};
inline StartupFrame Startup(double elapsed,bool animated){
    if(!animated)return {1.f,0.f};
    elapsed=std::isfinite(elapsed)?std::fmax(0.,elapsed):0.;
    const float phase=float(std::fmod(elapsed,6.));
    const float fade=phase<2.5f?0.f:phase<3.f?(phase-2.5f)*2:phase<5.5f?1.f:1.f-(phase-5.5f)*2;
    return {fade*fade*(3-2*fade),float(std::fmod(elapsed,2.)*3.141592653589793)};
}
struct StartupLayout {float Width=0,Height=0,RuneSize=0,GameWidth=0,LogoY=0,WheelY=0,WheelRadius=0,TextY=0,TipY=0;};
inline StartupLayout FitStartup(float width,float height,float scale,float fontScale=1){
    if(width<1||height<1)return {};
    const float unit=std::min({std::clamp(scale,.75f,3.f),width/800.f,height/600.f});
    const float padding=16*unit,rune=260*unit,game=std::min(720*unit,width*.78f),radius=24*unit;
    const float wheelY=padding+rune+28*unit+radius,textY=wheelY+radius+16*unit;
    const float tipY=textY+24*unit*std::clamp(fontScale,.5f,3.f)+14*unit;
    return {std::max(game,rune)+2*padding,tipY+80*unit+padding,rune,game,padding+rune*.5f,wheelY,radius,textY,tipY};
}
inline int StartupFps(bool animated){return animated?60:2;}
inline std::array<Point,4> Quad(Point center,float radius,float angle){
    const float c=std::cos(angle),s=std::sin(angle);
    std::array<Point,4> points{{{-radius,-radius},{radius,-radius},{radius,radius},{-radius,radius}}};
    for(auto& p:points)p={center.X+p.X*c-p.Y*s,center.Y+p.X*s+p.Y*c};
    return points;
}
}
