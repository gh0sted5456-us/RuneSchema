#include "Generator/HelpySurfaceDraw.h"
#include <cassert>
int main(){
    ImGui::CreateContext();
    {
        ImDrawList first(ImGui::GetDrawListSharedData()),second(ImGui::GetDrawListSharedData()),callback(ImGui::GetDrawListSharedData());
        ImDrawCmd left;left.ClipRect={-100,0,0,100};first.CmdBuffer.push_back(left);first.VtxBuffer.resize(3);first.IdxBuffer.resize(3);
        ImDrawCmd right;right.ClipRect={0,0,100,100};second.CmdBuffer.push_back(right);second.VtxBuffer.resize(4);second.IdxBuffer.resize(6);
        ImDrawCmd custom;custom.UserCallback=ImDrawCallback_ResetRenderState;callback.CmdBuffer.push_back(custom);
        ImDrawData original;original.Valid=true;original.CmdLists.push_back(&first);original.CmdLists.push_back(&second);original.CmdLists.push_back(&callback);original.CmdListsCount=3;original.TotalVtxCount=7;original.TotalIdxCount=9;
        auto surface=original;surface.DisplayPos={0,0};surface.DisplaySize={100,100};PS::HelpySurfaceDraw::Filter(surface);
        assert(surface.CmdListsCount==2&&surface.CmdLists[0]==&second&&surface.CmdLists[1]==&callback);
        assert(surface.TotalVtxCount==4&&surface.TotalIdxCount==6&&original.CmdListsCount==3&&original.CmdLists.Size==3);
        surface=original;surface.DisplayPos={-100,0};surface.DisplaySize={100,100};PS::HelpySurfaceDraw::Filter(surface);
        assert(surface.CmdListsCount==2&&surface.CmdLists[0]==&first&&surface.TotalVtxCount==3&&surface.TotalIdxCount==3);
        surface=original;surface.DisplayPos={100,200};surface.DisplaySize={100,100};PS::HelpySurfaceDraw::Filter(surface);
        assert(surface.CmdListsCount==1&&surface.CmdLists[0]==&callback&&surface.TotalVtxCount==0);
    }
    ImGui::DestroyContext();
}
