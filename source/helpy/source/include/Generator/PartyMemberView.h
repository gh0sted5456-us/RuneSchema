#pragma once
#include "Core/ItemPresentation.h"
#include "Core/PartyStats.h"
#include "Core/PartyCardLayout.h"
#include "Generator/ItemIconCache.h"
#include "Generator/QuickMenuDecorations.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/PlayerBadges.h"
#include "Generator/PlayerPortraitPicker.h"
#include "Generator/SourceBanners.h"
#include <chrono>
#include <cstdio>
#include <cmath>
namespace PS::PartyMemberView {
inline void BackgroundMenu(const nlohmann::json& row){
    const auto guid=ItemPresentation::Text(row,"Guid");if(!ImGui::BeginMenu("Card background",!PlayerPortraits::Identity(guid).empty()))return;
    auto& choices=PlayerPortraitPicker::Choices;const auto current=choices.GetBackground(guid);const auto select=[&](const char* label,const std::string& key){if(ImGui::MenuItem(label,nullptr,current==key))if(choices.SetBackground(guid,key)){PlayerPortraitPicker::SaveWanted=true;SourceBanners::Next={};}};
    select("Default / clear","");select("RuneSchema artwork","ui:home-banner.png");
    if(ImGui::BeginMenu("Source banners")){for(const auto& [owner,image]:SourceBanners::Images)if(ContentSource::Valid(owner))select(owner.c_str(),"source:"+owner);ImGui::EndMenu();}
    if(ImGui::BeginMenu("RuneSchema backgrounds")){if(!HelpyBackgrounds::Listed)HelpyBackgrounds::Refresh();for(const auto& file:HelpyBackgrounds::Files)select(file.c_str(),"background:"+file);if(HelpyBackgrounds::Files.empty())ImGui::TextDisabled("No local backgrounds");ImGui::EndMenu();}
    ImGui::TextDisabled("Saved per character on this device");ImGui::EndMenu();
}
inline int64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline bool EquipmentFresh(const nlohmann::json& row,int64_t now){return row.contains("Equipment")&&PartyStats::Fresh(row["Equipment"],now)&&row["Equipment"].contains("Items")&&row["Equipment"]["Items"].is_array()&&row["Equipment"]["Items"].size()<=32;}
inline void DurabilityMeter(const nlohmann::json& item,ImVec2 a,ImVec2 b,int64_t now){
    const auto& t=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    const auto value=PartyStats::Read(item,"Durability",now);const float top=b.y-t.Y(4),width=std::max(0.f,b.x-a.x-t.X(4));
    if(!value){draw->AddText(ImGui::GetFont(),t.Y(9),{a.x+t.X(2),b.y-t.Y(10)},ImGui::GetColorU32(t.textDisabled),"?");return;}
    draw->AddRectFilled({a.x+t.X(2),top},{b.x-t.X(2),b.y-t.Y(1)},ImGui::GetColorU32(t.border),t.Y(1));
    if(value){const auto fraction=float(value->Fraction());draw->AddRectFilled({a.x+t.X(2),top},{a.x+t.X(2)+width*fraction,b.y-t.Y(1)},ImGui::GetColorU32(fraction<=.2f?t.error:fraction<=.5f?t.warning:t.success),t.Y(1));}
}
inline void EquipmentDetails(const nlohmann::json& item,int64_t now){
    HelpyImGuiTheme::BeginDetailsTooltip();SourceBanners::DetailsBackground(item);ImGui::TextUnformatted(ItemPresentation::Text(item,"Name").c_str());
    ImGui::TextDisabled("Source: %s",ItemSource::DisplayName(ItemSource::Row(item)).c_str());
    ImGui::TextDisabled("Native equipped slot %d",item.value("Slot",0));
    if(const auto value=PartyStats::Read(item,"Durability",now)){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyDurabilityDetails=true;
#endif
        ImGui::SeparatorText("Durability");ImGui::Text("%.0f / %.0f (%.0f%%)",value->Current,value->Maximum,value->Fraction()*100);
        ImGui::ProgressBar(float(value->Fraction()),{HelpyImGuiTheme::Current.X(180),HelpyImGuiTheme::Current.Y(7)},"");
    }else ImGui::TextWrapped("Durability unavailable. %s",item.value("DurabilityReason",std::string("No fresh native reading")).c_str());
    HelpyImGuiTheme::EndDetailsTooltip();
}
inline void Equipment(const nlohmann::json& row,int64_t now){
    const auto& t=HelpyImGuiTheme::Current;
    if(!EquipmentFresh(row,now)){ImGui::TextDisabled("Equipment unavailable");return;}
    const auto& items=row["Equipment"]["Items"];if(items.empty()){ImGui::TextDisabled("No equipped items");return;}
    const float left=ImGui::GetCursorPosX(),width=ImGui::GetContentRegionAvail().x,side=std::min(t.Y(26),width),gap=t.X(6);
    for(const auto& line:PartyCardLayout::Centered(items.size(),width,side,gap))for(size_t col=0;col<line.Count;++col){const size_t i=line.First+col;if(col)ImGui::SameLine(0,gap);else ImGui::SetCursorPosX(left+line.Offset);
        ImGui::PushID(int(i));const auto a=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##gear",{side,side});
        ImGui::GetWindowDrawList()->AddRect(a,{a.x+side,a.y+side},ImGui::GetColorU32(t.border),t.X(3));
        if(!ItemIconCache::Draw(ItemPresentation::Text(items[i],"Icon"),a,{a.x+side,a.y+side},true)){const auto label=std::to_string(items[i].value("Slot",0));ImGui::GetWindowDrawList()->AddText({a.x+t.X(4),a.y+t.Y(4)},ImGui::GetColorU32(t.textDisabled),label.c_str());}
        DurabilityMeter(items[i],a,{a.x+side,a.y+side},now);
        if(ImGui::IsItemHovered())EquipmentDetails(items[i],now);ImGui::PopID();}
}
inline bool SkillsFresh(const nlohmann::json& row,int64_t now){return row.contains("Equipment")&&row["Equipment"].contains("Skills")&&PartyStats::Fresh(row["Equipment"]["Skills"],now)&&row["Equipment"]["Skills"].contains("Items")&&row["Equipment"]["Skills"]["Items"].is_array()&&row["Equipment"]["Skills"]["Items"].size()<=32;}
inline PartyCardLayout::Columns Columns(float width){
    const auto scale=std::max(.01f,HelpyImGuiTheme::Current.X(1));auto columns=PartyCardLayout::Split(width/scale);
    columns.Stats*=scale;columns.Equipment*=scale;columns.Gap*=scale;return columns;
}
inline float StatsHeight(float width){const auto& t=HelpyImGuiTheme::Current;return t.Y(PlayerPortraitPicker::Choices.Meters?(width>=t.X(195)?68:107):32);}
inline float GearHeight(const nlohmann::json& row,float width){
    const auto& t=HelpyImGuiTheme::Current;if(!EquipmentFresh(row,Now()))return t.Y(34);
    const auto count=row["Equipment"]["Items"].size();const auto lines=PartyCardLayout::Centered(count,width,std::min(t.Y(36),width),t.X(6));
    return t.Y(18)+std::max(t.Y(18),float(lines.size())*t.Y(50));
}
inline float BodyHeight(const nlohmann::json& row,float width,bool gear){
    if(!gear)return StatsHeight(width);const auto columns=Columns(width);
    const auto stats=StatsHeight(columns.Stats),equipment=GearHeight(row,columns.Equipment);
    return columns.Beside?std::max(stats,equipment):stats+HelpyImGuiTheme::Current.Y(8)+equipment;
}
// Draw only: the member remains one selectable row, including its context menu.
inline bool GearRow(const nlohmann::json& row,ImVec2 start,float width,int64_t now){
    const auto& t=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    draw->AddText(start,ImGui::GetColorU32(t.textDisabled),"Equipped");
    if(!EquipmentFresh(row,now)){draw->AddText({start.x,start.y+t.Y(18)},ImGui::GetColorU32(t.textDisabled),"Equipment unavailable");return false;}
    const auto& items=row["Equipment"]["Items"];if(items.empty()){draw->AddText({start.x,start.y+t.Y(18)},ImGui::GetColorU32(t.textDisabled),"No equipped items");return false;}
    const float side=std::min(t.Y(36),width);size_t lineIndex=0;bool hovered=false;
    for(const auto& line:PartyCardLayout::Centered(items.size(),width,side,t.X(6))){
        for(size_t col=0;col<line.Count;++col){const auto& item=items[line.First+col];
            const ImVec2 a{start.x+line.Offset+float(col)*(side+t.X(6)),start.y+t.Y(18)+float(lineIndex)*t.Y(50)},b{a.x+side,a.y+side+t.Y(5)};
            draw->AddRectFilled(a,b,ImGui::GetColorU32(t.panelAlt),t.X(3));draw->AddRect(a,b,ImGui::GetColorU32(t.border),t.X(3));
            if(!ItemIconCache::Draw(ItemPresentation::Text(item,"Icon"),a,{b.x,a.y+side},true))HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Item,{a.x+side*.5f,a.y+side*.5f},side*.6f,t.icon);
            DurabilityMeter(item,a,b,now);
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::PartyGear[ItemPresentation::Text(row,"Path")+":"+std::to_string(item.value("Slot",0))]={a.x,a.y,b.x,b.y};
            if(PartyStats::Read(item,"Durability",now))++HelpyPreviewHooks::PartyDurabilityMeters;
#endif
            if(ImGui::IsItemHovered()&&ImGui::IsMouseHoveringRect(a,b)){hovered=true;EquipmentDetails(item,now);}
        }++lineIndex;
    }return hovered;
}
inline float CardHeight(const nlohmann::json& row,float width){
    const auto& t=HelpyImGuiTheme::Current;const auto& p=PlayerPortraitPicker::Choices;
    const auto now=Now();
    float height=BodyHeight(row,width,p.Equipment)+t.Y(12);
    const auto equipment=row.value("Equipment",nlohmann::json::object());
    if(p.Skills&&SkillsFresh(row,now)){const auto skills=equipment.value("Skills",nlohmann::json::object()).value("Items",nlohmann::json::array());const int columns=std::max(1,int((width+ImGui::GetStyle().ItemSpacing.x)/(t.Y(24)+ImGui::GetStyle().ItemSpacing.x)));height+=std::max(t.Y(18),float((std::min<size_t>(32,skills.size())+columns-1)/columns)*(t.Y(37)+ImGui::GetStyle().ItemSpacing.y))+ImGui::GetStyle().ItemSpacing.y;}
    return height+t.Y(4);
}
inline void Skills(const nlohmann::json& row,int64_t now){
    if(!SkillsFresh(row,now)){ImGui::TextDisabled("Skill levels unavailable");return;}const auto& t=HelpyImGuiTheme::Current;const auto& items=row["Equipment"]["Skills"]["Items"];const float side=t.Y(24);ImGui::PushID("skills");
    const int columns=std::max(1,int((ImGui::GetContentRegionAvail().x+ImGui::GetStyle().ItemSpacing.x)/(side+ImGui::GetStyle().ItemSpacing.x)));
    for(size_t i=0;i<items.size();++i){if(i%columns)ImGui::SameLine();ImGui::PushID(int(i));const auto a=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##skill",{side,side+t.Y(13)});
        ItemIconCache::Draw(ItemPresentation::Text(items[i],"Icon"),a,{a.x+side,a.y+side},true);const auto level=std::to_string(items[i].value("Level",0));const auto extent=ImGui::CalcTextSize(level.c_str());ImGui::GetWindowDrawList()->AddText({a.x+(side-extent.x)*.5f,a.y+side},ImGui::GetColorU32(t.text),level.c_str());
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s · Level %d",ItemPresentation::Text(items[i],"Name").c_str(),items[i].value("Level",0));ImGui::PopID();}ImGui::PopID();
}
inline void Advanced(){
    if(HelpyImGuiTheme::SoftButton("Advanced"))ImGui::OpenPopup("Party display");if(!ImGui::BeginPopup("Party display"))return;
    auto& p=PlayerPortraitPicker::Choices;bool changed=false;changed|=ImGui::Checkbox("Vital meters",&p.Meters);changed|=ImGui::Checkbox("Equipped gear icons",&p.Equipment);changed|=ImGui::Checkbox("Skill levels",&p.Skills);changed|=ImGui::Checkbox("Native power level",&p.Power);changed|=ImGui::Checkbox("Host / role badge",&p.Roles);
    ImGui::Separator();changed|=ImGui::Checkbox("Repeat vitals in hover details",&p.HoverVitals);changed|=ImGui::Checkbox("Technical details in hover",&p.Technical);
    if(changed)PlayerPortraitPicker::SaveWanted=true;ImGui::TextWrapped("Double-click a portrait or use Change portrait in the member's right-click menu.");ImGui::EndPopup();
}
inline void Details(const nlohmann::json& row,int64_t now){
    HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextUnformatted(ItemPresentation::Text(row,"Name").c_str());ImGui::Separator();
    if(PlayerPortraitPicker::Choices.HoverVitals)for(const auto& entry:std::array<std::pair<const char*,const char*>,8>{{{"Health","Health"},{"Stamina","Stamina"},{"SpecialPower","Special power"},{"Hunger","Food / sustenance"},{"Thirst","Water / hydration"},{"RestLevel","Rest / endurance"},{"Food","Food effects"},{"Rest","Sleep state"}}}){
        const auto [key,label]=entry;
        if(const auto gauge=PartyStats::Read(row,key,now)){ImGui::Text("%s: %.0f / %.0f",label,gauge->Current,gauge->Maximum);ImGui::ProgressBar(static_cast<float>(gauge->Fraction()),{0,HelpyImGuiTheme::Current.Y(6)},"");}
        else{const auto& stat=PartyStats::Find(row,key);const auto state=ItemPresentation::Text(stat,"State");
            if(!state.empty()&&PartyStats::Fresh(stat,now))ImGui::TextWrapped("%s: %s",label,state.c_str());
            else ImGui::TextWrapped("%s: %s",label,PartyStats::Reason(row,key,now).c_str());}
    }
    if(const auto power=PartyStats::Power(row,now))ImGui::Text("Native power level: %.0f",*power);
    if(PlayerPortraitPicker::Choices.Equipment){ImGui::SeparatorText("Equipped");Equipment(row,now);}
    if(PlayerPortraitPicker::Choices.Skills){ImGui::SeparatorText("Skills");Skills(row,now);}
    if(PlayerPortraitPicker::Choices.Technical){
    ImGui::TextWrapped("Survival meters use native sustenance, hydration and endurance. Food buffs and sleep state are separate.");
    if(row.contains("Stats"))ImGui::TextWrapped("%s",ItemPresentation::Text(row["Stats"],"Source").c_str());
    ImGui::TextDisabled("Missing or stale readings are not zero.");}HelpyImGuiTheme::EndDetailsTooltip();
}
// One player-card design for map party members and item recipients.
inline bool Row(const nlohmann::json& row,bool selected,bool keepPopup=false,bool withEquipment=false){
    const auto& t=HelpyImGuiTheme::Current;const auto start=ImGui::GetCursorScreenPos();const float side=t.Y(28);
    const auto& display=PlayerPortraitPicker::Choices;const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const bool gear=withEquipment&&display.Equipment;const auto columns=Columns(width);const float statsWidth=gear?columns.Stats:width;
    const bool paired=statsWidth>=t.X(195);const float height=BodyHeight(row,width,gear);const auto now=Now();
    const bool clicked=ImGui::Selectable("##member",selected,keepPopup?ImGuiSelectableFlags_NoAutoClosePopups:ImGuiSelectableFlags_None,{0,height});
    const auto icon=PlayerPortraitPicker::Icon(row);bool ready=false;
    const float portraitHeight=StatsHeight(statsWidth);
    const ImVec2 portraitMin{start.x,start.y+(portraitHeight-side)*.5f},portraitMax{start.x+side,start.y+(portraitHeight+side)*.5f};
    if(!icon.empty()&&icon.front()=='/')ready=PlayerPortraitPicker::DrawCached(icon,portraitMin,portraitMax);
    if(ImGui::IsItemHovered()&&ImGui::IsMouseHoveringRect(portraitMin,portraitMax)&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))PlayerPortraitPicker::Request(row);
    auto* draw=ImGui::GetWindowDrawList();
    if(!ready){const ImVec2 center{start.x+side*.5f,start.y+portraitHeight*.5f};const auto color=ImGui::GetColorU32(row.value("IsLocal",false)?t.accentHover:t.text);
        draw->AddCircle(center,side*.46f,color,20,1.2f);draw->AddCircleFilled({center.x,center.y-side*.12f},side*.13f,color);
        draw->AddBezierCubic({center.x-side*.27f,center.y+side*.25f},{center.x-side*.25f,center.y-side*.02f},{center.x+side*.25f,center.y-side*.02f},{center.x+side*.27f,center.y+side*.25f},color,1.6f);}
    const auto power=display.Power?PartyStats::Power(row,now):std::optional<double>{};const float textX=start.x+side+t.X(8),right=start.x+statsWidth-t.X(4);
    const auto* role=PlayerBadges::Find(row);const bool haveRole=display.Roles&&role&&role->value("Role",std::string("unknown"))!="unknown";
    const float nameRight=std::max(textX,right-(power?side+t.X(4):0)-(haveRole?t.X(19):0));
    const ImVec4 clip{textX,start.y,nameRight,start.y+height};
    auto name=ItemPresentation::Text(row,"Name");if(name.empty())name="Unnamed player";if(row.value("IsLocal",false))name+=" (you)";
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{textX,start.y+t.Y(3)},ImGui::GetColorU32(t.text),name.c_str(),nullptr,0,&clip);
    const ImVec2 badge{std::min(nameRight,textX+ImGui::CalcTextSize(name.c_str()).x)+t.X(10),start.y+t.Y(10)};
    if(haveRole)PlayerBadges::Draw(draw,badge,t.Y(14),role);
    if(power&&width>side*3){
        const ImVec2 a{right-side,start.y},b{right,start.y+side};char value[32];std::snprintf(value,sizeof(value),"%.0f",*power);
        const bool native=ItemIconCache::Draw(QuickDecorations::PowerLevelBadge,a,b,true);if(!native)std::snprintf(value,sizeof(value),"PL %.0f",*power);
        const float font=t.Y(native?12:10);const auto extent=ImGui::GetFont()->CalcTextSizeA(font,1000,0,value);const ImVec2 at{a.x+(side-extent.x)*.5f,a.y+(side-extent.y)*.5f};
        draw->AddText(ImGui::GetFont(),font,{at.x+1,at.y+1},IM_COL32(0,0,0,240),value);draw->AddText(ImGui::GetFont(),font,at,ImGui::GetColorU32(t.text),value);
    }
    const char* keys[]{"Health","Stamina","SpecialPower","Hunger","Thirst","RestLevel"};const char* glyphs[]{"health","stamina","special","hunger","thirst","rest"};
    const ImVec4 colors[]{ImVec4(.82f,.27f,.27f,1),ImVec4(.29f,.74f,.39f,1),ImVec4(.40f,.62f,.96f,1),ImVec4(.90f,.65f,.29f,1),ImVec4(.24f,.72f,.84f,1),ImVec4(.74f,.65f,.91f,1)};
    const float columnWidth=std::max(0.0f,(right-textX-(paired?t.X(8):0))/(paired?2:1));
    if(display.Meters)for(int i=0;i<6;++i){const float y=start.y+t.Y(26+(paired?i%3:i)*13);const auto gauge=PartyStats::Read(row,keys[i],now);
        const float columnX=textX+(paired?i/3:0)*(columnWidth+t.X(8)),barX=columnX+t.X(23),barRight=columnX+columnWidth,barWidth=std::max(0.0f,barRight-barX);
        MapLayerIcons::Draw(glyphs[i],{columnX+t.X(7),y+t.Y(6)},t.Y(13));
        if(barWidth>1){draw->AddRectFilled({barX,y+t.Y(2)},{barRight,y+t.Y(10)},ImGui::GetColorU32(t.border),t.Y(2));
            if(gauge)draw->AddRectFilled({barX,y+t.Y(2)},{barX+barWidth*static_cast<float>(gauge->Fraction()),y+t.Y(10)},ImGui::GetColorU32(colors[i]),t.Y(2));
            char value[64];if(gauge)std::snprintf(value,sizeof(value),"%.0f / %.0f",gauge->Current,gauge->Maximum);else std::snprintf(value,sizeof(value),"unavailable");
            const ImVec4 barClip{barX,y,barRight,y+t.Y(12)};draw->AddText(ImGui::GetFont(),t.Y(10),{barX+t.X(4),y},ImGui::GetColorU32(t.text),value,nullptr,0,&barClip);
        }
    }
    const bool gearHovered=gear&&GearRow(row,columns.Beside?ImVec2{start.x+columns.Stats+columns.Gap,start.y}:ImVec2{start.x,start.y+StatsHeight(statsWidth)+t.Y(8)},columns.Equipment,now);
    if(!gearHovered&&haveRole&&ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect({badge.x-t.X(8),badge.y-t.Y(8)},{badge.x+t.X(8),badge.y+t.Y(8)}))ImGui::SetTooltip("%s",PlayerBadges::Tooltip(*role).c_str());
    else if(!gearHovered&&ImGui::IsItemHovered()&&!ImGui::IsMouseDown(ImGuiMouseButton_Right))Details(row,now);
    return clicked;
}
}
