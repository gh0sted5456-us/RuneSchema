#pragma once
#include "Core/PortraitGlyphs.h"
#include "Generator/HelpyImGuiTheme.h"
namespace PS::PortraitArt {
inline void Draw(int kind,ImVec2 min,ImVec2 max,ImU32 color){
    auto* draw=ImGui::GetWindowDrawList();const float size=std::min(max.x-min.x,max.y-min.y),scale=size/26;const ImVec2 origin{min.x+(max.x-min.x-size)*.5f+scale,min.y+(max.y-min.y-size)*.5f+scale};
    struct Sink{ImDrawList* Draw;ImVec2 Origin;float Scale;ImU32 Color;bool Active=false;
        ImVec2 At(float x,float y){return {Origin.x+x*Scale,Origin.y+y*Scale};}
        void Finish(){if(Active)Draw->PathStroke(Color,0,std::max(1.f,Scale*.5f));Active=false;}
        void Move(float x,float y){Finish();Draw->PathLineTo(At(x,y));Active=true;}void Line(float x,float y){Draw->PathLineTo(At(x,y));}
        void Quad(float x,float y,float a,float b){Draw->PathBezierQuadraticCurveTo(At(x,y),At(a,b));}
        void Cubic(float x,float y,float a,float b,float c,float d){Draw->PathBezierCubicCurveTo(At(x,y),At(a,b),At(c,d));}
        void Close(){Draw->PathStroke(Color,ImDrawFlags_Closed,std::max(1.f,Scale*.5f));Active=false;}
    } sink{draw,origin,scale,color};PortraitGlyphs::Replay(size_t(std::clamp(kind,0,3)),sink);
}
inline int Kind(const std::string& path){for(size_t i=0;i<PortraitGlyphs::All.size();++i)if(path==std::string("/HelpyGlyph/")+PortraitGlyphs::All[i].Id+"."+PortraitGlyphs::All[i].Id)return int(i);return -1;}
}
