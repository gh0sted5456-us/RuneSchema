#pragma once
#include "Core/MapProjection.h"
#include <algorithm>
namespace PS::MapProjection {
// Preserve native map proportions in a rectangular viewport, including when
// Fit world letterboxes the texture. Pixels and world coordinates share one view.
struct Viewport {
    double Width=1,Height=1,MapAspect=1,Zoom=1;
    Point Center{0.5,0.5};
    double Ratio()const{return std::max(1.0,Width)/(std::max(1.0,Height)*std::max(0.001,MapAspect));}
    double FitZoom()const{const auto ratio=Ratio();return std::min(ratio,1/ratio);}
    Point Span()const{const auto ratio=Ratio(),zoom=std::max(0.001,Zoom);return ratio>=1?Point{1/zoom,1/(ratio*zoom)}:Point{ratio/zoom,1/zoom};}
    void Clamp(){Zoom=std::clamp(Zoom,FitZoom(),32.0);const auto span=Span();
        Center.X=span.X>=1?0.5:std::clamp(Center.X,span.X/2,1-span.X/2);
        Center.Y=span.Y>=1?0.5:std::clamp(Center.Y,span.Y/2,1-span.Y/2);}
    Point ToUV(Point pixel)const{const auto span=Span();return {Center.X+(pixel.X/Width-0.5)*span.X,Center.Y+(pixel.Y/Height-0.5)*span.Y};}
    Point ToPixel(Point uv)const{const auto span=Span();return {((uv.X-Center.X)/span.X+0.5)*Width,((uv.Y-Center.Y)/span.Y+0.5)*Height};}
    void ZoomAt(double value,Point pixel){const auto anchor=ToUV(pixel);Zoom=std::clamp(value,FitZoom(),32.0);const auto span=Span();
        Center={anchor.X-(pixel.X/Width-0.5)*span.X,anchor.Y-(pixel.Y/Height-0.5)*span.Y};Clamp();}
    void Pan(Point delta){const auto span=Span();Center.X-=delta.X*span.X/Width;Center.Y-=delta.Y*span.Y/Height;Clamp();}
    static bool InMap(Point uv){return std::isfinite(uv.X)&&std::isfinite(uv.Y)&&uv.X>=0&&uv.X<=1&&uv.Y>=0&&uv.Y<=1;}
};
}
