#pragma once
#include "Core/ItemPresentation.h"
#include "Core/PartyStats.h"
#include "Core/PartyCardLayout.h"
#include "Core/ItemPreviewRequest.h"
#include <functional>
#include <algorithm>
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
#include <shellapi.h>
namespace PS::PartyMemberView {
inline std::function<void(const nlohmann::json&,const nlohmann::json&)> RestoreGear,ChangeGear;
inline ImTextureRef SteamIcon,EpicIcon,XboxIcon;
inline ImTextureRef PlatformIcon(const std::string& platform){return platform=="Steam"?SteamIcon:platform=="Epic"||platform=="Epic Games"?EpicIcon:platform=="Game Pass"||platform=="Xbox"?XboxIcon:ImTextureRef{};}
inline bool GearContext=false;
inline bool InspectorOpen=false;inline nlohmann::json Inspector;inline uint64_t InspectorGeneration=0;
inline void Inspect(const nlohmann::json& row){Inspector=row;InspectorGeneration=SpawnToolRequests::CurrentGeneration();InspectorOpen=true;}
inline std::optional<ImVec4> NativeColor(const nlohmann::json& row){
    // Only accept a supplied, verified native linear color. Never invent a
    // player-index palette or infer another player's color from array order.
    if(row.value("NativePlayerColorVerified",false)&&row.contains("NativePlayerColor")&&row["NativePlayerColor"].is_array()&&row["NativePlayerColor"].size()==4){
        ImVec4 value;float* out=&value.x;for(size_t i=0;i<4;++i){const auto& channel=row["NativePlayerColor"][i];if(!channel.is_number())return {};const auto number=channel.get<float>();if(!std::isfinite(number)||number<0||number>1)return {};out[i]=i<3?PlayerPortraits::Srgb(number):1;}return value;
    }return {};
}
inline ImVec4 Border(const nlohmann::json& row){return NativeColor(row).value_or(HelpyImGuiTheme::Current.border);}
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
    HelpyImGuiTheme::BeginDetailsTooltip();SourceBanners::DetailsBackground(item);PS::HelpyImGuiTheme::DetailsTextUnformatted(ItemPresentation::Name(item).c_str());
    PS::HelpyImGuiTheme::DetailsTextDisabled("Source: %s",ItemSource::DisplayName(ItemSource::Row(item)).c_str());
    PS::HelpyImGuiTheme::DetailsTextDisabled("Native equipped slot %d",item.value("Slot",0));
    if(const auto value=PartyStats::Read(item,"Durability",now)){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyDurabilityDetails=true;
#endif
        ImGui::SeparatorText("Durability");PS::HelpyImGuiTheme::DetailsText("%.0f / %.0f (%.0f%%)",value->Current,value->Maximum,value->Fraction()*100);
        ImGui::ProgressBar(float(value->Fraction()),{HelpyImGuiTheme::Current.X(180),HelpyImGuiTheme::Current.Y(7)},"");
    }else PS::HelpyImGuiTheme::DetailsText("Durability unavailable. %s",item.value("DurabilityReason",std::string("No fresh native reading")).c_str());
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
inline float StatsHeight(float){const auto& t=HelpyImGuiTheme::Current;const auto& p=PlayerPortraitPicker::Choices;return t.Y(std::max(p.Meters?68:48,p.Power?78:48));}
inline float GearSide(){return HelpyImGuiTheme::Current.Y(PlayerPortraitPicker::Choices.CompactParty?30:36);}
inline float GearLineHeight(){return HelpyImGuiTheme::Current.Y(PlayerPortraitPicker::Choices.CompactParty?36:50);}
inline float GearHeight(const nlohmann::json& row,float width){
    const auto& t=HelpyImGuiTheme::Current;if(!EquipmentFresh(row,Now()))return t.Y(34);
    const auto count=row["Equipment"]["Items"].size();const auto lines=PartyCardLayout::Centered(count,width,std::min(GearSide(),width),t.X(6));
    return t.Y(4)+std::max(t.Y(18),float(lines.size())*GearLineHeight());
}
inline float BodyHeight(const nlohmann::json& row,float width,bool gear){
    if(!gear)return StatsHeight(width);
    if(PlayerPortraitPicker::Choices.CompactParty)return StatsHeight(width)+HelpyImGuiTheme::Current.Y(8)+GearHeight(row,width);
    const auto columns=Columns(width);
    const auto stats=StatsHeight(columns.Stats),equipment=GearHeight(row,columns.Equipment);
    return columns.Beside?std::max(stats,equipment):stats+HelpyImGuiTheme::Current.Y(8)+equipment;
}
// Draw only: the member remains one selectable row, including its context menu.
inline bool GearRow(const nlohmann::json& row,ImVec2 start,float width,int64_t now){
    const auto& t=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    if(!EquipmentFresh(row,now)){draw->AddText({start.x,start.y+t.Y(18)},ImGui::GetColorU32(t.textDisabled),"Equipment unavailable");return false;}
    auto items=row["Equipment"]["Items"];if(items.empty()){draw->AddText({start.x,start.y+t.Y(18)},ImGui::GetColorU32(t.textDisabled),"No equipped items");return false;}
    const auto armor=[](const auto& item){const auto slot=item.value("Location",std::string{});return slot=="Head"||slot=="Body"||slot=="Legs"||slot=="Cape"||slot=="Trinket"||slot=="Trinket2"||slot=="Necklace";};
    std::stable_partition(items.begin(),items.end(),armor);const size_t armors=std::count_if(items.begin(),items.end(),armor);
    const float side=std::min(GearSide(),width);size_t lineIndex=0;bool hovered=false;
    for(const auto& line:PartyCardLayout::Centered(items.size(),width,side,t.X(6))){
        for(size_t col=0;col<line.Count;++col){const auto& item=items[line.First+col];
            const ImVec2 a{start.x+line.Offset+float(col)*(side+t.X(6)),start.y+t.Y(4)+float(lineIndex)*GearLineHeight()},b{a.x+side,a.y+side+t.Y(5)};
            draw->AddRectFilled(a,b,ImGui::GetColorU32(t.panelAlt),t.X(3));draw->AddRect(a,b,ImGui::GetColorU32(t.border),t.X(3));
            if(!ItemIconCache::Draw(ItemPresentation::Text(item,"Icon"),a,{b.x,a.y+side},true))HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Item,{a.x+side*.5f,a.y+side*.5f},side*.6f,t.icon);
            DurabilityMeter(item,a,b,now);
            if(line.First+col==armors&&armors&&armors<items.size())draw->AddLine({a.x-t.X(3),a.y+t.Y(3)},{a.x-t.X(3),b.y-t.Y(3)},ImGui::GetColorU32(t.accent),t.X(1));
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::PartyGear[ItemPresentation::Text(row,"Path")+":"+std::to_string(item.value("Slot",0))]={a.x,a.y,b.x,b.y};
            if(PartyStats::Read(item,"Durability",now))++HelpyPreviewHooks::PartyDurabilityMeters;
#endif
            const bool over=ImGui::IsItemHovered()&&ImGui::IsMouseHoveringRect(a,b);
            ImGui::PushID(int(line.First+col));
            if(over){hovered=true;EquipmentDetails(item,now);if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&RestoreGear)RestoreGear(row,item);if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)){GearContext=true;ImGui::OpenPopup("##equipped-item-actions");}}
            if(ImGui::BeginPopup("##equipped-item-actions")){GearContext=true;ImGui::TextUnformatted(ItemPresentation::Name(item).c_str());if(ImGui::MenuItem("Preview",nullptr,false,ItemPreviewRequest::Valid(item)))ItemPreviewRequest::Show(item);if(ImGui::MenuItem("Restore durability",nullptr,false,bool(RestoreGear)))RestoreGear(row,item);if(ImGui::MenuItem("Change gear…",nullptr,false,bool(ChangeGear)))ChangeGear(row,item);ImGui::EndPopup();}ImGui::PopID();
        }++lineIndex;
    }return hovered;
}
inline float CardHeight(const nlohmann::json& row,float width){
    const auto& t=HelpyImGuiTheme::Current;const auto& p=PlayerPortraitPicker::Choices;
    const auto now=Now();
    float height=BodyHeight(row,width,p.Equipment)+t.Y(12);
    const auto equipment=row.value("Equipment",nlohmann::json::object());
    if(!p.CompactParty&&p.Skills&&SkillsFresh(row,now)){const auto skills=equipment.value("Skills",nlohmann::json::object()).value("Items",nlohmann::json::array());const int columns=std::max(1,int((width+ImGui::GetStyle().ItemSpacing.x)/(t.Y(24)+ImGui::GetStyle().ItemSpacing.x)));height+=std::max(t.Y(18),float((std::min<size_t>(32,skills.size())+columns-1)/columns)*(t.Y(37)+ImGui::GetStyle().ItemSpacing.y))+ImGui::GetStyle().ItemSpacing.y;}
    return height+t.Y(4);
}
inline void Skills(const nlohmann::json& row,int64_t now){
    if(!SkillsFresh(row,now)){ImGui::TextDisabled("Skill levels unavailable");return;}const auto& t=HelpyImGuiTheme::Current;const auto& items=row["Equipment"]["Skills"]["Items"];const float side=t.Y(24);ImGui::PushID("skills");
    const int columns=std::max(1,int((ImGui::GetContentRegionAvail().x+ImGui::GetStyle().ItemSpacing.x)/(side+ImGui::GetStyle().ItemSpacing.x)));
    for(size_t i=0;i<items.size();++i){if(i%columns)ImGui::SameLine();ImGui::PushID(int(i));const auto a=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##skill",{side,side+t.Y(13)});
        ItemIconCache::Draw(ItemPresentation::Text(items[i],"Icon"),a,{a.x+side,a.y+side},true);const auto level=std::to_string(items[i].value("Level",0));const auto extent=ImGui::CalcTextSize(level.c_str());ImGui::GetWindowDrawList()->AddText({a.x+(side-extent.x)*.5f,a.y+side},ImGui::GetColorU32(t.text),level.c_str());
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s · Level %d",ItemPresentation::Text(items[i],"Name").c_str(),items[i].value("Level",0));ImGui::PopID();}ImGui::PopID();
}
inline void Advanced(){
    if(HelpyImGuiTheme::SoftButton("Advanced"))ImGui::OpenPopup("Party display");if(!ImGui::BeginPopup("Party display"))return;
    auto& p=PlayerPortraitPicker::Choices;bool changed=false;changed|=HelpyImGuiTheme::Toggle("Party placards",&p.CompactParty);changed|=HelpyImGuiTheme::Toggle("Vital meters",&p.Meters);changed|=HelpyImGuiTheme::Toggle("Equipped gear icons",&p.Equipment);changed|=HelpyImGuiTheme::Toggle("Skill icons in expanded rows",&p.Skills);changed|=HelpyImGuiTheme::Toggle("Native power level",&p.Power);changed|=HelpyImGuiTheme::Toggle("Host / role badge",&p.Roles);
    if(changed)PlayerPortraitPicker::SaveWanted=true;ImGui::TextWrapped("Double-click a portrait or use Change portrait in the member's right-click menu.");ImGui::EndPopup();
}
inline void Details(const nlohmann::json& row,int64_t now){
    HelpyImGuiTheme::BeginDetailsTooltip();PS::HelpyImGuiTheme::DetailsTextUnformatted(RuneSchemaProfile::Name(row).c_str());ImGui::Separator();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::PartyDetailSections.clear();
#endif
    for(const auto& entry:std::array<std::pair<const char*,const char*>,3>{{{"Hunger","Hunger"},{"Thirst","Thirst"},{"RestLevel","Rest"}}}){
        const auto [key,label]=entry;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyDetailSections+=std::string(key)+"|";
#endif
        if(const auto gauge=PartyStats::Read(row,key,now)){PS::HelpyImGuiTheme::DetailsText("%s: %.0f / %.0f",label,gauge->Current,gauge->Maximum);ImGui::ProgressBar(static_cast<float>(gauge->Fraction()),{0,HelpyImGuiTheme::Current.Y(6)},"");}
        else{const auto& stat=PartyStats::Find(row,key);const auto state=ItemPresentation::Text(stat,"State");
            if(!state.empty()&&PartyStats::Fresh(stat,now))PS::HelpyImGuiTheme::DetailsText("%s: %s",label,state.c_str());
            else PS::HelpyImGuiTheme::DetailsText("%s: %s",label,PartyStats::Reason(row,key,now).c_str());}
    }
    ImGui::SeparatorText("Skills");Skills(row,now);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::PartyDetailSections+="Skills";
#endif
    HelpyImGuiTheme::EndDetailsTooltip();
}
// One player-card design for map party members and item recipients.
inline bool Row(const nlohmann::json& row,bool selected,bool keepPopup=false,bool withEquipment=false){
    GearContext=false;
    const auto& t=HelpyImGuiTheme::Current;const auto start=ImGui::GetCursorScreenPos();const float side=t.Y(28);
    const auto& display=PlayerPortraitPicker::Choices;const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const bool gear=withEquipment&&display.Equipment;auto columns=Columns(width);if(display.CompactParty)columns={false,std::min(width,t.X(260)),width,0};const float statsWidth=gear?columns.Stats:width;
    const float height=BodyHeight(row,width,gear);const auto now=Now();
    const bool clicked=ImGui::Selectable("##member",selected,keepPopup?ImGuiSelectableFlags_NoAutoClosePopups:ImGuiSelectableFlags_None,{0,height});
    if(InspectorOpen&&ItemPresentation::Text(Inspector,"Path")==ItemPresentation::Text(row,"Path"))Inspector=row;
    const auto icon=PlayerPortraitPicker::Icon(row);bool ready=false;
    const float portraitHeight=StatsHeight(statsWidth);
    const auto power=display.Power?PartyStats::Power(row,now):std::optional<double>{};
    const float powerTop=start.y+std::max(0.f,(portraitHeight-side*2-t.Y(16))*.5f);
    const float portraitTop=power?powerTop+side+t.Y(2):start.y+(portraitHeight-side-t.Y(14))*.5f;
    const ImVec2 portraitMin{start.x,portraitTop},portraitMax{start.x+side,portraitTop+side};
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::PartyPortraits[ItemPresentation::Text(row,"Path")]={portraitMin.x,portraitMin.y,portraitMax.x,portraitMax.y};
#endif
    if(!icon.empty()&&icon.front()=='/')ready=PlayerPortraitPicker::DrawCached(icon,portraitMin,portraitMax);
    if(ImGui::IsItemHovered()&&ImGui::IsMouseHoveringRect(portraitMin,portraitMax)&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))PlayerPortraitPicker::Request(row);
    auto* draw=ImGui::GetWindowDrawList();
    const auto platform=ItemPresentation::Text(row,"Platform");const bool known=platform=="Steam"||platform=="Epic"||platform=="Game Pass"||platform=="Xbox"||platform=="PS5"||platform=="Switch 2"||platform=="Jagex";
    const ImVec2 platformMin{start.x,portraitMax.y+t.Y(2)},platformMax{start.x+side,portraitMax.y+t.Y(14)};
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::PartyPlatforms[ItemPresentation::Text(row,"Path")]={platformMin.x,platformMin.y,platformMax.x,platformMax.y};
#endif
    const auto platformIcon=PlatformIcon(platform);
    if(platformIcon.GetTexID())draw->AddImage(platformIcon,{start.x+(side-t.Y(12))*.5f,platformMin.y},{start.x+(side+t.Y(12))*.5f,platformMax.y},{0,0},{1,1},ImGui::GetColorU32(t.icon));
    else {const char* label=known?platform.c_str():"?";const float font=t.Y(8);const auto extent=ImGui::GetFont()->CalcTextSizeA(font,1000,0,label);draw->AddText(ImGui::GetFont(),font,{start.x+(side-extent.x)*.5f,platformMin.y},ImGui::GetColorU32(t.textDisabled),label);}
    const bool platformHovered=ImGui::IsItemHovered()&&ImGui::IsMouseHoveringRect(platformMin,platformMax);
    if(!ready){const ImVec2 center{start.x+side*.5f,portraitTop+side*.5f};const auto color=ImGui::GetColorU32(row.value("IsLocal",false)?t.accentHover:t.text);
        draw->AddCircle(center,side*.46f,color,20,1.2f);draw->AddCircleFilled({center.x,center.y-side*.12f},side*.13f,color);
        draw->AddBezierCubic({center.x-side*.27f,center.y+side*.25f},{center.x-side*.25f,center.y-side*.02f},{center.x+side*.25f,center.y-side*.02f},{center.x+side*.27f,center.y+side*.25f},color,1.6f);}
    const float textX=start.x+side+t.X(8),right=start.x+statsWidth-t.X(4);
    const auto* role=PlayerBadges::Find(row);const bool haveRole=display.Roles&&role&&role->value("Role",std::string("unknown"))!="unknown";
    const float nameRight=std::max(textX,right-(haveRole?t.X(19):0));
    const ImVec4 clip{textX,start.y,nameRight,start.y+height};
    auto name=RuneSchemaProfile::Name(row);if(name.empty())name="Unnamed player";if(row.value("IsLocal",false))name+=" (you)";
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{textX,start.y+t.Y(3)},ImGui::GetColorU32(t.text),name.c_str(),nullptr,0,&clip);
    const ImVec2 badge{std::min(nameRight,textX+ImGui::CalcTextSize(name.c_str()).x)+t.X(10),start.y+t.Y(10)};
    if(haveRole)PlayerBadges::Draw(draw,badge,t.Y(14),role);
    if(power&&width>side*3){
        const ImVec2 a{start.x,powerTop},b{start.x+side,powerTop+side};char value[32];std::snprintf(value,sizeof(value),"%.0f",*power);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyPower[ItemPresentation::Text(row,"Path")]={a.x,a.y,b.x,b.y};
#endif
        const bool native=ItemIconCache::Draw(QuickDecorations::PowerLevelBadge,a,b,true);if(!native)std::snprintf(value,sizeof(value),"PL %.0f",*power);
        const float font=t.Y(native?12:10);const auto extent=ImGui::GetFont()->CalcTextSizeA(font,1000,0,value);const ImVec2 at{a.x+(side-extent.x)*.5f,a.y+(side-extent.y)*.5f};
        draw->AddText(ImGui::GetFont(),font,{at.x+1,at.y+1},IM_COL32(0,0,0,240),value);draw->AddText(ImGui::GetFont(),font,at,ImGui::GetColorU32(t.text),value);
    }
    const char* keys[]{"Health","Stamina","SpecialPower"};const char* glyphs[]{"health","stamina","special"};
    const ImVec4 colors[]{ImVec4(.82f,.27f,.27f,1),ImVec4(.29f,.74f,.39f,1),ImVec4(.40f,.62f,.96f,1),ImVec4(.90f,.65f,.29f,1),ImVec4(.24f,.72f,.84f,1),ImVec4(.74f,.65f,.91f,1)};
    if(display.Meters)for(int i=0;i<3;++i){const float y=start.y+t.Y(26+i*13);const auto gauge=PartyStats::Read(row,keys[i],now);
        const float columnX=textX,barX=columnX+t.X(23),barRight=right,barWidth=std::max(0.0f,barRight-barX);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PartyMeters[ItemPresentation::Text(row,"Path")+":"+keys[i]]={barX,y,barRight,y+t.Y(12)};
#endif
        MapLayerIcons::Draw(glyphs[i],{columnX+t.X(7),y+t.Y(6)},t.Y(13));
        if(barWidth>1){draw->AddRectFilled({barX,y+t.Y(2)},{barRight,y+t.Y(10)},ImGui::GetColorU32(t.border),t.Y(2));
            if(gauge)draw->AddRectFilled({barX,y+t.Y(2)},{barX+barWidth*static_cast<float>(gauge->Fraction()),y+t.Y(10)},ImGui::GetColorU32(colors[i]),t.Y(2));
            char value[64];if(gauge)std::snprintf(value,sizeof(value),"%.0f / %.0f",gauge->Current,gauge->Maximum);else std::snprintf(value,sizeof(value),"unavailable");
            const ImVec4 barClip{barX,y,barRight,y+t.Y(12)};draw->AddText(ImGui::GetFont(),t.Y(10),{barX+t.X(4),y},ImGui::GetColorU32(t.text),value,nullptr,0,&barClip);
        }
    }
    const bool gearHovered=gear&&GearRow(row,columns.Beside?ImVec2{start.x+columns.Stats+columns.Gap,start.y}:ImVec2{start.x,start.y+StatsHeight(statsWidth)+t.Y(8)},columns.Equipment,now);
    if(platformHovered){const auto account=ItemPresentation::Text(row,"PlatformAccountName");const auto text=known?("Platform: "+platform+(account.empty()?"\nAccount name unavailable":"\nGame-reported account: "+account)):std::string("Platform not reported");HelpyImGuiTheme::TooltipAt(platformMin,platformMax,text.c_str());}
    else if(!gearHovered&&haveRole&&ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect({badge.x-t.X(8),badge.y-t.Y(8)},{badge.x+t.X(8),badge.y+t.Y(8)}))HelpyImGuiTheme::TooltipAt({badge.x-t.X(8),badge.y-t.Y(8)},{badge.x+t.X(8),badge.y+t.Y(8)},PlayerBadges::Tooltip(*role).c_str());
    else if(!gearHovered&&!GearContext&&ImGui::IsItemHovered()&&!ImGui::IsMouseDown(ImGuiMouseButton_Right))Details(row,now);
    return clicked&&!gearHovered;
}
inline bool ProfileOpen=false;inline nlohmann::json ProfilePlayer;inline uint64_t ProfileGeneration=0;
inline void ViewProfile(const nlohmann::json& row){ProfilePlayer=row;ProfileGeneration=SpawnToolRequests::CurrentGeneration();ProfileOpen=true;}
inline void RenderProfile(){
    if(!ProfileOpen)return;if(ProfileGeneration!=SpawnToolRequests::CurrentGeneration()){ProfileOpen=false;ProfilePlayer=nlohmann::json::object();return;}
    const auto& t=HelpyImGuiTheme::Current;HelpyImGuiTheme::Scope skin(t);const auto* viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*.5f,viewport->WorkPos.y+viewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});ImGui::SetNextWindowSize({std::min(t.X(400),ImGui::GetIO().DisplaySize.x-t.X(24)),0},ImGuiCond_Appearing);
    if(ImGui::Begin("Player profile###helpy-player-profile",&ProfileOpen,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("%s",RuneSchemaProfile::Name(ProfilePlayer).c_str());const auto profile=RuneSchemaProfile::PublicFor(ItemPresentation::Text(ProfilePlayer,"Guid"),ProfilePlayer.value("IsLocal",false));
        const auto platform=ItemPresentation::Text(ProfilePlayer,"Platform"),account=ItemPresentation::Text(ProfilePlayer,"PlatformAccountName");ImGui::TextDisabled("%s",platform.empty()?"Platform unavailable":platform.c_str());if(!account.empty())ImGui::TextWrapped("Game-reported account: %s",account.c_str());
        if(profile.is_null())ImGui::TextWrapped("No shared profile received. Both ends need the updated RuneSchema/Helpy.");
        else {
            ImGui::SeparatorText("Public details");for(const auto& [key,value]:profile.at("Social").items()){const auto text=value.get<std::string>();if(text.empty())continue;if(key=="Discord")ImGui::TextWrapped("Discord: %s",text.c_str());else if(ImGui::TextLink((key+" profile").c_str())){const std::wstring url(text.begin(),text.end());ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}}
            ImGui::TextDisabled("Shared by this player; links are not verified");
            if(ImGui::CollapsingHeader("Platforms played"))for(const auto& value:profile.at("Platforms"))ImGui::TextUnformatted(value.get_ref<const std::string&>().c_str());
        }
    }ImGui::End();
}
inline void RenderInspector(){
    RenderProfile();
    if(!InspectorOpen)return;if(InspectorGeneration!=SpawnToolRequests::CurrentGeneration()){InspectorOpen=false;Inspector=nlohmann::json::object();return;}
    const auto& t=HelpyImGuiTheme::Current;HelpyImGuiTheme::Scope skin(t);
    ImGui::SetNextWindowSize({t.X(560),t.Y(270)},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSizeConstraints({t.X(260),t.Y(120)},{ImGui::GetIO().DisplaySize.x-t.X(24),ImGui::GetIO().DisplaySize.y-t.Y(24)});
    HelpyImGuiTheme::CenterNextWindow();
    if(ImGui::Begin("Party equipment###party-equipment-inspector",&InspectorOpen)){
        ImGui::TextUnformatted(RuneSchemaProfile::Name(Inspector).c_str());ImGui::TextDisabled("Right-click equipped items for Preview, Restore durability, or Change gear.");
        const auto start=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;ImGui::InvisibleButton("##equipped-region",{width,GearHeight(Inspector,width)});GearRow(Inspector,start,width,Now());
    }ImGui::End();
}
}
