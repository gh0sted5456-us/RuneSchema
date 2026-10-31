#pragma once
#include "Generator/HelpyMenu.h"
#include "Core/ProcessExitFuture.h"

#include "Core/DiagnosticCenterModel.h"
#include "Core/StructuredDiagnosticAdvice.h"
#include "Core/StructuredDiagnosticRegistry.h"
#include "Core/DiagnosticIssueFilters.h"
#include "Core/DiagnosticIssuePresentation.h"
#include "Core/DiagnosticIssueNavigation.h"
#include "Core/DiagnosticHistoryNavigation.h"
#include "Core/DiagnosticCenterHistory.h"
#include "Core/DiagnosticCenterMutes.h"
#include "Core/DiagnosticCenterReport.h"
#include "Generator/LiveLogPanel.h"
#include "Generator/HelpyImGuiTheme.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <future>
#include <fstream>
#include <ctime>
#include <limits>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_set>

// HELPY-native diagnostics workspace. It is a viewer over the single log-reader
// callback, not a second reader or an engine-side reflection/diagnostic scanner.
namespace PS::DiagnosticsPanel {
enum class View { Overview, Issues, LiveLog };
inline View Current=View::Overview;
inline int RequestedTab=-1;
inline bool ShowAll=false,OnlyNew=false,Maximized=false,DetailOnly=false;
inline bool GroupByOperation=true;
inline int SeverityScope=0;
inline std::unordered_set<std::uint64_t> ExpandedOperations;
inline bool RevealSelectedInList=false;
inline std::uint64_t SelectedGroup=0;
inline float SplitRatio=.40f;
inline char Query[256]{};
inline int SortMode=0;
inline bool OpenOccurrences=false,OpenTechnical=false,OpenRelated=false;
inline std::uint64_t LastInspected=0;
inline DiagnosticCenterMutes::Manager* MuteManager=nullptr;
inline DiagnosticCenterHistory::Service* SessionHistory=nullptr;
inline DiagnosticCenterModel::Store ArchivedStore;
inline std::string ArchivedSessionId;
inline bool Historical=false;
inline bool SplitInitialized=false;
inline std::uint64_t SelectedHistoricalEvent=0;
inline std::uint64_t SelectedOccurrenceEvent=0;
inline bool RevealHistoricalEvent=false;
inline std::string SessionStatus;
inline DiagnosticCenterMutes::Scope PendingMuteScope=DiagnosticCenterMutes::Scope::Exact;
inline DiagnosticCenterMutes::Duration PendingMuteDuration=DiagnosticCenterMutes::Duration::Session;

// D4 export captures one bounded snapshot on explicit user action. No log
// additional reader or UE scanner is started by report rendering.
enum class ExportKind { Issue, Session };
inline ExportKind PendingExportKind=ExportKind::Issue;
inline DiagnosticCenterModel::Store PendingReportStore;
inline std::uint64_t PendingExportGroup=0;
inline std::string PendingExportSession,PendingReportText,PendingExportStatus;
inline DiagnosticCenterReport::Options PendingReportOptions{};
inline bool RequestExportPopup=false;
inline PS::ProcessExitFuture<std::string> ReportWorker;
inline void RequestReport(DiagnosticCenterModel::Store& store,ExportKind kind,std::uint64_t group=0){
    if(kind==ExportKind::Issue&&!store.FindGroup(group))return;
    PendingReportStore=store;PendingExportKind=kind;PendingExportGroup=group;
    PendingExportSession=Historical?"Previous session: "+ArchivedSessionId:"Current session";
    PendingReportOptions={};PendingReportOptions.IncludeEvidence=false;RequestExportPopup=true;
}
inline void RefreshReport(){
    if(PendingExportKind==ExportKind::Issue){
        const auto* group=PendingReportStore.FindGroup(PendingExportGroup);
        PendingReportText=group?DiagnosticCenterReport::Issue(PendingReportStore,*group,PendingExportSession,PendingReportOptions):std::string{};
    }else PendingReportText=DiagnosticCenterReport::Session(PendingReportStore,PendingExportSession,PendingReportOptions);
}
inline void Shutdown(){
    if(ReportWorker.valid()){
        try{PendingExportStatus=ReportWorker.get();}catch(...){PendingExportStatus="Report export failed during shutdown.";}
    }
}
inline void PollExport(){
    if(ReportWorker.valid()&&ReportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{PendingExportStatus=ReportWorker.get();}
        catch(...){PendingExportStatus="Diagnostic report export failed.";}
    }
}
inline void SaveReport(){
#ifdef _WIN32
    wchar_t file[32768]=L"RuneSchema-diagnostics.txt";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();
    dialog.lpstrFile=file;dialog.nMaxFile=static_cast<DWORD>(sizeof(file)/sizeof(file[0]));
    dialog.lpstrFilter=L"Text reports (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    dialog.lpstrDefExt=L"txt";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
    if(!GetSaveFileNameW(&dialog))return;
    const std::filesystem::path destination=file;
    if(!DiagnosticCenterReport::IsReportDestination(destination)){
        PendingExportStatus="Choose a .txt report, not a mod list, configuration or save file.";return;
    }
    // Export must never truncate the authoritative RuneSchema.log or backup.
    std::error_code ec;
    const auto target=std::filesystem::weakly_canonical(destination,ec);
    if(ec){PendingExportStatus="Cannot resolve selected report path.";return;}
    const auto log=std::filesystem::weakly_canonical(LiveLogPanel::Path(),ec);
    if(!ec){
        const auto a=LiveLog::Lower(target.generic_string());
        const auto b=LiveLog::Lower(log.generic_string());
        if(a==b||a==b+".previous"){
            PendingExportStatus="Refusing to overwrite RuneSchema.log or its backup.";return;
        }
    }
    auto contents=PendingReportText;
    ReportWorker=std::async(std::launch::async,[destination,contents=std::move(contents)]{
        std::ofstream out(destination,std::ios::binary|std::ios::trunc);
        if(!out)return std::string("Report export failed: cannot open destination.");
        out.write(contents.data(),static_cast<std::streamsize>(contents.size()));out.flush();
        return out?std::string("Report exported. Review for private identifiers before sharing."):
                   std::string("Report export failed: destination may be incomplete.");
    });
    PendingExportStatus="Exporting diagnostic report...";
#else
    PendingExportStatus="Native report-save dialog is available on Windows builds only.";
#endif
}
inline bool ActionButton(const char* id,HelpyImGuiTheme::Icon icon,const char* label,
                         const HelpyImGuiTheme::Theme& theme,float logicalWidth);
inline void DrawExportPopup(const HelpyImGuiTheme::Theme& theme){
    PollExport();
    if(RequestExportPopup){
        ImGui::OpenPopup("Review diagnostic report##helpy-d4");RequestExportPopup=false;RefreshReport();
        ImGui::SetNextWindowSize(ImVec2(theme.X(790.0f),theme.Y(560.0f)),ImGuiCond_Appearing);
    }
    const auto work=ImGui::GetMainViewport()->WorkSize;
    const float maxW=std::max(1.0f,work.x-theme.X(28.0f));
    const float maxH=std::max(1.0f,work.y-theme.Y(28.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(theme.X(420.0f),maxW),std::min(theme.Y(320.0f),maxH)),
        ImVec2(maxW,maxH));
    if(HelpyImGuiTheme::BeginCenteredModal("Review diagnostic report##helpy-d4",nullptr,ImGuiWindowFlags_None)){
        ImGui::TextWrapped("Export %s from %s",
            PendingExportKind==ExportKind::Issue?"selected issue":"selected session",PendingExportSession.c_str());
        bool edited=ImGui::Checkbox("Include original retained log evidence",&PendingReportOptions.IncludeEvidence);
        edited|=ImGui::Checkbox("Hide Windows user-home account names (best effort)",&PendingReportOptions.RedactUserPaths);
        if(edited)RefreshReport();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(theme.X(650.0f),ImGui::GetContentRegionAvail().x));
        ImGui::TextColored(theme.warning,"Warning: raw messages may contain player names, IDs, file paths or private data. Always inspect the final report before sharing.");
        ImGui::PopTextWrapPos();
        ImGui::TextDisabled("Preview: first 6 KiB of %zu bytes%s",PendingReportText.size(),
            PendingReportText.size()>6144?" (truncated)":"");
        const std::string excerpt=PendingReportText.substr(0,6144);
        const float previewHeight=std::max(theme.Y(36.0f),ImGui::GetContentRegionAvail().y-theme.Y(65.0f));
        if(ImGui::BeginChild("##diagnostics-export-preview",ImVec2(0,previewHeight),true))
            ImGui::TextWrapped("%s",excerpt.c_str());
        ImGui::EndChild();
        const float actionAvail=ImGui::GetContentRegionAvail().x;
        const bool stackActions=actionAvail<theme.X(320.0f);
        const float actionWidth=stackActions?actionAvail:std::min(theme.X(150.0f),
            (actionAvail-ImGui::GetStyle().ItemSpacing.x*2)/3.0f);
        if(ActionButton("report-copy",HelpyImGuiTheme::Icon::Copy,"Copy",theme,actionWidth/theme.Scale()))
            ImGui::SetClipboardText(PendingReportText.c_str());
        if(!stackActions)ImGui::SameLine();
        ImGui::BeginDisabled(ReportWorker.valid());
        if(ActionButton("report-save",HelpyImGuiTheme::Icon::Export,"Save",theme,actionWidth/theme.Scale()))
            {SaveReport();if(ReportWorker.valid())ImGui::CloseCurrentPopup();}
        ImGui::EndDisabled();
        if(!stackActions)ImGui::SameLine();
        if(ActionButton("report-close",HelpyImGuiTheme::Icon::Acknowledge,"Close",theme,actionWidth/theme.Scale()))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(!PendingExportStatus.empty())ImGui::TextDisabled("%s",PendingExportStatus.c_str());
}
inline const ImVec4& SeverityColor(LiveLog::Level level,const HelpyImGuiTheme::Theme& t){
    switch(level){case LiveLog::Level::Error:return t.error;case LiveLog::Level::Warning:return t.warning;
    case LiveLog::Level::Diagnostic:return t.accent;default:return t.info;}
}
inline void DrawSeverityIcon(const ImVec2& center,float size,LiveLog::Level level,const HelpyImGuiTheme::Theme& theme){
    auto* draw=ImGui::GetWindowDrawList();
    const ImU32 color=ImGui::GetColorU32(SeverityColor(level,theme));
    const float r=size*0.5f;
    const float w=std::max(1.0f,size*0.11f);
    switch(level){
    case LiveLog::Level::Error:{
        const ImVec2 markCenter(center.x-size*0.022f,center.y-size*0.012f);
        draw->AddCircle(center,r*0.92f,color,0,w);
        draw->AddLine(ImVec2(markCenter.x-r*0.32f,markCenter.y-r*0.32f),ImVec2(markCenter.x+r*0.32f,markCenter.y+r*0.32f),color,w);
        draw->AddLine(ImVec2(markCenter.x+r*0.32f,markCenter.y-r*0.32f),ImVec2(markCenter.x-r*0.32f,markCenter.y+r*0.32f),color,w);
        break;}
    case LiveLog::Level::Warning:{
        const ImVec2 p1(center.x,center.y-r*0.92f),p2(center.x-r*0.86f,center.y+r*0.72f),p3(center.x+r*0.86f,center.y+r*0.72f);
        draw->AddTriangle(p1,p2,p3,color,w);
        draw->AddLine(ImVec2(center.x,center.y-r*0.30f),ImVec2(center.x,center.y+r*0.18f),color,w);
        draw->AddCircleFilled(ImVec2(center.x,center.y+r*0.46f),w,color,10);
        break;}
    case LiveLog::Level::Diagnostic:
    default:
        draw->AddCircle(center,r*0.92f,color,16,w);
        draw->AddCircleFilled(ImVec2(center.x,center.y-r*0.28f),w,color,10);
        draw->AddLine(ImVec2(center.x,center.y-r*0.04f),ImVec2(center.x,center.y+r*0.42f),color,w);
        break;
    }
}
inline std::string TimeLabel(DiagnosticCenterModel::Clock::time_point time){
    const auto instant=DiagnosticCenterModel::Clock::to_time_t(time);
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out,&instant);
#else
    localtime_r(&instant,&out);
#endif
    char label[40]{};
    std::strftime(label,sizeof(label),"%b %d %H:%M:%S",&out);
    return label;
}
inline void ResetIssueFilters(){
    Query[0]='\0';SeverityScope=0;ShowAll=false;OnlyNew=false;SortMode=0;
}
inline void Choose(DiagnosticCenterModel::Store& store,std::uint64_t id){
    SelectedGroup=id;Current=View::Issues;RequestedTab=1;DetailOnly=true;
    RevealSelectedInList=true;
    if(const auto* group=store.FindGroup(id)){
        if(!DiagnosticCenterModel::Actionable(group->Severity)){
            ShowAll=true;SeverityScope=3;OnlyNew=false;
        }else if(SeverityScope==3 ||
                 (SeverityScope==1&&group->Severity!=LiveLog::Level::Error)||
                 (SeverityScope==2&&group->Severity!=LiveLog::Level::Warning)){
            SeverityScope=0;OnlyNew=false;
        }
        if(OnlyNew&&group->Viewed)OnlyNew=false;
        if(!DiagnosticIssueFilters::Matches(*group,Query,ShowAll,OnlyNew,SeverityScope))
            Query[0]='\0';
        if(!Historical)store.MarkViewed(id);
    }
}
inline bool Matches(const DiagnosticCenterModel::Group& group,std::string_view query){
    return DiagnosticIssueFilters::Matches(group,query,ShowAll,OnlyNew,SeverityScope);
}
// Presentation helpers. These never change the underlying diagnostic identity
// or original evidence. All widths are ImGui layout pixels, not hardcoded glyph counts.
inline std::string Shorten(std::string_view value,std::size_t maxBytes=140){
    if(value.size()<=maxBytes)return std::string(value);
    if(maxBytes<6)return std::string(value.substr(0,maxBytes));
    std::size_t cut=maxBytes-3;
    while(cut>0 && (static_cast<unsigned char>(value[cut])&0xc0u)==0x80u)--cut;
    return std::string(value.substr(0,cut))+"...";
}
inline std::string Fitted(std::string_view text,float width){
    if(width<=ImGui::CalcTextSize("...").x)return "...";
    std::string source(text);
    if(ImGui::CalcTextSize(source.c_str()).x<=width)return source;
    const float dotWidth=ImGui::CalcTextSize("...").x;
    std::size_t lo=0,hi=source.size();
    while(lo<hi){
        const auto mid=lo+(hi-lo+1)/2;
        std::size_t end=mid;
        while(end>0&&end<source.size()&&(static_cast<unsigned char>(source[end])&0xc0u)==0x80u)--end;
        if(ImGui::CalcTextSize(source.data(),source.data()+end).x+dotWidth<=width)lo=mid;
        else hi=mid-1;
    }
    std::size_t cut=lo;
    while(cut>0&&cut<source.size()&&(static_cast<unsigned char>(source[cut])&0xc0u)==0x80u)--cut;
    return source.substr(0,cut)+"...";
}
inline std::string IssueTitle(const DiagnosticCenterModel::Group& group){
    if(!group.Code.empty())return Shorten(group.Title,92);
    if(group.Help>=0&&group.Help<static_cast<int>(LiveLog::Guides.size())&&group.Help!=7&&
       !group.Category.empty()&&group.Category.rfind("Subsystem:",0)!=0)
        return group.Category;
    if(group.Category=="Asset resolution"||group.Category=="Blueprint modifications"||
       group.Category=="Property modifications"||group.Category=="Import map")return group.Category;
    return Shorten(group.Title,92);
}
inline std::string TargetHint(const DiagnosticCenterModel::Group& group){
    if(!group.Property.empty())return Shorten(group.Property,72);
    if(!group.Target.empty()){
        std::string_view target(group.Target);
        const auto slash=target.find_last_of("/\\");
        if(slash!=std::string_view::npos)target.remove_prefix(slash+1);
        return Shorten(target,72);
    }
    if(!group.File.empty()){
        std::string_view file(group.File);
        const auto slash=file.find_last_of("/\\");
        if(slash!=std::string_view::npos)file.remove_prefix(slash+1);
        return Shorten(file,72);
    }
    std::string_view raw(group.Title);
    auto marker=raw.find("item=\"");
    if(marker==std::string_view::npos)marker=raw.find("target=\"");
    if(marker!=std::string_view::npos){
        marker=raw.find('"',marker);
        if(marker!=std::string_view::npos){
            ++marker;const auto stop=raw.find('"',marker);
            auto target=raw.substr(marker,stop==std::string_view::npos?raw.size()-marker:stop-marker);
            const auto slash=target.find_last_of("/\\");
            if(slash!=std::string_view::npos)target.remove_prefix(slash+1);
            if(!target.empty())return Shorten(target,72);
        }
    }
    return group.Source.empty()?Shorten(group.Title,72):group.Source;
}
inline void TextFittedAt(const ImVec2& at,std::string_view text,float maxWidth,ImU32 color){
    const auto clipped=Fitted(text,maxWidth);
    ImGui::GetWindowDrawList()->AddText(at,color,clipped.c_str());
}
inline bool BeginPaddedChild(const char* id,const HelpyImGuiTheme::Theme& theme,const ImVec2& size=ImVec2(0,0),bool border=true,ImGuiWindowFlags flags=0){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(theme.X(theme.contentPaddingX),theme.Y(theme.contentPaddingY)));
    return ImGui::BeginChild(id,size,border,flags|ImGuiWindowFlags_AlwaysUseWindowPadding);
}
inline void EndPaddedChild(){ImGui::EndChild();ImGui::PopStyleVar();}
inline bool ActionButton(const char* id,HelpyImGuiTheme::Icon icon,const char* label,const HelpyImGuiTheme::Theme& theme,float logicalWidth=0.0f){
    ImGui::PushID(id);
    std::string caption="   ";caption+=label;
    const bool clicked=HelpyImGuiTheme::SoftButton(caption.c_str(),logicalWidth>0.0f?ImVec2(theme.X(logicalWidth),theme.Y(28.0f)):ImVec2(-1,theme.Y(28.0f)));
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    HelpyImGuiTheme::DrawIcon(icon,ImVec2(a.x+theme.X(12.0f),(a.y+b.y)*0.5f),theme.Y(14.0f),theme.icon);
    ImGui::PopID();
    return clicked;
}
inline void MetaRow(const char* label,const std::string& value,const HelpyImGuiTheme::Theme& theme,const ImVec4* tint=nullptr){
    if(value.empty())return;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s",label);
    ImGui::SameLine(theme.X(112.0f));
    if(tint)ImGui::TextColored(*tint,"%s",value.c_str());
    else ImGui::TextWrapped("%s",value.c_str());
}
inline bool DrawMetricCard(std::size_t count,std::string_view label,LiveLog::Level level,const ImVec4& accent,const HelpyImGuiTheme::Theme& theme){
    const float cardHeight=theme.Y(112.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
    if(ImGui::BeginChild("##metric",ImVec2(0,cardHeight),true,ImGuiWindowFlags_NoScrollbar)){
        const auto min=ImGui::GetWindowPos();
        const auto max=ImVec2(min.x+ImGui::GetWindowSize().x,min.y+ImGui::GetWindowSize().y);
        auto* dl=ImGui::GetWindowDrawList();
        const float iconSize=theme.X(18.0f);
        const float iconY=std::max(theme.Y(18.0f),iconSize*0.5f+theme.Y(6.0f));
        DrawSeverityIcon(ImVec2(max.x-theme.X(20.0f),min.y+iconY),iconSize,level,theme);
        dl->AddRectFilled(ImVec2(min.x+theme.X(10.0f),max.y-theme.Y(12.0f)),
                          ImVec2(max.x-theme.X(10.0f),max.y-theme.Y(8.0f)),
                          ImGui::GetColorU32(accent),theme.X(theme.childRounding));
        const float contentWidth=ImGui::GetContentRegionAvail().x;
        const std::string countText=std::to_string(count);
        const float baseFontSize=ImGui::GetFontSize();
        const float targetFontSize=std::clamp(cardHeight*0.34f,baseFontSize*1.8f,baseFontSize*2.8f);
        const float fontScale=std::min(targetFontSize/baseFontSize,
            std::max(0.65f,(contentWidth-theme.X(24.0f))/std::max(1.0f,ImGui::CalcTextSize(countText.c_str()).x)));
        const ImVec2 rawCountSize=ImGui::CalcTextSize(countText.c_str());
        const ImVec2 countSize(rawCountSize.x*fontScale,rawCountSize.y*fontScale);
        const ImVec2 labelSize=ImGui::CalcTextSize(label.data(),label.data()+label.size());
        const float gap=theme.Y(6.0f);
        const float contentHeight=countSize.y+gap+labelSize.y;
        const float startY=std::max(theme.Y(14.0f),(cardHeight-contentHeight)*0.5f+theme.Y(2.0f));
        ImGui::SetCursorPosY(startY);
        ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x+
                            std::max(0.0f,(contentWidth-countSize.x)*0.5f));
        ImGui::SetWindowFontScale(fontScale);
        ImGui::TextColored(accent,"%s",countText.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPosY(startY+countSize.y+gap);
        ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x+
                            std::max(0.0f,(contentWidth-labelSize.x)*0.5f));
        ImGui::TextDisabled("%.*s",static_cast<int>(label.size()),label.data());
    }
    const bool hovered=ImGui::IsWindowHovered();
    const bool clicked=hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const auto cardMin=ImGui::GetWindowPos(),cardSize=ImGui::GetWindowSize();
    if(hovered){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);HelpyImGuiTheme::TooltipAt(cardMin,{cardMin.x+cardSize.x,cardMin.y+cardSize.y},("Browse "+std::string(label)).c_str());}
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}
inline bool ToolbarIconButton(const char* id,const char* tooltip,int direction,
                              const HelpyImGuiTheme::Theme& theme){
    ImGui::PushID(id);
    const float side=std::max(theme.Y(30.0f),ImGui::GetFrameHeight());
    const bool clicked=HelpyImGuiTheme::SoftButton("##icon",ImVec2(side,side));
    const ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    const ImVec2 c((a.x+b.x)*.5f,(a.y+b.y)*.5f);
    auto* draw=ImGui::GetWindowDrawList();const auto col=ImGui::GetColorU32(theme.icon);
    const float radius=theme.Y(5.0f),width=std::max(1.0f,theme.X(1.8f));
    if(direction==0){
        draw->AddRect(ImVec2(c.x-radius,c.y-radius),ImVec2(c.x+radius,c.y+radius),col,0,0,width);
        draw->AddLine(ImVec2(c.x-radius*.6f,c.y-radius*.6f),ImVec2(c.x-radius*1.1f,c.y-radius*1.1f),col,width);
        draw->AddLine(ImVec2(c.x+radius*.6f,c.y+radius*.6f),ImVec2(c.x+radius*1.1f,c.y+radius*1.1f),col,width);
    }else{
        const float sign=direction<0?1.0f:-1.0f;
        draw->AddLine(ImVec2(c.x+sign*radius*.65f,c.y-radius),ImVec2(c.x-sign*radius*.45f,c.y),col,width);
        draw->AddLine(ImVec2(c.x-sign*radius*.45f,c.y),ImVec2(c.x+sign*radius*.65f,c.y+radius),col,width);
    }
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("%s",tooltip);
    ImGui::PopID();return clicked;
}
inline float IssueRowHeight(const HelpyImGuiTheme::Theme& theme){
    return std::max(theme.Y(76.0f),ImGui::GetTextLineHeightWithSpacing()*3.3f);
}
inline void DrawIssueRow(DiagnosticCenterModel::Store& store,const DiagnosticCenterModel::Group& group,
                         const HelpyImGuiTheme::Theme& theme,bool selected,
                         std::size_t related=0,bool* expanded=nullptr,bool nested=false){
    ImGui::PushID(static_cast<int>(group.Id));
    const float height=IssueRowHeight(theme);
    const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const bool chosen=ImGui::Selectable("##issue-row",selected,ImGuiSelectableFlags_None,ImVec2(width,height));
    const bool hovered=ImGui::IsItemHovered();
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled(a,b,ImGui::GetColorU32(selected?theme.selection:(hovered?theme.controlHover:theme.panelAlt)),theme.X(theme.childRounding));
    dl->AddRect(a,b,ImGui::GetColorU32(selected?theme.accent:theme.border),theme.X(theme.childRounding));
    const float inset=theme.X(nested?22.0f:13.0f),space=theme.X(8.0f);
    const float toggleWidth=related?theme.X(52.0f):0.0f;
    const ImVec2 clipMin(a.x+inset,a.y+theme.Y(6.0f)),clipMax(b.x-inset-toggleWidth,b.y-theme.Y(6.0f));
    ImGui::PushClipRect(clipMin,clipMax,true);
    const float titleX=clipMin.x+theme.X(7.0f);
    const float titleY=clipMin.y+theme.Y(2.0f);
    dl->AddRectFilled(ImVec2(clipMin.x,a.y+theme.Y(12.0f)),ImVec2(clipMin.x+theme.X(3.0f),b.y-theme.Y(12.0f)),
                      ImGui::GetColorU32(SeverityColor(group.Severity,theme)),theme.X(2.0f));
    const float iconSize=theme.X(15.0f);
    const ImVec2 severityIconCenter(titleX+space+iconSize*0.5f,titleY+ImGui::GetTextLineHeight()*0.5f);
    DrawSeverityIcon(severityIconCenter,iconSize,group.Severity,theme);
    const float textX=severityIconCenter.x+iconSize*0.5f+theme.X(7.0f);
    const auto name=IssueTitle(group);
    TextFittedAt(ImVec2(textX,titleY),name,clipMax.x-textX,ImGui::GetColorU32(theme.text));
    const auto hint=TargetHint(group);
    const auto rest=hint+"  ·  "+std::to_string(group.Count)+" report"+(group.Count==1?"":"s")+
        (group.Acknowledged?"  ·  ACKNOWLEDGED":(group.Viewed?"  ·  VIEWED":"  ·  NEW"));
    const float subY=titleY+ImGui::GetTextLineHeight()+theme.Y(4.0f);
    float subX=textX;
    const std::string legacyReference=group.Code.empty() && group.Help>=0 &&
        group.Help<static_cast<int>(LiveLog::Guides.size()) &&
        LiveLog::Guides[group.Help].Code!="RS-LOG-000" &&
        LiveLog::Guides[group.Help].Code!="RS-GAME-000"
            ? "~"+std::string(LiveLog::Guides[group.Help].Code) : "";
    const std::string diagnosticLabel=!group.Code.empty()?group.Code:
        !legacyReference.empty()?legacyReference:
        (group.Severity==LiveLog::Level::Error?"LEGACY ERR":
         group.Severity==LiveLog::Level::Warning?"LEGACY WARN":
         group.Severity==LiveLog::Level::Diagnostic?"LEGACY DIAG":"LEGACY INFO");
    if(!diagnosticLabel.empty()){
        const float codeMax=clipMax.x-subX;
        TextFittedAt(ImVec2(subX,subY),diagnosticLabel,codeMax,ImGui::GetColorU32(SeverityColor(group.Severity,theme)));
        subX+=std::min(ImGui::CalcTextSize(diagnosticLabel.c_str()).x,codeMax)+theme.X(8.0f);
        if(subX<clipMax.x-theme.X(10.0f)){
            dl->AddText(ImVec2(subX,subY),ImGui::GetColorU32(theme.textDisabled),"·");
            subX+=ImGui::CalcTextSize("·").x+theme.X(8.0f);
        }
    }
    if(subX<clipMax.x)TextFittedAt(ImVec2(subX,subY),rest,clipMax.x-subX,ImGui::GetColorU32(theme.textDisabled));
    ImGui::PopClipRect();
    if(related){
        const std::string toggle=(expanded&&*expanded?"- ":"+ ")+std::to_string(related);
        const auto toggleSize=ImGui::CalcTextSize(toggle.c_str());
        dl->AddText(ImVec2(b.x-theme.X(12.0f)-toggleSize.x,a.y+(height-toggleSize.y)*0.5f),
                    ImGui::GetColorU32(theme.accent),toggle.c_str());
    }
    if(hovered)HelpyImGuiTheme::Tooltip("%s\n%s\n%llu occurrence%s%s",group.Title.c_str(),group.Source.c_str(),
                    static_cast<unsigned long long>(group.Count),group.Count==1?"":"s",
                    related?"\nClick + / - at right to show related reports":"");
    if(chosen){
        if(related&&expanded&&ImGui::GetIO().MousePos.x>=b.x-toggleWidth)*expanded=!*expanded;
        else Choose(store,group.Id);
    }
    ImGui::PopID();
}
inline void DrawActivityRow(DiagnosticCenterModel::Store& store,const DiagnosticCenterModel::Event& event,
                            const HelpyImGuiTheme::Theme& theme){
    ImGui::PushID(static_cast<int>(event.Id));
    const auto title=std::string(LiveLog::Label(event.Severity))+" · "+std::string(DiagnosticToastPatterns::Body(event.Text));
    const float height=std::max(theme.Y(30.0f),ImGui::GetTextLineHeightWithSpacing()+theme.Y(8));
    const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const bool clicked=ImGui::Selectable("##activity-row",false,0,ImVec2(width,height));
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    auto* dl=ImGui::GetWindowDrawList();
    const ImVec4 bg=ImGui::IsItemHovered()?theme.controlHover:theme.panelAlt;
    dl->AddRectFilled(a,b,ImGui::GetColorU32(bg),theme.X(theme.childRounding));
    dl->AddRectFilled(ImVec2(a.x+theme.X(5),a.y+theme.Y(5)),ImVec2(a.x+theme.X(8),b.y-theme.Y(5)),
                      ImGui::GetColorU32(SeverityColor(event.Severity,theme)));
    const float left=a.x+theme.X(14),right=b.x-theme.X(8);
    ImGui::PushClipRect(ImVec2(left,a.y),ImVec2(right,b.y),true);
    const auto time=TimeLabel(event.Timestamp);
    const float timeWidth=ImGui::CalcTextSize(time.c_str()).x+theme.X(12);
    TextFittedAt(ImVec2(left,a.y+theme.Y(5)),time,right-left,ImGui::GetColorU32(theme.textDisabled));
    if(right-left>timeWidth+theme.X(32))
        TextFittedAt(ImVec2(left+timeWidth,a.y+theme.Y(5)),title,right-left-timeWidth,ImGui::GetColorU32(theme.text));
    ImGui::PopClipRect();
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("%s",event.Text.c_str());
    if(clicked)Choose(store,event.GroupId);
    ImGui::PopID();
}
inline void DrawOverview(DiagnosticCenterModel::Store& store,const HelpyImGuiTheme::Theme& theme){
    const auto& groups=store.Groups();
    std::size_t errors=0,warnings=0,other=0;
    for(const auto& group:groups)switch(group.Severity){
        case LiveLog::Level::Error:++errors;break;
        case LiveLog::Level::Warning:++warnings;break;
        default:++other;break;
    }
    ImGui::TextDisabled("%s  |  %zu retained events  |  %llu earlier records dropped",
        Historical?"Historical session (read-only)":"Current session",
        store.Events().size(),static_cast<unsigned long long>(store.Dropped()));
    if(ImGui::SmallButton("Export selected session..."))RequestReport(store,ExportKind::Session);
    if(theme.windowOpacity<0.86f){
        ImGui::TextColored(theme.warning,"Window opacity is %.0f%%. Increase it in Settings > Appearance for better contrast.",
                           theme.windowOpacity*100.0f);
    }
    ImGui::Spacing();
    const int metricColumns=ImGui::GetContentRegionAvail().x<theme.X(600.0f)?1:3;
    if(ImGui::BeginTable("##diagnostic-counts",metricColumns,ImGuiTableFlags_SizingStretchSame)){
        const std::size_t counts[]{errors,warnings,other};
        const char* labels[]{"Error groups","Warning groups","Other groups"};
        const LiveLog::Level levels[]{LiveLog::Level::Error,LiveLog::Level::Warning,LiveLog::Level::Info};
        const ImVec4* colors[]{&theme.error,&theme.warning,&theme.info};
        for(int i=0;i<3;++i){
            ImGui::TableNextColumn();ImGui::PushID(i);
            if(DrawMetricCard(counts[i],labels[i],levels[i],*colors[i],theme)){
                SeverityScope=i+1;ShowAll=i==2;OnlyNew=false;Query[0]='\0';
                Current=View::Issues;RequestedTab=1;DetailOnly=false;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Dummy({0,theme.Y(24.0f)});ImGui::SeparatorText("Needs attention");
    ImGui::TextDisabled("Unviewed errors first. Select an issue for details and suggested checks.");
    std::vector<const DiagnosticCenterModel::Group*> priority;
    for(const auto& group:groups)if(DiagnosticCenterModel::Actionable(group.Severity))priority.push_back(&group);
    const auto count=std::min<std::size_t>(priority.size(),5);
    std::partial_sort(priority.begin(),priority.begin()+count,priority.end(),[](const auto* a,const auto* b){
        if(a->Viewed!=b->Viewed)return !a->Viewed;
        if(a->Severity!=b->Severity)return a->Severity==LiveLog::Level::Error;
        return a->LastSeen>b->LastSeen;
    });
    if(priority.empty()){
        ImGui::TextDisabled("No errors or warnings reported in the available session history.");
        ImGui::TextDisabled("Diagnostic reporting is not a complete system health check.");
    }
    for(std::size_t i=0;i<count;++i){
        DrawIssueRow(store,*priority[i],theme,false);
        ImGui::Dummy(ImVec2(0,theme.Y(6.0f)));
    }
    if(priority.size()>count){
        ImGui::TextDisabled("%zu more error/warning groups",priority.size()-count);
        ImGui::SameLine();if(ImGui::SmallButton("Browse all##issues")){Current=View::Issues;RequestedTab=1;}
    }
    ImGui::Spacing();ImGui::SeparatorText("Recent activity");
    ImGui::TextDisabled("Latest retained events, including informational activity and failures.");
    if(!Historical){
        const auto& source=LiveLogPanel::Streams[0];
        if(!source.ReadError.empty())ImGui::TextColored(theme.warning,"Log reader: %s",source.ReadError.c_str());
        else ImGui::TextDisabled("RuneSchema.log: %zu buffered lines",source.History.Rows.size());
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Source: %s",LiveLogPanel::Path().string().c_str());
    }
    const auto& events=store.Events();
    if(events.empty())ImGui::TextDisabled(Historical?"No saved evidence for this session.":"Waiting for new RuneSchema log activity.");
    int shown=0;
    for(auto it=events.rbegin();it!=events.rend()&&shown<7;++it,++shown)DrawActivityRow(store,*it,theme);
    if(events.size()>static_cast<std::size_t>(shown)){
        if(ImGui::SmallButton("View full Live Log")){Current=View::LiveLog;RequestedTab=2;}
    }
}
struct InspectorPopupPlacement { ImVec2 Position; float Width; };
inline InspectorPopupPlacement PlaceInspectorActions(const ImVec2& buttonMin,const ImVec2& buttonMax,
                                                       const ImVec2& workPos,const ImVec2& workSize,
                                                       const HelpyImGuiTheme::Theme& theme){
    const float margin=std::min(theme.X(10.0f),std::max(0.0f,workSize.x*0.04f));
    const float width=std::min(theme.X(258.0f),std::max(1.0f,workSize.x-2.0f*margin));
    const float left=workPos.x+margin,right=workPos.x+workSize.x-margin-width;
    const float x=std::clamp(buttonMax.x-width,left,std::max(left,right));
    const float gap=theme.Y(5.0f);
    const float menuHeight=std::min(theme.Y(245.0f),std::max(1.0f,workSize.y-theme.Y(12.0f)));
    const float top=workPos.y+theme.Y(6.0f);
    const float bottom=workPos.y+workSize.y-theme.Y(6.0f);
    float y=buttonMax.y+gap;
    if(y+menuHeight>bottom)y=buttonMin.y-menuHeight-gap;
    y=std::clamp(y,top,std::max(top,bottom-menuHeight));
    return {{x,y},width};
}
inline bool InspectorMenuItem(const char* id,const char* title,HelpyImGuiTheme::Icon icon,
                              const HelpyImGuiTheme::Theme& theme,bool enabled=true){
    ImGui::PushID(id);
    std::string caption="       ";caption+=title;
    const bool clicked=ImGui::MenuItem(caption.c_str(),nullptr,false,enabled);
    const ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    HelpyImGuiTheme::DrawIcon(icon,ImVec2(a.x+theme.X(15.0f),(a.y+b.y)*0.5f),
                             theme.X(15.0f),enabled?theme.icon:theme.textDisabled);
    ImGui::PopID();
    return clicked;
}
inline void DrawInspectorWindowHeader(DiagnosticCenterModel::Store& store,
                                       const DiagnosticCenterModel::Group& group,
                                       const HelpyImGuiTheme::Theme& theme){
    const float buttonSide=std::max(theme.X(28.0f),ImGui::GetFrameHeight());
    const float headerHeight=std::max(theme.Y(42.0f),buttonSide+theme.Y(10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    if(ImGui::BeginChild("##issue-inspector-window-header",ImVec2(0,headerHeight),true,
                         ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){
        const ImVec2 a=ImGui::GetWindowPos(),windowSize=ImGui::GetWindowSize();
        const float iconSize=theme.X(15.0f);
        const float inset=theme.X(10.0f);
        const float iconX=a.x+inset+iconSize*0.5f;
        const float centerY=a.y+windowSize.y*0.5f;
        DrawSeverityIcon(ImVec2(iconX,centerY),iconSize,group.Severity,theme);
        const float textX=iconX+iconSize*0.5f+theme.X(9.0f);
        const float buttonX=a.x+windowSize.x-inset-buttonSide;
        const auto heading=Fitted("Issue Inspector",std::max(1.0f,buttonX-textX-theme.X(8.0f)));
        ImGui::GetWindowDrawList()->AddText(ImVec2(textX,centerY-ImGui::GetFontSize()*0.5f),
                                           ImGui::GetColorU32(theme.text),heading.c_str());
        ImGui::SetCursorScreenPos(ImVec2(buttonX,centerY-buttonSide*0.5f));
        ImGui::PushID("issue-actions-header");
        if(HelpyImGuiTheme::SoftButton("##actions-dropdown",ImVec2(buttonSide,buttonSide)))
            ImGui::OpenPopup("##inspector-actions-menu");
        const ImVec2 buttonA=ImGui::GetItemRectMin(),buttonB=ImGui::GetItemRectMax();
        const ImVec2 center((buttonA.x+buttonB.x)*0.5f,(buttonA.y+buttonB.y)*0.5f);
        auto* draw=ImGui::GetWindowDrawList();
        const ImU32 dotColor=ImGui::GetColorU32(theme.icon);
        const float dotRadius=std::max(1.0f,theme.X(1.55f));
        for(int i=-1;i<=1;++i)
            draw->AddCircleFilled(ImVec2(center.x+static_cast<float>(i)*theme.X(6.0f),center.y),dotRadius,dotColor,10);
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Issue actions");
        const auto* viewport=ImGui::GetMainViewport();
        const auto placement=PlaceInspectorActions(buttonA,buttonB,viewport->WorkPos,viewport->WorkSize,theme);
        ImGui::SetNextWindowPos(placement.Position,ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(placement.Width,0.0f),
            ImVec2(placement.Width,std::max(1.0f,viewport->WorkSize.y-theme.Y(20.0f))));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(theme.X(12.0f),theme.Y(9.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(theme.X(7.0f),theme.Y(6.0f)));
        bool requestMuteDialog=false;
        if(ImGui::BeginPopup("##inspector-actions-menu")){
            ImGui::TextColored(theme.text,"Issue actions");
            ImGui::Separator();
            if(InspectorMenuItem("ack","Acknowledge",HelpyImGuiTheme::Icon::Acknowledge,theme,!Historical&&!group.Acknowledged))
                store.MarkViewed(group.Id,true);
            if(InspectorMenuItem("copy","Copy summary",HelpyImGuiTheme::Icon::Copy,theme)){
                DiagnosticCenterReport::Options options;options.IncludeEvidence=false;
                const auto summary=DiagnosticCenterReport::Issue(store,group,
                    Historical?"Previous session: "+ArchivedSessionId:"Current session",options);
                ImGui::SetClipboardText(summary.c_str());
            }
            if(InspectorMenuItem("export","Export issue",HelpyImGuiTheme::Icon::Export,theme))
                RequestReport(store,ExportKind::Issue,group.Id);
            ImGui::Separator();
            if(InspectorMenuItem("mute","Mute notifications...",HelpyImGuiTheme::Icon::Bell,theme,MuteManager!=nullptr)){
                PendingMuteScope=DiagnosticCenterMutes::Scope::Exact;
                PendingMuteDuration=DiagnosticCenterMutes::Duration::Session;
                requestMuteDialog=true;
            }
            if(InspectorMenuItem("evidence","Inspect evidence",HelpyImGuiTheme::Icon::LiveLog,theme,!group.EvidenceIds.empty())){
                SelectedOccurrenceEvent=group.EvidenceIds.back();OpenOccurrences=true;
            }
            if(InspectorMenuItem("raw","Raw details",HelpyImGuiTheme::Icon::Code,theme))OpenTechnical=true;
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        if(requestMuteDialog)ImGui::OpenPopup("Mute notifications##inspector-actions");
        const float modalW=std::min(theme.X(430.0f),std::max(1.0f,viewport->WorkSize.x-theme.X(24.0f)));
        ImGui::SetNextWindowSizeConstraints(ImVec2(modalW,0.0f),ImVec2(modalW,std::numeric_limits<float>::max()));
        if(HelpyImGuiTheme::BeginCenteredModal("Mute notifications##inspector-actions",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped("Mute pop-up notifications. Reports remain in Diagnostics and the original logs.");
            ImGui::SeparatorText("Scope");
            if(ImGui::RadioButton("This issue only",PendingMuteScope==DiagnosticCenterMutes::Scope::Exact))
                PendingMuteScope=DiagnosticCenterMutes::Scope::Exact;
            if(ImGui::RadioButton("All issues in this category",PendingMuteScope==DiagnosticCenterMutes::Scope::Category))
                PendingMuteScope=DiagnosticCenterMutes::Scope::Category;
            ImGui::SeparatorText("Duration");
            if(ImGui::RadioButton("This session",PendingMuteDuration==DiagnosticCenterMutes::Duration::Session))
                PendingMuteDuration=DiagnosticCenterMutes::Duration::Session;
            if(ImGui::RadioButton("Until manually unmuted",PendingMuteDuration==DiagnosticCenterMutes::Duration::Persistent))
                PendingMuteDuration=DiagnosticCenterMutes::Duration::Persistent;
            if(PendingMuteScope==DiagnosticCenterMutes::Scope::Category){
                ImGui::Spacing();
                ImGui::TextColored(theme.warning,"Applies to other mods in '%s' too.",group.Category.c_str());
            }
            ImGui::Separator();
            if(ImGui::Button("Apply mute")){
                if(MuteManager)MuteManager->Add(group,PendingMuteScope,PendingMuteDuration);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f,theme.Y(4.0f)));
}
inline void DrawInspector(DiagnosticCenterModel::Store& store,const HelpyImGuiTheme::Theme& theme){
    const auto* group=store.FindGroup(SelectedGroup);
    if(!group){ImGui::TextDisabled("Select an issue to inspect its reports and troubleshooting information.");return;}
    if(LastInspected!=SelectedGroup){LastInspected=SelectedGroup;OpenOccurrences=false;OpenTechnical=false;OpenRelated=false;SelectedOccurrenceEvent=0;}
    DrawInspectorWindowHeader(store,*group,theme);
    const float detailHeight=std::max(1.0f,ImGui::GetContentRegionAvail().y);
    if(ImGui::BeginChild("##issue-inspector-scrollable-details",ImVec2(0,detailHeight),false)){

    ImGui::TextColored(SeverityColor(group->Severity,theme),"%s",IssueTitle(*group).c_str());
    ImGui::TextDisabled("%s  ·  %llu occurrence%s",LiveLog::Label(group->Severity),
                       static_cast<unsigned long long>(group->Count),group->Count==1?"":"s");
    if(!group->Code.empty())MetaRow(group->Key.starts_with("structured:")?"Code (structured)":"Code (legacy log)",
                                      group->Code,theme,&SeverityColor(group->Severity,theme));
    else if(group->Help>=0 && group->Help<static_cast<int>(LiveLog::Guides.size()) &&
            LiveLog::Guides[group->Help].Code!="RS-LOG-000" &&
            LiveLog::Guides[group->Help].Code!="RS-GAME-000"){
        const auto label="~"+std::string(LiveLog::Guides[group->Help].Code);
        MetaRow("Suggested reference",label,theme,&SeverityColor(group->Severity,theme));
        ImGui::TextDisabled("Legacy log only; this code was not emitted by RuneSchema.");
    }else ImGui::TextDisabled("Uncoded legacy %s; original message retained.",LiveLog::Label(group->Severity));
    MetaRow("Source",group->Source,theme);
    MetaRow("First / latest",TimeLabel(group->FirstSeen)+"  ->  "+TimeLabel(group->LastSeen),theme);

    ImGui::SeparatorText("What happened");
    ImGui::TextWrapped("%s",group->Message.empty()?group->Title.c_str():group->Message.c_str());
    if(!group->Outcome.empty())MetaRow("Outcome",group->Outcome,theme,&SeverityColor(group->Severity,theme));

    if(!group->ModId.empty()||!group->File.empty()||!group->Target.empty()||!group->Property.empty()){
        ImGui::SeparatorText("Affected definition");
        MetaRow("Mod",group->ModId,theme);
        MetaRow("File",group->File,theme);
        MetaRow("Target",group->Target,theme);
        MetaRow("Property",group->Property,theme);
    }

    ImGui::SeparatorText("Suggested checks");
    if(group->Help>=0 && group->Help<static_cast<int>(LiveLog::Guides.size())){
        const auto& guide=LiveLog::Guides[group->Help];
        ImGui::TextWrapped("%s",guide.Title.data());
        if(ActionButton("open-guide",HelpyImGuiTheme::Icon::Info,guide.Code.data(),theme,210.0f)){
#ifdef _WIN32
            const auto url=LiveLog::HelpUrl(guide);
            ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
        }
    }else if(const auto check=StructuredDiagnosticAdvice::Find(group->Code);!check.empty()){
        ImGui::TextWrapped("%.*s",static_cast<int>(check.size()),check.data());
    }else if(const auto* known=StructuredDiagnosticRegistry::Find(group->Code)){
        ImGui::TextWrapped("Registered condition: %.*s",static_cast<int>(known->Description.size()),known->Description.data());
        ImGui::TextDisabled("No code-specific repair procedure has been verified. Check the original evidence and affected definition.");
    }else ImGui::TextWrapped("No code-specific check has been verified for this issue. Review the retained evidence and any named mod, file, target or property before making changes.");

    ImGui::Spacing();

    if(!group->CauseDescription.empty()){
        if(ImGui::CollapsingHeader("Additional context")){
            const char* qualifier=group->CauseStatus=="Confirmed"?"Confirmed by reporter":
                group->CauseStatus=="Suspected"?"Suspected (not proven)":"Unknown / unverified";
            ImGui::TextDisabled("Certainty: %s",qualifier);
            ImGui::TextWrapped("%s",group->CauseDescription.c_str());
        }
    }

    if(OpenOccurrences)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
    const bool evidenceExpanded=ImGui::CollapsingHeader("Occurrences and retained evidence");
    OpenOccurrences=false;
    if(evidenceExpanded){
        const auto& ids=group->EvidenceIds;
        ImGui::TextDisabled("%zu retained sample%s / %llu total reported occurrence%s (bounded history).",
            ids.size(),ids.size()==1?"":"s",static_cast<unsigned long long>(group->Count),group->Count==1?"":"s");
        if(group->Count>ids.size())ImGui::TextDisabled("Earlier reports or aggregated repeats may not have individual evidence samples.");
        if(ids.empty())ImGui::TextDisabled("No original evidence remains in the retained session buffer.");
        else{
            const auto chosen=DiagnosticHistoryNavigation::SelectedOccurrence(ids,SelectedOccurrenceEvent);
            const auto older=DiagnosticHistoryNavigation::AdjacentOccurrence(ids,chosen,1);
            const auto newer=DiagnosticHistoryNavigation::AdjacentOccurrence(ids,chosen,-1);
            const auto it=std::find(ids.begin(),ids.end(),chosen);
            const auto index=static_cast<std::size_t>(it-ids.begin());
            ImGui::TextDisabled("Sample %zu of %zu (newest is %zu)",index+1,ids.size(),ids.size());
            ImGui::BeginDisabled(older==0);
            if(ImGui::SmallButton("Older sample"))SelectedOccurrenceEvent=older;
            ImGui::EndDisabled();
            ImGui::SameLine();ImGui::BeginDisabled(newer==0);
            if(ImGui::SmallButton("Newer sample"))SelectedOccurrenceEvent=newer;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if(ImGui::SmallButton("Latest"))SelectedOccurrenceEvent=ids.back();
            const auto selected=DiagnosticHistoryNavigation::SelectedOccurrence(ids,SelectedOccurrenceEvent);
            if(const auto* event=store.FindEvent(selected)){
                ImGui::Separator();
                ImGui::TextDisabled("Retained at: %s",TimeLabel(event->Timestamp).c_str());
                if(Historical)ImGui::TextDisabled("Historical snapshot only; it does not link to the current RuneSchema.log.");
                else if(!event->StructuredJson.empty())ImGui::TextDisabled("Structured JSONL event; no corresponding row was written to RuneSchema.log.");
                else if(event->SourceLogId)ImGui::TextDisabled("Legacy entry: the source row may have rotated out of Live Log.");
                else ImGui::TextDisabled("Legacy entry retained without a source-row reference.");
                ImGui::TextWrapped("%s",event->Text.c_str());
                if(ImGui::SmallButton("Copy retained text"))ImGui::SetClipboardText(event->Text.c_str());
                if(!event->StructuredJson.empty()){
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Copy structured JSON"))ImGui::SetClipboardText(event->StructuredJson.c_str());
                }
                if(Historical){
                    if(ImGui::SmallButton("Show in retained session log")){
                        SelectedHistoricalEvent=event->Id;RevealHistoricalEvent=true;Current=View::LiveLog;RequestedTab=2;
                    }
                }else if(event->StructuredJson.empty()&&event->SourceLogId){
                    if(ImGui::SmallButton("Find original RuneSchema log row")){
                        if(LiveLogPanel::Reveal(event->SourceLogId)){
                            Current=View::LiveLog;RequestedTab=2;
                        }else PendingExportStatus="Original RuneSchema.log row unavailable; the retained copy is still shown here.";
                    }
                }
            }else ImGui::TextDisabled("The selected sample is no longer in the bounded event buffer.");
        }
    }
    if(ImGui::CollapsingHeader("Same operation (confirmed identifiers)")){
        const auto related=DiagnosticIssuePresentation::Related(store,group->Id);
        if(related.empty())ImGui::TextDisabled("No other retained structured reports share a verified operation ID and mod.");
        else{
            ImGui::TextDisabled("Linked by the exact backend OperationId and ModId, not by timestamp.");
            for(const auto id:related){
                if(const auto* peer=store.FindGroup(id)){
                    ImGui::PushID(static_cast<int>(peer->Id));
                    ImGui::TextColored(SeverityColor(peer->Severity,theme),"%s",LiveLog::Label(peer->Severity));
                    ImGui::SameLine();ImGui::TextColored(SeverityColor(peer->Severity,theme),"%s",peer->Code.c_str());ImGui::SameLine();ImGui::TextWrapped("%s",peer->Title.c_str());
                    if(ImGui::SmallButton("Inspect linked report"))Choose(store,peer->Id);
                    ImGui::PopID();
                }
            }
        }
        ImGui::TextDisabled("Independent legacy and save-cleanup safety reports remain separate.");
    }
    if(ImGui::CollapsingHeader("Nearby reports (not confirmed related)")){
        ImGui::TextDisabled("Same source within 10 seconds of the latest occurrence. Time proximity does not prove causation.");
        std::size_t shown=0;
        for(auto it=store.Events().rbegin();it!=store.Events().rend()&&shown<12;++it){
            if(it->GroupId==group->Id||it->Source!=group->Source)continue;
            const auto difference=it->Timestamp>group->LastSeen?it->Timestamp-group->LastSeen:group->LastSeen-it->Timestamp;
            if(difference>std::chrono::seconds(10))continue;
            ImGui::PushID(static_cast<int>(it->Id));
            ImGui::TextWrapped("%s - %s",TimeLabel(it->Timestamp).c_str(),Shorten(it->Text,240).c_str());
            if(ImGui::SmallButton("Inspect nearby issue"))Choose(store,it->GroupId);
            ImGui::PopID();++shown;
        }
        if(!shown)ImGui::TextDisabled("No nearby retained reports from this source.");
    }
    if(OpenTechnical)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
    else if(theme.diagnosticDeveloperMode)ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    OpenTechnical=false;
    if(ImGui::CollapsingHeader("Technical details")){
        ImGui::TextDisabled("Issue reference: %llu",static_cast<unsigned long long>(group->Id));
        ImGui::TextDisabled("Severity: %s",LiveLog::Label(group->Severity));
        ImGui::TextDisabled("Category: %s",group->Category.c_str());
        if(!group->Code.empty())ImGui::TextColored(SeverityColor(group->Severity,theme),"Code: %s",group->Code.c_str());
        if(!group->Phase.empty())ImGui::TextDisabled("Phase: %s",group->Phase.c_str());
        if(!group->Outcome.empty())ImGui::TextDisabled("Outcome: %s",group->Outcome.c_str());
        if(!group->ModId.empty()&&ImGui::SmallButton("Copy mod ID"))ImGui::SetClipboardText(group->ModId.c_str());
        if(!group->File.empty()){if(!group->ModId.empty())ImGui::SameLine();if(ImGui::SmallButton("Copy file path"))ImGui::SetClipboardText(group->File.c_str());}
        if(!group->Target.empty()){if(!group->ModId.empty()||!group->File.empty())ImGui::SameLine();if(ImGui::SmallButton("Copy target"))ImGui::SetClipboardText(group->Target.c_str());}
        if(!group->Property.empty()){if(!group->ModId.empty()||!group->File.empty()||!group->Target.empty())ImGui::SameLine();if(ImGui::SmallButton("Copy property"))ImGui::SetClipboardText(group->Property.c_str());}
        if(!group->EvidenceIds.empty()){
            if(const auto* latest=store.FindEvent(group->EvidenceIds.back());latest&&!latest->OperationId.empty()){
                ImGui::TextWrapped("Latest operation ID: %s",latest->OperationId.c_str());
                if(ImGui::SmallButton("Copy operation ID"))ImGui::SetClipboardText(latest->OperationId.c_str());
            }
        }
        if(!group->CauseStatus.empty())ImGui::TextDisabled("Reported cause status: %s",group->CauseStatus.c_str());
        ImGui::TextDisabled("Evidence older than the bounded history is unavailable from this viewer.");
    }
    }
    ImGui::EndChild();
}
inline void DrawIssues(DiagnosticCenterModel::Store& store,const HelpyImGuiTheme::Theme& theme){
    const float availableBefore=ImGui::GetContentRegionAvail().x;
    const bool narrow=availableBefore<std::max(theme.X(745.0f),ImGui::GetFontSize()*54.0f);
    const float clearWidth=ImGui::CalcTextSize("Clear").x+theme.X(35.0f);
    const float queryWidth=std::max(theme.X(85.0f),availableBefore-clearWidth);
    ImGui::SetNextItemWidth(queryWidth);
    ImGui::InputTextWithHint("##diagnostics-search","Search code, mod, file, target, property, message...",Query,sizeof(Query));
    ImGui::SameLine();
    if(ImGui::SmallButton("Clear##diagnostics-search"))Query[0]='\0';
    const char* scopes[]={"Errors + warnings","Errors only","Warnings only",
        "Other events","Show all (all severities)","New only (errors + warnings)",
        "New errors only","New warnings only"};
    int filterPreset=OnlyNew?(SeverityScope==1?6:SeverityScope==2?7:5):
        (ShowAll&&SeverityScope==0?4:SeverityScope);
    ImGui::SetNextItemWidth(std::min(theme.X(285.0f),std::max(theme.X(110.0f),ImGui::GetContentRegionAvail().x)));
    if(ImGui::Combo("Filter##issues",&filterPreset,scopes,8)){
        OnlyNew=filterPreset>=5;
        ShowAll=filterPreset==3||filterPreset==4;
        SeverityScope=filterPreset==3?3:filterPreset==6?1:filterPreset==7?2:
            filterPreset>=4?0:filterPreset;
    }
    if(narrow){
        ImGui::SetNextItemWidth(std::min(theme.X(220.0f),ImGui::GetContentRegionAvail().x));
        ImGui::Combo("Sort##diagnostic-sort",&SortMode,"Newest\0Oldest\0Severity\0Most frequent\0");
    }else{
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(theme.X(185.0f),ImGui::GetContentRegionAvail().x));
        ImGui::Combo("Sort##diagnostic-sort",&SortMode,"Newest\0Oldest\0Severity\0Most frequent\0");
    }
    if(ImGui::SmallButton("Reset filters"))ResetIssueFilters();
    if(!narrow)ImGui::SameLine();
    ImGui::Checkbox("Group same-operation",&GroupByOperation);
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Only reports sharing explicit OperationId AND ModId. Legacy safety warnings remain separate.");

    std::vector<const DiagnosticCenterModel::Group*> filtered;
    for(const auto& group:store.Groups())if(Matches(group,Query))filtered.push_back(&group);
    DiagnosticIssueFilters::Sort(filtered,SortMode);
    if(RevealSelectedInList&&GroupByOperation&&DiagnosticIssueNavigation::Contains(filtered,SelectedGroup)){
        const auto parent=DiagnosticIssueNavigation::ParentFor(store,filtered,SelectedGroup);
        if(parent&&parent!=SelectedGroup)ExpandedOperations.insert(parent);
    }
    const auto display=DiagnosticIssuePresentation::Rows(store,filtered,GroupByOperation,ExpandedOperations);
    ImGui::TextDisabled("%zu matching groups / %zu visible rows  |  %zu total groups%s",
        filtered.size(),display.size(),store.Groups().size(),OnlyNew?"  |  New only":"");
    const auto previous=DiagnosticIssueNavigation::Adjacent(filtered,SelectedGroup,-1);
    const auto next=DiagnosticIssueNavigation::Adjacent(filtered,SelectedGroup,1);
    const auto drawNavigation=[&]{
        ImGui::BeginDisabled(previous==0);
        if(ToolbarIconButton("previous","Previous issue",-1,theme))Choose(store,previous);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(next==0);
        if(ToolbarIconButton("next","Next issue",1,theme))Choose(store,next);
        ImGui::EndDisabled();
    };
    const auto drawRow=[&](const DiagnosticIssuePresentation::Row& row){
        bool expanded=ExpandedOperations.contains(row.Issue->Id);
        DrawIssueRow(store,*row.Issue,theme,SelectedGroup==row.Issue->Id,
                     row.Related,row.Related?&expanded:nullptr,row.Child);
        ImGui::Dummy(ImVec2(0,theme.Y(6.0f)));
        if(expanded)ExpandedOperations.insert(row.Issue->Id);
        else ExpandedOperations.erase(row.Issue->Id);
    };
    if(filtered.empty())ImGui::TextDisabled("No issues match these filters. Use Reset filters to show all reported errors and warnings.");
    const auto selectedIndex=DiagnosticIssueNavigation::SelectedRowIndex(display,SelectedGroup);
    const float rowHeight=IssueRowHeight(theme)+theme.Y(6.0f)+ImGui::GetStyle().ItemSpacing.y*2;
    if(narrow){
        if(DetailOnly&&store.FindGroup(SelectedGroup)){
            if(ImGui::SmallButton("Back to issues"))DetailOnly=false;
            ImGui::SameLine();drawNavigation();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
            if(BeginPaddedChild("##diagnostic-narrow-inspector",theme,ImVec2(0,0),true))DrawInspector(store,theme);
            EndPaddedChild();ImGui::PopStyleColor();
        }else{
            ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);
            if(BeginPaddedChild("##diagnostic-narrow-list",theme,ImVec2(0,0),true)){
                if(RevealSelectedInList&&selectedIndex<display.size()){
                    ImGui::SetScrollY(static_cast<float>(selectedIndex)*rowHeight);
                    RevealSelectedInList=false;
                }
                for(const auto& row:display)drawRow(row);
            }EndPaddedChild();ImGui::PopStyleColor();
        }
        return;
    }
    if(ToolbarIconButton("maximize",Maximized?"Restore split view":"Maximize inspector",0,theme))Maximized=!Maximized;
    ImGui::SameLine();drawNavigation();
    if(Maximized){
        ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
        if(BeginPaddedChild("##diagnostic-maximized",theme,ImVec2(0,0),true))DrawInspector(store,theme);
        EndPaddedChild();ImGui::PopStyleColor();return;
    }
    const float available=ImGui::GetContentRegionAvail().x;
    const float splitter=theme.X(7);
    const float minPane=std::max(theme.X(240),ImGui::GetFontSize()*19.0f);
    const float usable=std::max(0.0f,available-splitter-ImGui::GetStyle().ItemSpacing.x*2);
    const float lower=usable>minPane*2?minPane/usable:.33f;
    SplitRatio=std::clamp(SplitRatio,lower,1.0f-lower);
    const float paneHeight=std::max(theme.Y(75),ImGui::GetContentRegionAvail().y);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);
    if(BeginPaddedChild("##diagnostic-list",theme,ImVec2(usable*SplitRatio,paneHeight),true)){
        if(filtered.empty())ImGui::TextDisabled("No matching reports.");
        if(RevealSelectedInList&&selectedIndex<display.size()){
            ImGui::SetScrollY(static_cast<float>(selectedIndex)*rowHeight);
            RevealSelectedInList=false;
        }
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(display.size()),rowHeight);
        while(clipper.Step())for(int index=clipper.DisplayStart;index<clipper.DisplayEnd;++index)
            drawRow(display[static_cast<std::size_t>(index)]);
    }EndPaddedChild();ImGui::PopStyleColor();
    ImGui::SameLine(0,0);
    ImGui::InvisibleButton("##diagnostic-divider",ImVec2(splitter,paneHeight));
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
        SplitRatio=std::clamp(SplitRatio+ImGui::GetIO().MouseDelta.x/std::max(1.0f,usable),lower,1.0f-lower);
        HelpyImGuiTheme::Current.diagnosticSplitRatio=SplitRatio;
    }
    if(ImGui::IsItemDeactivatedAfterEdit())HelpyImGuiTheme::SaveCurrent();
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SameLine(0,0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
    if(BeginPaddedChild("##diagnostic-inspector",theme,ImVec2(0,paneHeight),true))DrawInspector(store,theme);
    EndPaddedChild();ImGui::PopStyleColor();
}
inline void DrawLiveStructuredReports(DiagnosticCenterModel::Store& store,const HelpyImGuiTheme::Theme& theme){
    const auto& events=store.Events();
    std::size_t confirmed=0;
    for(const auto& event:events)if(!event.StructuredId.empty())++confirmed;
    if(!confirmed)return;
    if(ImGui::CollapsingHeader("Confirmed structured diagnostic codes##live",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::TextDisabled("%zu code-bearing reports in retained history. Legacy log entries without emitted codes remain unclassified.",confirmed);
        const float height=std::clamp(theme.Y(132.0f),theme.Y(70.0f),std::max(theme.Y(70.0f),ImGui::GetContentRegionAvail().y*.33f));
        if(ImGui::BeginChild("##structured-live-codes",ImVec2(0,height),true)){
            std::size_t shown=0;
            for(auto it=events.rbegin();it!=events.rend()&&shown<200;++it){
                if(it->StructuredId.empty())continue;
                ++shown;
                ImGui::PushID(static_cast<int>(it->Id));
                ImGui::TextDisabled("%s",TimeLabel(it->Timestamp).c_str());ImGui::SameLine();
                const auto& code=it->Category;
                ImGui::PushStyleColor(ImGuiCol_Text,SeverityColor(it->Severity,theme));
                if(ImGui::SmallButton(code.c_str()))Choose(store,it->GroupId);
                ImGui::PopStyleColor();
                if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Exact emitted code. Open the associated issue.");
                ImGui::SameLine();
                const auto end=it->Text.find('\n');
                const std::string_view line(it->Text.data(),end==std::string::npos?it->Text.size():end);
                ImGui::TextUnformatted(Fitted(line,ImGui::GetContentRegionAvail().x).c_str());
                ImGui::PopID();
            }
        }ImGui::EndChild();
    }
}
inline void Render(DiagnosticCenterModel::Store& current,
                   DiagnosticCenterHistory::Service* history=nullptr,
                   DiagnosticCenterMutes::Manager* muted=nullptr){
    const auto& theme=HelpyImGuiTheme::Current;
    MuteManager=muted;SessionHistory=history;
    if(!SplitInitialized){SplitRatio=theme.diagnosticSplitRatio;SplitInitialized=true;}
    DiagnosticCenterModel::Store* selected=&current;
    if(history){
        // Refresh once per session and when the user opens the session picker.
        static std::vector<DiagnosticCenterHistory::Session> sessions;
        static std::filesystem::path sessionDirectory;
        static std::string currentSession;
        static bool sessionComboOpen=false;
        if(history->Directory()!=sessionDirectory||history->CurrentId()!=currentSession){
            sessions=history->Sessions();sessionDirectory=history->Directory();
            currentSession=history->CurrentId();
        }
        if(!ArchivedSessionId.empty()&&std::none_of(sessions.begin(),sessions.end(),
           [](const auto& s){return s.Id==ArchivedSessionId;})){
            ArchivedSessionId.clear();SelectedGroup=0;LastInspected=0;SelectedHistoricalEvent=0;SelectedOccurrenceEvent=0;
            RevealHistoricalEvent=false;ExpandedOperations.clear();RevealSelectedInList=false;
            SessionStatus="Previously selected session is no longer retained.";
        }
        const auto switchSession=[&](const std::string& id){
            if(id.empty()){
                ArchivedSessionId.clear();selected=&current;SessionStatus.clear();
            }else{
                DiagnosticCenterModel::Store loaded;
                if(!history->Load(id,loaded)){
                    SessionStatus="Could not load that historical session; the current view was preserved.";
                    return;
                }
                ArchivedStore=std::move(loaded);ArchivedSessionId=id;selected=&ArchivedStore;SessionStatus.clear();
            }
            SelectedGroup=0;LastInspected=0;SelectedHistoricalEvent=0;SelectedOccurrenceEvent=0;
            RevealHistoricalEvent=false;ExpandedOperations.clear();RevealSelectedInList=false;DetailOnly=false;
        };
        std::vector<std::string> previousIds;
        previousIds.reserve(sessions.size());
        for(const auto& item:sessions)if(item.Id!=history->CurrentId())previousIds.push_back(item.Id);
        const auto newer=DiagnosticHistoryNavigation::AdjacentSession(previousIds,ArchivedSessionId,-1);
        const auto older=DiagnosticHistoryNavigation::AdjacentSession(previousIds,ArchivedSessionId,1);
        std::string label="Current session (Live)";
        if(!ArchivedSessionId.empty()){
            label="Previous session: "+ArchivedSessionId;
        }
        ImGui::SetNextItemWidth(std::min(theme.X(260),ImGui::GetContentRegionAvail().x));
        const bool comboOpen=ImGui::BeginCombo("##diagnostics-session",label.c_str());
        if(comboOpen){
            if(!sessionComboOpen)sessions=history->Sessions();
            if(ImGui::Selectable("Current session (Live)",ArchivedSessionId.empty()))switchSession({});
            for(const auto& item:sessions){
                if(item.Id==history->CurrentId())continue;
                const auto dash=item.Id.find('-',2);
                std::string stamp=item.Id;
                if(dash!=std::string::npos){
                    try{stamp=TimeLabel(DiagnosticCenterHistory::Instant(std::stoll(item.Id.substr(2,dash-2))));}catch(...){}
                }
                ImGui::PushID(item.Id.c_str());
                if(ImGui::Selectable(stamp.c_str(),ArchivedSessionId==item.Id))switchSession(item.Id);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        sessionComboOpen=comboOpen;
        if(ImGui::GetContentRegionAvail().x>theme.X(215))ImGui::SameLine();
        ImGui::BeginDisabled(!newer.has_value());
        if(ImGui::SmallButton("Newer session")&&newer)switchSession(*newer);
        ImGui::EndDisabled();
        ImGui::SameLine();ImGui::BeginDisabled(!older.has_value());
        if(ImGui::SmallButton("Older session")&&older)switchSession(*older);
        ImGui::EndDisabled();
        if(!ArchivedSessionId.empty()){
            ImGui::TextDisabled("Historical (read-only) | %zu retained events | %zu issue groups",
                                ArchivedStore.Events().size(),ArchivedStore.Groups().size());
        }
        if(!SessionStatus.empty())ImGui::TextColored(theme.warning,"%s",SessionStatus.c_str());
    }
    Historical=!ArchivedSessionId.empty();
    auto& store=*selected;
    if(RequestedTab>=0)Current=static_cast<View>(RequestedTab);
    int section=static_cast<int>(Current);constexpr std::array labels{"Overview","Issues","Live Log"};
    HelpyMenu::Tabs("diagnostic-views",labels,section);Current=static_cast<View>(section);
    if(HelpyMenu::BeginBody("Diagnostic view body",section)){
        const auto visible=Current;const auto tab=[&](const char*,View id,const auto& draw){if(visible==id)draw();};
        tab("Overview",View::Overview,[&]{DrawOverview(store,theme);});
        tab("Issues",View::Issues,[&]{DrawIssues(store,theme);});
        tab("Live Log",View::LiveLog,[&]{
            if(!Historical){
                DrawLiveStructuredReports(store,theme);
                LiveLogPanel::Render();
            }
            else{
                ImGui::TextDisabled("Saved session snapshot: retained events only. No current RuneSchema.log source-row links.");
                const auto& events=store.Events();
                const float evidenceHeight=std::max(theme.Y(90),ImGui::GetContentRegionAvail().y*0.70f);
                if(ImGui::BeginChild("##diagnostic-historical-log",ImVec2(0,evidenceHeight),true)){
                    if(RevealHistoricalEvent){
                        if(const auto index=DiagnosticHistoryNavigation::EventIndex(events,SelectedHistoricalEvent)){
                            ImGui::SetScrollY(static_cast<float>(*index)*ImGui::GetTextLineHeightWithSpacing());
                        }
                        RevealHistoricalEvent=false;
                    }
                    // Fixed-height rows support thousands of retained records without
                    // creating a wrapped ImGui text widget for every event each frame.
                    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(events.size()),ImGui::GetTextLineHeightWithSpacing());
                    while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
                        const auto& event=events[static_cast<std::size_t>(i)];
                        ImGui::PushID(static_cast<int>(event.Id));
                        auto line=TimeLabel(event.Timestamp)+" · "+LiveLog::Label(event.Severity)+" · "+event.Text;
                        if(ImGui::Selectable(Fitted(line,ImGui::GetContentRegionAvail().x).c_str(),
                           SelectedHistoricalEvent==event.Id))SelectedHistoricalEvent=event.Id;
                        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("%s",event.Text.c_str());
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
                if(const auto* event=store.FindEvent(SelectedHistoricalEvent)){
                    ImGui::TextDisabled("Retained historical evidence: %s",TimeLabel(event->Timestamp).c_str());
                    ImGui::TextWrapped("%s",event->Text.c_str());
                    if(ImGui::SmallButton("Copy retained text"))ImGui::SetClipboardText(event->Text.c_str());
                    if(!event->StructuredJson.empty()){
                        ImGui::SameLine();
                        if(ImGui::SmallButton("Copy structured JSON"))ImGui::SetClipboardText(event->StructuredJson.c_str());
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Inspect issue"))Choose(store,event->GroupId);
                }
            }
        });
        RequestedTab=-1;
    }HelpyMenu::EndBody();
    DrawExportPopup(theme);
}
}
