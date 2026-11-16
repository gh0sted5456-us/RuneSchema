#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "ModMenuColor.h"
#include <numbers>

namespace PS::ModMenuControls {
inline bool Dial(const char* id,double& value,double minimum,double maximum,double step,int decimals){
    const auto& theme=HelpyImGuiTheme::Current;const float side=std::max(1.f,std::min(theme.Y(108),ImGui::GetContentRegionAvail().x));
    const auto item=ImGui::GetID(id);const bool typing=ImGui::TempInputIsActive(item);auto clear=ImVec4(0,0,0,0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,clear);ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,clear);ImGui::PushStyleColor(ImGuiCol_FrameBgActive,clear);ImGui::PushStyleColor(ImGuiCol_Border,clear);ImGui::PushStyleColor(ImGuiCol_NavCursor,clear);ImGui::PushStyleColor(ImGuiCol_Text,typing?theme.text:clear);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,std::max(0.f,(side-ImGui::GetFontSize())*.5f)});ImGui::SetNextItemWidth(side);
    char format[16]{};snprintf(format,sizeof(format),"%%.%df",std::clamp(decimals,0,6));
    const bool changed=ImGui::DragScalar(id,ImGuiDataType_Double,&value,float(std::max(step,(maximum-minimum)/200.)),&minimum,&maximum,format,ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleVar();ImGui::PopStyleColor(6);
    if(!ImGui::TempInputIsActive(item)){
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const ImVec2 center{(a.x+b.x)*.5f,(a.y+b.y)*.5f};const float radius=side*.37f,start=std::numbers::pi_v<float>*.75f,sweep=std::numbers::pi_v<float>*1.5f;
        const auto fraction=std::isfinite(value)&&maximum>minimum?std::clamp((value-minimum)/(maximum-minimum),0.,1.):0.;auto* draw=ImGui::GetWindowDrawList();
        const int segments=std::clamp(int(std::ceil(sweep*radius/3.f)),16,256),circleSegments=std::clamp(int(std::ceil(2*std::numbers::pi_v<float>*(radius+theme.Y(8))/3.f)),32,256);
        draw->AddCircleFilled(center,std::max(0.f,radius-theme.Y(6)),ImGui::GetColorU32(theme.control),circleSegments);
        draw->PathArcTo(center,radius,start,start+sweep,segments);draw->PathStroke(ImGui::GetColorU32(theme.border),0,theme.Y(6));
        if(fraction>0){draw->PathArcTo(center,radius,start,start+float(fraction)*sweep,std::max(1,int(std::ceil(segments*fraction))));draw->PathStroke(ImGui::GetColorU32(theme.accent),0,theme.Y(6));}
        const float angle=start+float(fraction)*sweep;const ImVec2 thumb{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};
        draw->AddCircleFilled(thumb,theme.Y(5),ImGui::GetColorU32(theme.accentHover));draw->AddCircleFilled(thumb,theme.Y(2),ImGui::GetColorU32(theme.control));
        if(ImGui::IsItemFocused()||ImGui::IsItemActive())draw->AddCircle(center,radius+theme.Y(8),ImGui::GetColorU32(theme.accentHover),circleSegments,theme.Y(2));
        char text[64]{};snprintf(text,sizeof(text),format,value);const auto size=ImGui::CalcTextSize(text);
        ImGui::RenderTextClipped({a.x+2,center.y-size.y*.5f},{b.x-2,center.y+size.y*.5f},text,nullptr,nullptr,{.5f,.5f});
    }
    return changed;
}
inline bool Radial(const char* id,const nlohmann::json& options,const nlohmann::json& current,nlohmann::json& next){
    if(!options.is_array()||options.empty()||options.size()>12)return false;
    for(const auto& option:options)if(!option.is_string())return false;
    const auto& theme=HelpyImGuiTheme::Current;const float diameter=std::max(1.f,std::min(theme.Y(280),ImGui::GetContentRegionAvail().x));
    if(diameter<theme.Y(120)){
        const auto text=current.is_string()?current.get<std::string>():std::string{};bool changed=false;ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo(id,text.c_str())){for(const auto& option:options){const auto& label=option.get_ref<const std::string&>();if(ImGui::Selectable(label.c_str(),option==current)){next=option;changed=true;}}ImGui::EndCombo();}return changed;
    }
    const auto start=ImGui::GetCursorScreenPos();const ImVec2 center{start.x+diameter*.5f,start.y+diameter*.5f};const float outer=diameter*.46f,inner=diameter*.25f,radius=(outer+inner)*.5f;
    auto* draw=ImGui::GetWindowDrawList();bool changed=false;ImGui::PushID(id);ImGui::BeginGroup();
    for(size_t i=0;i<options.size();++i){
        ImGui::PushID(int(i));const float angle=-std::numbers::pi_v<float>*.5f+2*std::numbers::pi_v<float>*float(i)/float(options.size());const float half=std::numbers::pi_v<float>/float(options.size())*.92f;
        const float side=std::min(outer-inner-theme.Y(4),2*radius*std::sin(std::numbers::pi_v<float>/float(std::max(size_t(2),options.size())))*.8f);
        const ImVec2 point{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};ImGui::SetCursorScreenPos({point.x-side*.5f,point.y-side*.5f});
        if(ImGui::InvisibleButton("option",{side,side},ImGuiButtonFlags_EnableNav)){next=options[i];changed=true;}
        const bool highlighted=ImGui::IsItemFocused()||ImGui::IsItemHovered(),selected=current==options[i];
        const int segments=std::clamp(int(std::ceil(2*half*outer/3.f)),12,128);
        auto fill=selected?theme.selection:theme.control;if(highlighted)fill=theme.selection;
        draw->PathArcTo(center,outer,angle-half,angle+half,segments);draw->PathArcTo(center,inner,angle+half,angle-half,segments);draw->PathFillConcave(ImGui::GetColorU32(fill));
        const auto border=ImGui::GetColorU32(highlighted?theme.accentHover:selected?theme.accent:theme.border);
        draw->PathArcTo(center,outer,angle-half,angle+half,segments);draw->PathStroke(border,0,theme.Y(selected||highlighted?2.f:1.f));
        draw->PathArcTo(center,inner,angle-half,angle+half,segments);draw->PathStroke(border,0,theme.Y(1));
        for(float edge:{angle-half,angle+half})draw->AddLine({center.x+std::cos(edge)*inner,center.y+std::sin(edge)*inner},{center.x+std::cos(edge)*outer,center.y+std::sin(edge)*outer},border,theme.Y(1));
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const auto& label=options[i].get_ref<const std::string&>();
        ImGui::PushStyleColor(ImGuiCol_Text,selected||highlighted?theme.accentHover:theme.text);ImGui::RenderTextClipped({a.x+2,a.y+2},{b.x-2,b.y-2},label.c_str(),nullptr,nullptr,{.5f,.5f});ImGui::PopStyleColor();
        if(ImGui::IsItemHovered()||ImGui::IsItemFocused())HelpyImGuiTheme::Tooltip("%s",label.c_str());ImGui::PopID();
    }
    draw->AddCircleFilled(center,inner-theme.Y(5),ImGui::GetColorU32(theme.control),std::clamp(int(std::ceil(2*std::numbers::pi_v<float>*inner/3.f)),32,256));
    const auto text=current.is_string()?current.get<std::string>():std::string{};const float textWidth=inner*1.55f;const auto size=ImGui::CalcTextSize(text.c_str(),nullptr,false,textWidth);
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{center.x-textWidth*.5f,center.y-size.y*.5f},ImGui::GetColorU32(theme.text),text.c_str(),nullptr,textWidth);
    ImGui::SetCursorScreenPos(start);ImGui::Dummy({diameter,diameter});ImGui::EndGroup();ImGui::PopID();return changed;
}
inline bool Color(const char* id,std::string& value,bool alpha){
    auto color=ModMenuColor::Decode(value);if(!color)return false;
    const auto flags=ImGuiColorEditFlags_DisplayRGB|ImGuiColorEditFlags_InputRGB|ImGuiColorEditFlags_Float|ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_PickerHueWheel|(alpha?ImGuiColorEditFlags_AlphaBar:ImGuiColorEditFlags_NoAlpha);
    if(!ImGui::ColorEdit4(id,color->data(),flags))return false;
    const auto encoded=ModMenuColor::Encode(*color,alpha);if(encoded.empty())return false;value=encoded;return true;
}
}
