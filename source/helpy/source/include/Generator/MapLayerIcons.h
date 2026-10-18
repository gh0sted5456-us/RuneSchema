#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyGlyphs.h"
#include "Runtime/HelpyWikiIcons.h"
namespace PS::MapLayerIcons {
// Original Helpy glyphs. The dataset's CC licence does not grant ownership of
// Wildsforge's separately authored SVG icons. No network request per marker.
inline void Draw(std::string_view group,ImVec2 center,float size,bool mixed=false){
    auto* d=ImGui::GetWindowDrawList();const auto& t=HelpyImGuiTheme::Current;const auto color=ImGui::GetColorU32(t.accentHover);
    const float r=size*.5f,w=std::max(1.0f,size*.075f);
    const auto p=[&](float x,float y){return ImVec2{center.x+x*r,center.y+y*r};};
    d->AddCircleFilled(center,r,IM_COL32(12,13,15,235));
    if(mixed){d->AddRect(p(-.55f,-.5f),p(.25f,.3f),color,0,0,w);d->AddRect(p(-.25f,-.2f),p(.55f,.6f),color,0,0,w);return;}
    struct Sink{
        ImDrawList* Draw;ImVec2 Center;float Scale,Width;ImU32 Color;bool Active=false;
        ImVec2 At(float x,float y)const{return {Center.x+(x-12)*Scale,Center.y+(y-12)*Scale};}
        void Finish(){if(Active)Draw->PathStroke(Color,0,Width);Active=false;}
        void Move(float x,float y){Finish();Draw->PathLineTo(At(x,y));Active=true;}
        void Line(float x,float y){Draw->PathLineTo(At(x,y));}
        void Quad(float x,float y,float a,float b){Draw->PathBezierQuadraticCurveTo(At(x,y),At(a,b));}
        void Cubic(float x,float y,float a,float b,float c,float e){Draw->PathBezierCubicCurveTo(At(x,y),At(a,b),At(c,e));}
        void Close(){Draw->PathStroke(Color,ImDrawFlags_Closed,Width);Active=false;}
    } sink{d,center,size/28.0f,w,color};
    HelpyGlyphs::Replay(HelpyGlyphs::Find(group),sink);
}
inline void Inline(std::string_view group){const float side=HelpyImGuiTheme::Current.Y(20);const auto at=ImGui::GetCursorScreenPos();Draw(group,{at.x+side*.5f,at.y+side*.5f},side);ImGui::Dummy({side,side});ImGui::SameLine();}
inline void Resource(std::string_view group,const std::string& name,ImVec2 center,float size,bool mixed=false){
    if(!mixed&&HelpyWikiIcons::Draw(name,{center.x-size*.5f,center.y-size*.5f},{center.x+size*.5f,center.y+size*.5f}))return;Draw(group,center,size,mixed);
}
}
