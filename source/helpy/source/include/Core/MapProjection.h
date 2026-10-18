#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <nlohmann/json.hpp>
namespace PS::MapProjection {
using Json=nlohmann::json;
struct Point {double X=0,Y=0;};
struct Projection {
    Point Center,Right,Up;double HalfWidth=0,HalfHeight=0,MinZ=0,MaxZ=0;
    bool Valid()const{return std::isfinite(Center.X)&&std::isfinite(Center.Y)&&std::isfinite(HalfWidth)&&std::isfinite(HalfHeight)&&
        HalfWidth>1&&HalfHeight>1&&HalfWidth<10000000&&HalfHeight<10000000&&std::isfinite(MinZ)&&std::isfinite(MaxZ)&&MinZ<MaxZ&&
        std::abs(Right.X*Right.X+Right.Y*Right.Y-1)<0.001&&std::abs(Up.X*Up.X+Up.Y*Up.Y-1)<0.001&&std::abs(Right.X*Up.X+Right.Y*Up.Y)<0.001;}
    Point ToUV(Point p)const {if(!Valid())throw std::runtime_error("Native map alignment is unavailable");const auto x=p.X-Center.X,y=p.Y-Center.Y;return {0.5+(x*Right.X+y*Right.Y)/(2*HalfWidth),0.5-(x*Up.X+y*Up.Y)/(2*HalfHeight)};}
    Point ToWorld(Point uv)const {if(!Valid()||!std::isfinite(uv.X)||!std::isfinite(uv.Y)||uv.X<0||uv.X>1||uv.Y<0||uv.Y>1)throw std::runtime_error("Choose a point inside the world map");
        const auto x=(uv.X-0.5)*2*HalfWidth,y=(0.5-uv.Y)*2*HalfHeight;return {Center.X+x*Right.X+y*Up.X,Center.Y+x*Right.Y+y*Up.Y};}
    bool Contains(Point p)const {if(!std::isfinite(p.X)||!std::isfinite(p.Y)||!Valid())return false;const auto uv=ToUV(p);return uv.X>=0&&uv.X<=1&&uv.Y>=0&&uv.Y<=1;}
    Json ToJson()const{return {{"Center",{Center.X,Center.Y}},{"Right",{Right.X,Right.Y}},{"Up",{Up.X,Up.Y}},{"HalfWidth",HalfWidth},{"HalfHeight",HalfHeight},{"MinZ",MinZ},{"MaxZ",MaxZ}};}
    static Projection From(const Json& value){Projection p;if(!value.is_object())return p;try{
        p.Center={value.at("Center").at(0).get<double>(),value.at("Center").at(1).get<double>()};p.Right={value.at("Right").at(0).get<double>(),value.at("Right").at(1).get<double>()};
        p.Up={value.at("Up").at(0).get<double>(),value.at("Up").at(1).get<double>()};p.HalfWidth=value.at("HalfWidth").get<double>();p.HalfHeight=value.at("HalfHeight").get<double>();p.MinZ=value.at("MinZ").get<double>();p.MaxZ=value.at("MaxZ").get<double>();
    }catch(...){return {};}return p;}
};
}
