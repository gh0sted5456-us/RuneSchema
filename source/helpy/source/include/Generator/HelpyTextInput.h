#pragma once
#include <imgui_internal.h>
#include "Core/HelpyPreviewHooks.h"
#include <algorithm>
#include <string>
#include <cstring>
#include <cctype>
#include <windows.h>

namespace PS::HelpyTextInput {
inline ImGuiID Owner=0,ClosedFor=0;
inline std::string Draft;
inline bool Shift=false,Symbols=false;
inline void Wipe(){Draft.resize(Draft.capacity(),'\0');if(!Draft.empty())SecureZeroMemory(Draft.data(),Draft.size());Draft.clear();}
inline void Clear(){Wipe();Owner=0;}
inline bool Allowed(unsigned char c,ImGuiInputTextFlags flags){
    if(flags&ImGuiInputTextFlags_CharsDecimal)return std::strchr("0123456789.+-*/",c)!=nullptr;
    if(flags&ImGuiInputTextFlags_CharsHexadecimal)return std::isxdigit(c)!=0;
    if(flags&ImGuiInputTextFlags_CharsScientific)return std::strchr("0123456789.+-*/eE",c)!=nullptr;
    return !(flags&ImGuiInputTextFlags_CharsNoBlank)||!std::isspace(c);
}
inline bool Keyboard(char* buffer,size_t capacity,ImGuiInputTextFlags flags,ImGuiInputTextCallback callback,void* user){
    auto& g=*ImGui::GetCurrentContext();const auto field=g.LastItemData;const auto id=field.ID;
    const bool active=ImGui::IsItemActive(),focused=ImGui::IsItemFocused();
    if(ClosedFor==id&&!active)ClosedFor=0;
    const bool request=!(flags&ImGuiInputTextFlags_ReadOnly)&&capacity>0&&capacity<=65536&&
        ((active&&g.NavInputSource==ImGuiInputSource_Gamepad&&ClosedFor!=id)||(focused&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp,false)));
    ImGui::PushID(id);
    if(request&&Owner!=id){Clear();Owner=id;Draft.assign(buffer,std::min(std::strlen(buffer),capacity-1));Shift=Symbols=false;ImGui::OpenPopup("Controller keyboard");}
    bool committed=false,restore=false;
    const auto* viewport=ImGui::GetMainViewport();const float width=std::max(120.f,std::min(620.f,viewport->WorkSize.x-24));
    ImGui::SetNextWindowSize({width,0},ImGuiCond_Appearing);ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*.5f,viewport->WorkPos.y+viewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    if(ImGui::BeginPopupModal("Controller keyboard",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)){
        if(Owner!=id||capacity==0||Draft.size()>=capacity||(flags&ImGuiInputTextFlags_ReadOnly)){Clear();ImGui::CloseCurrentPopup();}
        else{
            const auto preview=flags&ImGuiInputTextFlags_Password?std::string(Draft.size(),'*'):Draft;
            ImGui::TextWrapped("%s",preview.empty()?"Enter text":preview.c_str());ImGui::Separator();
            const float keyWidth=std::max(1.f,(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*9)/10);
            const auto key=[&](const char* text,float size=0.f){const bool pressed=ImGui::Button(text,{size>0?size:keyWidth,ImGui::GetFrameHeight()*1.3f});
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::ControllerKeys[text]=HelpyPreviewHooks::LastRect();
#endif
                return pressed;};
            const char* letters[]{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm"};
            const char* symbols[]{"!@#$%^&*()","[]{}<>/\\|","_-+=:;\"'?",".,`~"};
            for(size_t row=0;row<4;++row){const auto text=Symbols?symbols[row]:letters[row];for(size_t i=0;text[i];++i){if(i)ImGui::SameLine();char c=Shift?char(std::toupper(static_cast<unsigned char>(text[i]))):text[i];const char label[]{c,0};
                    if(key(label)&&Draft.size()+1<capacity&&Allowed(static_cast<unsigned char>(c),flags)){
                        ImGuiInputTextCallbackData data;data.EventFlag=ImGuiInputTextFlags_CallbackCharFilter;data.EventChar=flags&ImGuiInputTextFlags_CharsUppercase?std::toupper(static_cast<unsigned char>(c)):c;data.UserData=user;
                        if((!(flags&ImGuiInputTextFlags_CallbackCharFilter)||!callback||callback(&data)==0)&&data.EventChar){char encoded[5]{};ImTextCharToUtf8(encoded,data.EventChar);const auto bytes=std::strlen(encoded);if(Draft.size()+bytes<capacity)Draft.append(encoded,bytes);}
                    }} }
            if(key(Shift?"Lower":"Shift",keyWidth*2))Shift=!Shift;ImGui::SameLine();if(key(Symbols?"ABC":"Symbols",keyWidth*2))Symbols=!Symbols;
            ImGui::SameLine();if(key("Space",keyWidth*2)&&Draft.size()+1<capacity&&Allowed(' ',flags))Draft+=' ';
            ImGui::SameLine();if(key("Backspace",keyWidth*3)&&!Draft.empty()){size_t at=Draft.size()-1;while(at>0&&(static_cast<unsigned char>(Draft[at])&0xc0)==0x80)--at;Draft.resize(at);}
            if(key("Clear",keyWidth*2))Wipe();ImGui::SameLine();
            if(key("Done",keyWidth*3)){std::memcpy(buffer,Draft.data(),Draft.size());buffer[Draft.size()]=0;if(auto* state=ImGui::GetInputTextState(id))state->ReloadUserBufAndMoveToEnd();committed=true;restore=true;}
            ImGui::SameLine();if(key("Cancel",keyWidth*3)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))restore=true;
            if(restore){ClosedFor=id;Clear();ImGui::CloseCurrentPopup();}
        }ImGui::EndPopup();
    }
    ImGui::PopID();g.LastItemData=field;if(committed)g.LastItemData.StatusFlags|=ImGuiItemStatusFlags_Edited;if(restore)ImGui::SetKeyboardFocusHere(-1);return committed;
}
inline bool InputText(const char* label,char* buffer,size_t capacity,ImGuiInputTextFlags flags=0,ImGuiInputTextCallback callback=nullptr,void* user=nullptr){
    const bool changed=ImGui::InputText(label,buffer,capacity,flags,callback,user);return Keyboard(buffer,capacity,flags,callback,user)||changed;
}
inline bool InputTextWithHint(const char* label,const char* hint,char* buffer,size_t capacity,ImGuiInputTextFlags flags=0,ImGuiInputTextCallback callback=nullptr,void* user=nullptr){
    const bool changed=ImGui::InputTextWithHint(label,hint,buffer,capacity,flags,callback,user);return Keyboard(buffer,capacity,flags,callback,user)||changed;
}
inline bool InputTextMultiline(const char* label,char* buffer,size_t capacity,const ImVec2& size=ImVec2(0,0),ImGuiInputTextFlags flags=0,ImGuiInputTextCallback callback=nullptr,void* user=nullptr){
    const bool changed=ImGui::InputTextMultiline(label,buffer,capacity,size,flags,callback,user);return Keyboard(buffer,capacity,flags,callback,user)||changed;
}
}
