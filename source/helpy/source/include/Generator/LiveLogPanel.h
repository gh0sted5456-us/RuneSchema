#pragma once
#include "Core/LiveLog.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HostServices.h"
#include <commdlg.h>
#include <shellapi.h>
#include <future>
#include <fstream>
#include <set>

namespace PS::LiveLogPanel {
inline LiveLog::History History;
inline LiveLog::Cursor Cursor;
inline std::future<LiveLog::Batch> Worker;
inline std::future<std::string> ExportWorker;
inline std::chrono::steady_clock::time_point NextRead{};
inline std::string Status,ReadError,Scope;
inline char Search[256]{};
inline bool Paused=false,Follow=true,Dirty=true;
inline std::array<bool,4> Levels{true,true,true,true};
inline std::vector<size_t> Visible;
inline std::set<std::string> Scopes;
inline std::uint64_t Selected=0,Revision=~std::uint64_t{};
inline bool Preview=false;
inline auto Path(){return HostServices::ModDirectory()/"RuneSchema.log";}
inline void Shutdown(){if(Worker.valid())try{(void)Worker.get();}catch(...){}if(ExportWorker.valid())try{(void)ExportWorker.get();}catch(...){};}
inline void Poll(){
    if(ExportWorker.valid()&&ExportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Status=ExportWorker.get();}catch(...){Status="Export failed.";}}
    if(Preview)return;
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto batch=Worker.get();Cursor=std::move(batch.Next);ReadError=std::move(batch.Error);
            if(batch.Reset){History.Clear();Selected=0;Status="Log rotated or truncated; showing the new file.";}
            if(batch.Skipped)Status="Showing the recent log tail; the original full log is unchanged.";
            for(auto& line:batch.Lines)History.Append(std::move(line));
        }catch(const std::exception& error){ReadError=error.what();}
    }
    const auto now=std::chrono::steady_clock::now();
    if(!Paused&&!Worker.valid()&&now>=NextRead){NextRead=now+std::chrono::milliseconds(250);const auto path=Path();const auto cursor=Cursor;Worker=std::async(std::launch::async,[path,cursor]{return LiveLog::Read(path,cursor);});}
}
inline void Export(const LiveLog::Filter& filter){
    wchar_t file[32768]=L"RuneSchema-filtered.log";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Log files (*.log)\0*.log\0Text files (*.txt)\0*.txt\0\0";dialog.lpstrDefExt=L"log";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
    if(!GetSaveFileNameW(&dialog))return;
    const std::filesystem::path destination=file;
    std::error_code error;const auto canonical=std::filesystem::weakly_canonical(destination,error);const auto log=std::filesystem::weakly_canonical(Path(),error);
    if(LiveLog::Lower(canonical.string())==LiveLog::Lower(log.string())||LiveLog::Lower(canonical.string())==LiveLog::Lower(log.string()+".previous")){Status="Choose a different file; the live log and its rotation backup cannot be overwritten.";return;}
    // Freeze the filtered snapshot. The worker never borrows UI state.
    auto text=History.Export(filter);
    ExportWorker=std::async(std::launch::async,[destination,text=std::move(text)]{
        const auto handle=CreateFileW(destination.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)return std::string("Export failed: destination is unavailable or in use.");
        DWORD written=0;const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();CloseHandle(handle);
        return ok?std::string("Filtered log exported. Review player names, IDs and local paths before sharing."):std::string("Export failed; the destination may contain an incomplete export.");
    });Status="Exporting filtered log...";
}
inline void Render(){
    Poll();const auto& theme=HelpyImGuiTheme::Current;
    ImGui::TextDisabled("Recent RuneSchema.log entries · local only · 4,000-row / 2 MiB viewer limit");
    if(ImGui::Checkbox("Pause reading",&Paused))NextRead={};ImGui::SameLine();ImGui::Checkbox("Follow newest",&Follow);
    ImGui::SameLine();if(ImGui::SmallButton("Clear view")){History.Clear();Selected=0;Status="Viewer cleared; the log on disk was not changed.";}
    if(ImGui::Button("Open log folder"))ShellExecuteW(nullptr,L"open",HostServices::ModDirectory().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    ImGui::SameLine();ImGui::BeginDisabled(ExportWorker.valid()||History.Rows.empty());
    LiveLog::Filter filter{Levels,LiveLog::Lower(Search),Scope};
    if(ImGui::Button("Export filtered log"))Export(filter);ImGui::EndDisabled();
    ImGui::SetNextItemWidth(std::min(theme.X(380),ImGui::GetContentRegionAvail().x));Dirty|=ImGui::InputTextWithHint("##log-search","Search text or diagnostic code...",Search,sizeof(Search));
    for(size_t i=0;i<Levels.size();++i){if(i)ImGui::SameLine();Dirty|=ImGui::Checkbox(LiveLog::Label(static_cast<LiveLog::Level>(i)),&Levels[i]);}
    const bool changed=Revision!=History.Revision;
    if(changed){Scopes.clear();for(const auto& row:History.Rows)if(!row.Scope.empty())Scopes.insert(row.Scope);}
    ImGui::SetNextItemWidth(theme.X(230));if(ImGui::BeginCombo("Source / subsystem",Scope.empty()?"All sources":Scope.c_str())){
        if(ImGui::Selectable("All sources",Scope.empty())){Scope.clear();Dirty=true;}
        for(const auto& source:Scopes)if(ImGui::Selectable(source.c_str(),source==Scope)){Scope=source;Dirty=true;}ImGui::EndCombo();}
    filter={Levels,LiveLog::Lower(Search),Scope};
    if(Dirty||changed){Visible.clear();for(size_t i=0;i<History.Rows.size();++i)if(filter.Matches(History.Rows[i]))Visible.push_back(i);Revision=History.Revision;Dirty=false;}
    ImGui::Text("%zu matching / %zu retained · %zu older rows dropped",Visible.size(),History.Rows.size(),History.Dropped);
    if(!ReadError.empty())ImGui::TextColored(theme.warning,"%s",ReadError.c_str());if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    const auto height=std::max(theme.Y(100),ImGui::GetContentRegionAvail().y-theme.Y(150));
    if(ImGui::BeginTable("Live log rows",4,ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,height))){
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,theme.X(80));ImGui::TableSetupColumn("Subsystem",ImGuiTableColumnFlags_WidthFixed,theme.X(120));ImGui::TableSetupColumn("Help code",ImGuiTableColumnFlags_WidthFixed,theme.X(120));ImGui::TableSetupColumn("Message");ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(Visible.size()));while(clipper.Step())for(int index=clipper.DisplayStart;index<clipper.DisplayEnd;++index){const auto& row=History.Rows[Visible[index]];ImGui::PushID(static_cast<int>(row.Id));ImGui::TableNextRow();ImGui::TableNextColumn();
            const auto color=row.Severity==LiveLog::Level::Error?theme.error:row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Diagnostic?theme.textDisabled:theme.text;
            ImGui::PushStyleColor(ImGuiCol_Text,color);if(ImGui::Selectable(LiveLog::Label(row.Severity),Selected==row.Id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))Selected=row.Id;ImGui::PopStyleColor();
            ImGui::TableNextColumn();ImGui::TextUnformatted(row.Scope.c_str());ImGui::TableNextColumn();
            if(row.Help>=0){const auto& guide=LiveLog::Guides[row.Help];if(ImGui::SmallButton(guide.Code.data())){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nOpens RuneSchema's troubleshooting website. No log data is sent.",guide.Title.data());}
            ImGui::TableNextColumn();ImGui::TextUnformatted(row.Text.c_str());ImGui::PopID();}
        if(Follow&&changed)ImGui::SetScrollY(ImGui::GetScrollMaxY());ImGui::EndTable();
    }
    if(ImGui::BeginChild("Selected log details",ImVec2(0,0),true)){
        const auto selected=std::find_if(History.Rows.begin(),History.Rows.end(),[](const auto& row){return row.Id==Selected;});
        if(selected!=History.Rows.end()){if(ImGui::SmallButton("Copy entry"))ImGui::SetClipboardText(selected->Text.c_str());ImGui::TextWrapped("%s",selected->Text.c_str());}
        else ImGui::TextDisabled("Select a row to read the full entry. Clear view never deletes the original log.");
    }ImGui::EndChild();
}
}
