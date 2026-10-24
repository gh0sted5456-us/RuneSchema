#pragma once
#include "imgui.h"
namespace PS::HelpySurfaceDraw {
// Cull whole draw lists only; preserve command offsets and shared texture updates.
inline void Filter(ImDrawData& draw){
    const auto x=draw.DisplayPos.x,y=draw.DisplayPos.y;
    const auto right=x+draw.DisplaySize.x,bottom=y+draw.DisplaySize.y;
    int kept=0;draw.TotalIdxCount=draw.TotalVtxCount=0;
    for(auto* list:draw.CmdLists){
        bool visible=false;
        for(const auto& cmd:list->CmdBuffer)if(cmd.UserCallback||(cmd.ClipRect.x<right&&cmd.ClipRect.z>x&&cmd.ClipRect.y<bottom&&cmd.ClipRect.w>y)){visible=true;break;}
        if(!visible)continue;
        draw.CmdLists[kept++]=list;draw.TotalIdxCount+=list->IdxBuffer.Size;draw.TotalVtxCount+=list->VtxBuffer.Size;
    }
    draw.CmdLists.resize(kept);draw.CmdListsCount=kept;
}
}
