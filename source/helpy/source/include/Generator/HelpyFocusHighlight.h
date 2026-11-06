#pragma once
#include <imgui.h>
#include <algorithm>

namespace PS::HelpyFocusHighlight {
inline void Draw(ImDrawList* draw,ImVec2 a,ImVec2 b,ImU32 color,float rounding,bool fill=true,float thickness=1.f){
    const float shortest=std::min(b.x-a.x,b.y-a.y);if(shortest<=0||(color&IM_COL32_A_MASK)==0)return;
    // Inset strokes contain antialiasing; clip fills without splitting every item-card draw batch.
    if(fill)draw->PushClipRect(a,b,true);
    if(fill){const auto alpha=static_cast<ImU32>(((color>>IM_COL32_A_SHIFT)&255)*.16f);draw->AddRectFilled(a,b,(color&~IM_COL32_A_MASK)|(alpha<<IM_COL32_A_SHIFT),rounding);}
    thickness=std::min(std::max(.5f,thickness),shortest*.5f);const float inset=std::min(shortest*.5f,thickness*.5f+1.f);
    if(shortest>inset*2)draw->AddRect({a.x+inset,a.y+inset},{b.x-inset,b.y-inset},color,std::max(0.f,rounding-inset),0,thickness);
    if(fill)draw->PopClipRect();
}
}
