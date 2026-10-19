#pragma once
#include "Core/CharacterPreviewFrame.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/ItemGridPicker.h"
#include <imgui_internal.h>
namespace PS::CharacterPreviewPanel {
inline CharacterPreviewFrame::Channel Channel;
inline ImTextureData* Texture=nullptr;
inline uint64_t Serial=0;
inline int Yaw=0,Zoom=100,Pitch=0;
inline int Animation=0;inline bool Playing=false;
inline std::string AnimationName;
inline nlohmann::json DroppedItem=nlohmann::json::object();
inline std::string Status="Open a character copy to preview native appearance.";
inline void Close(){if(Channel.Lock()){Channel.Get()->Stop=1;Channel.Unlock();}Channel.Close();Serial=0;}
inline void Fail(const std::string& error){const auto ready=Serial;Close();Serial=ready;Status=error;}
inline void Shutdown(){Close();if(Texture)Texture->SetStatus(ImTextureStatus_WantDestroy);}
// Call after the renderer releases GPU textures, while the ImGui context lives.
inline void ReleaseTexture(){if(Texture){ImGui::UnregisterUserTexture(Texture);IM_DELETE(Texture);Texture=nullptr;}}
inline void Attach(const std::string& name){if(Channel.Name()==name)return;Close();Channel.Open(name,false);}
inline void Draw(float height){
    if(!Channel.Name().empty()&&Channel.Lock()){
        auto* frame=Channel.Get();frame->Heartbeat=GetTickCount64();frame->Yaw=Yaw;frame->Zoom=Zoom;frame->Pitch=Pitch;frame->Animation=Animation;frame->Playing=Playing?1:0;
        Status=frame->Status;
        if(frame->Serial&&frame->Serial!=Serial){
            if(!Texture){Texture=IM_NEW(ImTextureData)();Texture->Create(ImTextureFormat_RGBA32,512,512);Texture->UseColors=true;Texture->RefCount=1;Texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(Texture);}
            if(Texture->Status==ImTextureStatus_Destroyed)Texture->SetStatus(ImTextureStatus_WantCreate);
            if(Texture->Status==ImTextureStatus_OK||Texture->Status==ImTextureStatus_WantCreate){std::memcpy(Texture->GetPixels(),frame->Pixels,CharacterPreviewFrame::Bytes);if(Texture->Status==ImTextureStatus_OK){Texture->Updates.clear();Texture->Updates.push_back({0,0,512,512});Texture->UpdateRect={0,0,512,512};Texture->SetStatus(ImTextureStatus_WantUpdates);}else Texture->SetStatus(ImTextureStatus_WantCreate);Serial=frame->Serial;}
        }Channel.Unlock();
    }
    const float width=ImGui::GetContentRegionAvail().x;const float side=std::max(1.f,std::min(width,height));const auto pos=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##native-character-preview",{width,height},ImGuiButtonFlags_MouseButtonLeft);
    const ImVec2 a(pos.x+(width-side)*.5f,pos.y+(height-side)*.5f),b(a.x+side,a.y+side);auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(pos,{pos.x+width,pos.y+height},ImGui::ColorConvertFloat4ToU32(HelpyImGuiTheme::Current.panelAlt),4);
    if(Texture&&Serial)draw->AddImage(Texture->GetTexRef(),a,b);else{const auto text=ImGui::CalcTextSize("Native game preview");draw->AddText({pos.x+(width-text.x)*.5f,pos.y+height*.45f},ImGui::GetColorU32(ImGuiCol_TextDisabled),"Native game preview");}
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0)){Yaw=std::clamp(Yaw+int(ImGui::GetIO().MouseDelta.x),-180,180);}
    if(ImGui::IsItemHovered()){ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);if(ImGui::GetIO().MouseWheel!=0)Zoom=std::clamp(Zoom+int(ImGui::GetIO().MouseWheel*5),70,150);}
    if(ImGui::BeginDragDropTarget()){std::string error;if(auto row=ItemGridPicker::AcceptItemDrop(error))DroppedItem=*row;ImGui::EndDragDropTarget();}
    if(ImGui::Button("Left"))Yaw=std::max(-180,Yaw-15);ImGui::SameLine();if(ImGui::Button("Right"))Yaw=std::min(180,Yaw+15);ImGui::SameLine();if(ImGui::Button("Up"))Pitch=std::min(40,Pitch+10);ImGui::SameLine();if(ImGui::Button("Down"))Pitch=std::max(-40,Pitch-10);
    if(ImGui::Button("Zoom +"))Zoom=std::min(150,Zoom+10);ImGui::SameLine();if(ImGui::Button("Zoom -"))Zoom=std::max(70,Zoom-10);ImGui::SameLine();if(ImGui::Button("Reset view")){Yaw=Pitch=0;Zoom=100;}
    if(!AnimationName.empty()){
        ImGui::SetNextItemWidth(std::max(120.f,width*.7f));if(ImGui::BeginCombo("##native-preview-animation",Animation?AnimationName.c_str():"Standard fitting pose")){
            if(ImGui::Selectable("Standard fitting pose",Animation==0)){Animation=0;Playing=false;}
            if(ImGui::Selectable(AnimationName.c_str(),Animation==1)){Animation=1;Playing=false;}
            ImGui::EndCombo();}
        ImGui::SameLine();ImGui::BeginDisabled(Animation==0);if(ImGui::Button(Playing?"Pause":"Play"))Playing=!Playing;ImGui::EndDisabled();
    }
    ImGui::TextDisabled("Drag to rotate · scroll to zoom · preview only");ImGui::TextWrapped("%s",Status.c_str());
}
}
