#pragma once
#include "Core/RuneSchemaProfile.h"
#include "Core/PlayerStatus.h"
#include "Generator/PlayerPortraitPicker.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/SaveEditorPanel.h"
#include "Generator/PartyMemberView.h"
#include <shellapi.h>
namespace PS::ProfilePanel {
inline uint64_t DraftRevision=UINT64_MAX;
inline char Name[65]{},Steam[257]{},Nexus[257]{},Discord[129]{};
inline bool Dirty=false;
inline void Refresh(){const auto doc=RuneSchemaProfile::Read();std::snprintf(Name,sizeof(Name),"%s",doc.value("ProfileName",std::string{}).c_str());const auto social=doc.value("Social",nlohmann::json::object());std::snprintf(Steam,sizeof(Steam),"%s",social.value("Steam",std::string{}).c_str());std::snprintf(Nexus,sizeof(Nexus),"%s",social.value("Nexus",std::string{}).c_str());std::snprintf(Discord,sizeof(Discord),"%s",social.value("Discord",std::string{}).c_str());DraftRevision=RuneSchemaProfile::Revision;Dirty=false;}
inline std::string PlatformFilter;
inline char CharacterSearch[129]{};
#ifdef HELPY_PREVIEW
inline std::string OpenedLink;
inline size_t PlatformLogos=0;
inline std::map<std::string,ImVec4> CharacterRows;
#endif
inline std::string PlatformKey(const std::string& value){return value=="Xbox"?"Game Pass":value=="Epic Games"?"Epic":value;}
inline bool MatchesPlatform(const nlohmann::json& row){if(PlatformFilter.empty())return true;for(const auto& value:row.at("Platforms"))if(PlatformKey(value.get<std::string>())==PlatformFilter)return true;return false;}
inline void OpenLink(const char* key,const char* value){
    if((std::string_view(key)!="Steam"&&std::string_view(key)!="Nexus")||!*value||!RuneSchemaProfile::Link(key,value))return;
#ifdef HELPY_PREVIEW
    OpenedLink=value;
#else
    const std::string link(value);const std::wstring wide(link.begin(),link.end());ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
}
inline void SocialInput(const char* key,const char* hint,char* value,size_t size){
    const auto& t=HelpyImGuiTheme::Current;ImGui::PushID(key);const bool discord=std::string_view(key)=="Discord";
    ImGui::SetNextItemWidth(std::max(t.X(80),ImGui::GetContentRegionAvail().x-t.X(68)-ImGui::GetStyle().ItemSpacing.x));
    Dirty|=ImGui::InputTextWithHint("##link",hint,value,size);ImGui::SameLine();
    ImGui::BeginDisabled(!*value||!RuneSchemaProfile::Link(key,value));if(HelpyImGuiTheme::SoftButton(discord?"Copy":"Open",{t.X(68),0})){if(discord)ImGui::SetClipboardText(value);else OpenLink(key,value);}ImGui::EndDisabled();
#ifdef HELPY_PREVIEW
    HelpyPreviewHooks::AuthoringButtons[std::string("Profile ")+key]=HelpyPreviewHooks::LastRect();
#endif
    ImGui::PopID();
}
inline bool Render(const nlohmann::json& catalog){
    namespace P=RuneSchemaProfile;const auto& t=HelpyImGuiTheme::Current;bool openCharacter=false;
    if(P::Migration.valid()&&P::Migration.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto error=P::Migration.get();if(!error.empty())P::Status=error;}
    if(!P::Ready){ImGui::TextWrapped("Profile unavailable: %s",P::Status.c_str());if(HelpyImGuiTheme::SoftButton("Retry profile"))P::Load(HostServices::StateDirectory());return false;}
    if(!Dirty&&DraftRevision!=P::Revision)Refresh();const auto doc=P::Read();const auto* local=PlayerStatus::LocalCharacter(catalog);
    const auto id=doc.at("ProfileId").get<std::string>(),localGuid=local?HelpyPersonalization::Guid(local->value("Guid",std::string{})):std::string{};
    const bool named=!doc.at("ProfileName").get<std::string>().empty();const nlohmann::json account={{"Guid",id},{"Name",named?doc.at("ProfileName").get<std::string>():std::string("Profile")}};
    if(ImGui::BeginChild("##profile-account",{0,t.Y(112)},ImGuiChildFlags_Borders)){
        const auto a=ImGui::GetCursorScreenPos();const float side=t.Y(84);ImGui::BeginGroup();
        const bool picture=ImGui::InvisibleButton("##account-portrait",{side,side});
#ifdef HELPY_PREVIEW
        HelpyPreviewHooks::AuthoringButtons["Profile portrait"]=HelpyPreviewHooks::LastRect();
#endif
        const bool portraitHover=ImGui::IsItemHovered();
        if(!PlayerPortraitPicker::DrawCached(PlayerPortraitPicker::Icon(account),a,{a.x+side,a.y+side}))PortraitArt::Draw(0,{a.x+side*.2f,a.y+side*.2f},{a.x+side*.8f,a.y+side*.8f},ImGui::GetColorU32(t.accent));
        ImGui::GetWindowDrawList()->AddCircle({a.x+side*.5f,a.y+side*.5f},side*.49f,ImGui::GetColorU32(portraitHover?t.accent:t.border),0,t.X(1));
        if(picture)PlayerPortraitPicker::Request(account);if(portraitHover)HelpyImGuiTheme::Tooltip("Change your picture · drop and crop an image in the picker");
        ImGui::EndGroup();ImGui::SameLine(0,t.X(16));ImGui::BeginGroup();
        if(named){ImGui::TextUnformatted(Name);if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Your permanent profile name");}
        else {ImGui::SetNextItemWidth(std::min(t.X(320),ImGui::GetContentRegionAvail().x));Dirty|=ImGui::InputTextWithHint("##profile-name","Choose your permanent profile name",Name,sizeof(Name));}
        if(local)ImGui::TextDisabled("Playing: %s",local->value("Name",std::string("Player")).c_str());else ImGui::TextDisabled("One profile across your characters");
        if(HelpyImGuiTheme::SoftButton("Picture..."))PlayerPortraitPicker::Request(account);ImGui::SameLine();
        ImGui::BeginDisabled(!Dirty||(!named&&HelpyPersonalization::Trim(Name).empty()));
        if(HelpyImGuiTheme::SoftButton(named?"Save details":"Create profile")){if(P::Save(Name,{{"Steam",std::string(Steam)},{"Nexus",std::string(Nexus)},{"Discord",std::string(Discord)}}))Refresh();}
        ImGui::EndDisabled();ImGui::EndGroup();
    }ImGui::EndChild();
    if(!P::Status.empty())ImGui::TextWrapped("%s",P::Status.c_str());
#ifdef HELPY_PREVIEW
    PlatformLogos=0;CharacterRows.clear();
#endif
    ImGui::SeparatorText("Platforms");
    std::set<std::string> platforms;for(const auto& value:doc.at("Platforms"))platforms.insert(PlatformKey(value.get<std::string>()));
    if(!PlatformFilter.empty()&&!platforms.contains(PlatformFilter))PlatformFilter.clear();
    float used=0;const float available=ImGui::GetContentRegionAvail().x,gap=ImGui::GetStyle().ItemSpacing.x;
    for(const auto& platform:platforms){
        const float width=std::min(available,std::max(t.X(112),ImGui::CalcTextSize(platform.c_str()).x+t.X(48))),height=t.Y(34);
        if(used&&used+gap+width<=available){ImGui::SameLine(0,gap);used+=gap;}else used=0;
        ImGui::PushID(platform.c_str());const auto a=ImGui::GetCursorScreenPos();if(HelpyImGuiTheme::SoftButton("##platform",{width,height}))PlatformFilter=PlatformFilter==platform?std::string{}:platform;
#ifdef HELPY_PREVIEW
        HelpyPreviewHooks::AuthoringButtons["Profile platform "+platform]=HelpyPreviewHooks::LastRect();
#endif
        const auto icon=PartyMemberView::PlatformIcon(platform);const float side=t.Y(22);auto* draw=ImGui::GetWindowDrawList();
        if(icon.GetTexID()){draw->AddImage(icon,{a.x+t.X(8),a.y+(height-side)*.5f},{a.x+t.X(8)+side,a.y+(height+side)*.5f});
#ifdef HELPY_PREVIEW
            ++PlatformLogos;
#endif
        }
        draw->AddText({a.x+t.X(38),a.y+(height-ImGui::GetTextLineHeight())*.5f},ImGui::GetColorU32(t.text),platform.c_str());
        if(PlatformFilter==platform)draw->AddRect(a,{a.x+width,a.y+height},ImGui::GetColorU32(t.accent),t.X(4));
        if(ImGui::IsItemHovered()){const auto liveName=local&&PlatformKey(local->value("Platform",std::string{}))==platform?ItemPresentation::Text(*local,"PlatformAccountName"):std::string{};
            const auto nativeName=liveName.empty()?doc.value("PlatformAccounts",nlohmann::json::object()).value(platform,std::string{}):liveName;
            HelpyImGuiTheme::BeginDetailsTooltip(220);ImGui::TextUnformatted(platform.c_str());ImGui::Separator();ImGui::TextWrapped("%s",nativeName.empty()?"Observed platform · account name unavailable":((liveName.empty()?"Last observed account: ":"Platform account: ")+nativeName).c_str());ImGui::TextDisabled("Click to filter characters");HelpyImGuiTheme::EndDetailsTooltip();}
        used+=width;ImGui::PopID();
    }
    if(platforms.empty())ImGui::TextDisabled("Play a character to link a platform.");
    ImGui::SeparatorText("Characters");ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##profile-character-search","Find a linked character...",CharacterSearch,sizeof(CharacterSearch));
    const auto query=HelpyPersonalization::Fold(CharacterSearch);size_t matches=0;
    for(const auto& [guid,row]:doc.at("Characters").items())if(MatchesPlatform(row)&&HelpyPersonalization::Fold(row.at("Name").get<std::string>()).find(query)!=std::string::npos)++matches;
    const float visibleRows=float(std::clamp<size_t>(matches,1,3)),listHeight=visibleRows*t.Y(40)+(visibleRows-1)*ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y*2+ImGui::GetStyle().ChildBorderSize*2;
    if(ImGui::BeginChild("##profile-characters",{0,listHeight},ImGuiChildFlags_Borders)){
        if(!matches)ImGui::TextDisabled("No matching characters.");
        for(const auto& [guid,row]:doc.at("Characters").items()){
            if(!MatchesPlatform(row)||HelpyPersonalization::Fold(row.at("Name").get<std::string>()).find(query)==std::string::npos)continue;
            ImGui::PushID(guid.c_str());const auto a=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,height=t.Y(40);const bool current=guid==localGuid;
            const bool clicked=ImGui::Selectable("##character",current,0,{width,height});auto* draw=ImGui::GetWindowDrawList();
#ifdef HELPY_PREVIEW
            CharacterRows[guid]=HelpyPreviewHooks::LastRect();
#endif
            float right=a.x+width-t.X(8);for(auto it=row.at("Platforms").rbegin();it!=row.at("Platforms").rend();++it){const auto icon=PartyMemberView::PlatformIcon(it->get<std::string>());if(!icon.GetTexID())continue;const float side=t.Y(18);draw->AddImage(icon,{right-side,a.y+(height-side)*.5f},{right,a.y+(height+side)*.5f});right-=side+t.X(6);}
            if(current){const char* text="Live";right-=ImGui::CalcTextSize(text).x+t.X(8);draw->AddText({right,a.y+(height-ImGui::GetTextLineHeight())*.5f},ImGui::GetColorU32(t.accent),text);}
            const ImVec4 clip{a.x,a.y,right-t.X(6),a.y+height};draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{a.x+t.X(6),a.y+(height-ImGui::GetTextLineHeight())*.5f},ImGui::GetColorU32(t.text),row.at("Name").get_ref<const std::string&>().c_str(),nullptr,0,&clip);
            if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip(220);ImGui::TextUnformatted(row.at("Name").get_ref<const std::string&>().c_str());for(const auto& value:row.at("Platforms"))ImGui::TextDisabled("%s",value.get_ref<const std::string&>().c_str());ImGui::Separator();ImGui::TextWrapped("Open an editable copy. Native saves stay unchanged.");HelpyImGuiTheme::EndDetailsTooltip();}
            if(clicked){const auto platform=!PlatformFilter.empty()?PlatformFilter:current&&local?PlatformKey(local->value("Platform",std::string{})):row.at("Platforms").size()==1?PlatformKey(row.at("Platforms")[0].get<std::string>()):std::string{};openCharacter=SaveEditorPanel::RequestProfileCharacter(guid,platform);}
            ImGui::PopID();
        }
    }ImGui::EndChild();
    const bool links=ImGui::CollapsingHeader("Social links");
#ifdef HELPY_PREVIEW
    HelpyPreviewHooks::AuthoringButtons["Profile links"]=HelpyPreviewHooks::LastRect();
#endif
    if(links){SocialInput("Steam","Steam profile link",Steam,sizeof(Steam));SocialInput("Nexus","Nexus profile link",Nexus,sizeof(Nexus));SocialInput("Discord","Discord username",Discord,sizeof(Discord));ImGui::TextDisabled("Shared with connected Helpy users after saving.");}
    if(ImGui::CollapsingHeader("Loadouts"))SpawnToolsPanel::RenderKitLibrary();
    if(ImGui::CollapsingHeader("Spawn browser presets"))SpawnBrowserSettings::PresetControls();
    if(ImGui::CollapsingHeader("More")){
        if(HelpyImGuiTheme::SoftButton(Dirty?"Discard draft":"Reload")){if(Dirty){Refresh();P::Status.clear();}else {P::Load(HostServices::StateDirectory());Refresh();}}ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Open profile folder"))ShellExecuteW(nullptr,L"open",HostServices::ProfileDirectory().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::TextWrapped("Private tags stay local. Names, links and platforms are public. Portrait sync and delegated server grants are not active yet.");
    }
    return openCharacter;
}
}
