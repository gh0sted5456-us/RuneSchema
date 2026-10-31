#pragma once
#include "Core/HelpyBranding.h"
#include "Core/RuntimeDuration.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyTaskbar.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/RuntimeConnectionPanel.h"
#include "Core/HelpyPreviewHooks.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyBackgrounds.h"
#include "Runtime/HelpyUpdates.h"
#include "Runtime/HostServices.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <shellapi.h>
namespace PS::RuntimeDetailsPanel {
enum class Tab {Runtime,Attributions,Legal};
inline bool Open=false,Detached=false,FocusWanted=false,SelectTab=false;
inline HelpyWindowChrome::State Chrome;
inline Tab Selected=Tab::Runtime;
inline ImTextureRef GameLogo;
inline ImTextureRef ModManagerIcon;
inline bool VortexInstalled=false;
inline void RenderGameLogo(){
    const auto& theme=HelpyImGuiTheme::Current;const float available=ImGui::GetContentRegionAvail().x;
    if(GameLogo.GetTexID()!=ImTextureID{}){
        using G=HelpyBranding::GameWordmark;
        const float width=std::min(available*.86f,theme.X(280)),height=width*G::Aspect;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(available-width)*.5f));
        ImGui::Image(GameLogo,{width,height},{G::Left,G::Top},{G::Right,G::Bottom});
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::RuntimeGameLogo=HelpyPreviewHooks::LastRect();
#endif
    }else{const char* title="RuneScape: Dragonwilds";ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(available-ImGui::CalcTextSize(title).x)*.5f));ImGui::TextUnformatted(title);}
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
}
inline int LastSurface=-1;
inline nlohmann::json Startup=nlohmann::json::object();
inline std::string StatsStatus;
inline std::chrono::steady_clock::time_point NextPoll{};
inline void Request(Tab tab=Tab::Runtime){Open=true;Selected=tab;SelectTab=true;FocusWanted=true;NextPoll={};HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Runtime);}
inline void PollStats(){
    const auto now=std::chrono::steady_clock::now();if(now<NextPoll)return;NextPoll=now+std::chrono::seconds(1);
    nlohmann::json reply;
    if(RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","StartupStats"}},reply)&&reply.is_object()&&reply.value("Available",false)){
        Startup=std::move(reply);StatsStatus.clear();
    }else StatsStatus="Startup statistics are not available from this runtime yet.";
}
inline void RenderRuntime(){
    PollStats();const auto& theme=HelpyImGuiTheme::Current;
    RenderGameLogo();
    if(VortexInstalled){
        if(ModManagerIcon.GetTexID()){ImGui::Image(ModManagerIcon,{theme.X(24),theme.Y(24)});ImGui::SameLine();}
        ImGui::TextUnformatted("Vortex installed · Mod manager");
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Vortex is a mod manager. This indicator does not change the selected runtime or prove this installation is managed by Vortex.");
    }
    if(ImGui::BeginTable("runtime-versions",2,ImGuiTableFlags_SizingStretchProp)){
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthFixed,theme.X(120));ImGui::TableSetupColumn("Value");
        const auto row=[](const char* name,const std::string& value){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextDisabled("%s",name);ImGui::TableNextColumn();ImGui::TextWrapped("%s",value.empty()?"Not reported":value.c_str());};
        row("Platform",HelpyEnvironment::Label());row("RuneSchema",HelpyEnvironment::RuneSchemaVersion);
        row("Helpy (RSMK)",HelpyBranding::Version);row("UE4SS",HelpyEnvironment::UE4SS.value("Build",std::string{}));
        row("Game build",HelpyBranding::BuildLabel(HelpyEnvironment::GameVersion));
        if(HelpyEnvironment::Steam.contains("BuildId"))row("Steam build ID",HelpyEnvironment::Steam["BuildId"].dump());
        ImGui::EndTable();
    }
    if(ImGui::CollapsingHeader("Technical details")){
        ImGui::TextWrapped("Executable: %s",HelpyEnvironment::GameVersion.empty()?"Not reported":HelpyEnvironment::GameVersion.c_str());
        ImGui::TextWrapped("%s",HelpyEnvironment::UE4SS.value("Capabilities",std::string("Capabilities not reported")).c_str());
        ImGui::TextWrapped("%s",HelpyEnvironment::UE4SS.value("Source",std::string{}).c_str());
    }
    RuntimeConnectionPanel::Render();
    ImGui::SeparatorText("Startup statistics");
    auto& timeUnit=HelpyImGuiTheme::Current.runtimeTimeUnit;
    ImGui::SetNextItemWidth(theme.X(165));
    if(ImGui::Combo("Time units",&timeUnit,RuntimeDuration::Labels,4))HelpyImGuiTheme::SaveCurrent();
    const auto duration=[&](int64_t ms){return RuntimeDuration::Format(ms,static_cast<RuntimeDuration::Unit>(timeUnit));};
    if(!StatsStatus.empty())ImGui::TextWrapped("%s",StatsStatus.c_str());
    if(Startup.contains("Checkpoints")&&Startup["Checkpoints"].is_array()){
        const auto& checkpoints=Startup["Checkpoints"];
        if(!checkpoints.empty()&&checkpoints.back().value("ElapsedMs",int64_t(-1))>=0)
            ImGui::Text("%s: %s",Startup.value("Complete",false)?"Pipeline ready":"Latest checkpoint",duration(checkpoints.back()["ElapsedMs"].get<int64_t>()).c_str());
        ImGui::TextWrapped("RuneSchema startup checkpoints only, not total game or world loading. Deferred world work may remain.");
        if(ImGui::BeginTable("startup-checkpoints",2,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,theme.Y(165)})){
            ImGui::TableSetupColumn("Checkpoint");ImGui::TableSetupColumn("Elapsed",ImGuiTableColumnFlags_WidthFixed,theme.X(110));ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            size_t shown=0;for(const auto& checkpoint:checkpoints){if(++shown>64)break;
                if(!checkpoint.is_object()||!checkpoint.contains("Stage")||!checkpoint["Stage"].is_string()||!checkpoint.contains("ElapsedMs")||!checkpoint["ElapsedMs"].is_number_integer())continue;
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextWrapped("%s",checkpoint["Stage"].get_ref<const std::string&>().c_str());
                ImGui::TableNextColumn();ImGui::TextUnformatted(duration(checkpoint["ElapsedMs"].get<int64_t>()).c_str());
            }ImGui::EndTable();
        }
    }
    if(HelpyImGuiTheme::SoftButton("Refresh statistics"))NextPoll={};
    const auto& news=HelpyUpdates::Current();
    if(news.GameUpdateDate>0){
        const auto stamp=static_cast<time_t>(news.GameUpdateDate);tm utc{};char date[64]{};
        if(gmtime_s(&utc,&stamp)==0)std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M UTC",&utc);
        ImGui::SeparatorText("Game announcement");ImGui::TextWrapped("%s",news.GameUpdateTitle.c_str());ImGui::TextDisabled("Published %s",date);
        ImGui::TextWrapped("Announcement date, not proof of the installed build's release date.");
        if(HelpyImGuiTheme::SoftButton("Read official update")){const std::wstring url(news.GameUpdateUrl.begin(),news.GameUpdateUrl.end());ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    }
}
inline void RenderContents(){
    const int surface=Detached?1:0;if(LastSurface!=surface){LastSurface=surface;SelectTab=true;}
    if(!HelpyImGuiTheme::NativePresentation&&HelpyImGuiTheme::SoftButton(Detached?"Dock back":"Detach window"))Detached=!Detached;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RuntimeDetach=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::BeginTabBar("Runtime details tabs",ImGuiTabBarFlags_Reorderable)){
        const auto requested=Selected;const bool select=SelectTab;
        for(const auto tab:{Tab::Runtime,Tab::Attributions,Tab::Legal}){
            const char* name=tab==Tab::Runtime?"Runtime":tab==Tab::Attributions?"Attributions":"Legal";
            if(ImGui::BeginTabItem(name,nullptr,select&&requested==tab?ImGuiTabItemFlags_SetSelected:0)){
                Selected=tab;
                if(ImGui::BeginChild("runtime-tab-body",{0,0},false)){
                    if(tab==Tab::Runtime)RenderRuntime();else{
                        ImGui::TextWrapped("%s",tab==Tab::Attributions?HelpyBranding::Credits:HelpyBranding::Legal);
                        if(tab==Tab::Legal&&HelpyImGuiTheme::SoftButton("Open complete legal notice")){
                            const auto file=HostServices::ModDirectory()/"settings"/"licenses"/"RuneSchema-Licenses.txt";
                            ShellExecuteW(nullptr,L"open",file.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                        }
                    }
                }ImGui::EndChild();ImGui::EndTabItem();
            }
        }ImGui::EndTabBar();SelectTab=false;
    }
}
inline void RenderDocked(const HelpyImGuiTheme::Theme& theme,ImVec2 origin,ImVec2 size){
    if(!Open||Detached||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Runtime))return;
    ImGui::SetNextWindowSize({std::min(theme.X(640),size.x-theme.X(24)),std::min(theme.Y(570),size.y-theme.Y(24))},ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({origin.x+size.x*.5f,origin.y+size.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints({theme.X(280),theme.Y(220)},{std::max(theme.X(280),size.x-theme.X(16)),std::max(theme.Y(220),size.y-theme.Y(16))});
    if(FocusWanted){ImGui::SetNextWindowFocus();FocusWanted=false;}
    HelpyImGuiTheme::Scope skin(theme);
    HelpyWindowChrome::Prepare(Chrome);
    if(ImGui::Begin("Runtime details",&Open,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse)){HelpyBackgrounds::DrawWindow();HelpyWindowChrome::Render(Chrome,HelpyTaskbar::Window::Runtime);RenderContents();}ImGui::End();
}
}
