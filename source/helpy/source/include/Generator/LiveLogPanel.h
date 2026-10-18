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
struct StreamState {
    LiveLog::History History;
    LiveLog::Cursor Cursor;
    std::future<LiveLog::Batch> Worker;
    std::chrono::steady_clock::time_point NextRead{};
    std::string Status,ReadError,Scope;
    char Search[256]{};
    bool Paused=false,Follow=true,Dirty=true;
    std::array<bool,4> Levels{true,true,true,true};
    std::vector<size_t> Visible;
    std::set<std::string> Scopes;
    std::uint64_t Selected=0,Revision=~std::uint64_t{};
};
inline std::array<StreamState,2> Streams;
// Retain the preview fixture's RuneSchema stream entry points.
inline LiveLog::History& History=Streams[0].History;
inline std::uint64_t& Selected=Streams[0].Selected;
inline std::future<std::string> ExportWorker;
inline size_t ExportStream=0;
inline bool Preview=false;
inline bool PreviewUE4SS=false;
inline auto Path(size_t index){return LiveLog::LogPath(HostServices::ModDirectory(),index?LiveLog::Stream::UE4SS:LiveLog::Stream::RuneSchema);}
inline void Shutdown(){for(auto& stream:Streams)if(stream.Worker.valid())try{(void)stream.Worker.get();}catch(...){}if(ExportWorker.valid())try{(void)ExportWorker.get();}catch(...){};}
inline void Poll(size_t index){
    auto& state=Streams[index];auto& Worker=state.Worker;auto& Cursor=state.Cursor;auto& History=state.History;
    auto& ReadError=state.ReadError;auto& Status=state.Status;auto& Selected=state.Selected;auto& NextRead=state.NextRead;
    if(ExportWorker.valid()&&ExportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Streams[ExportStream].Status=ExportWorker.get();}catch(...){Streams[ExportStream].Status="Export failed.";}}
    if(Preview)return;
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto batch=Worker.get();Cursor=std::move(batch.Next);ReadError=std::move(batch.Error);
            if(batch.Reset){History.Clear();Selected=0;Status="Log rotated or truncated; showing the new file.";}
            if(batch.Skipped)Status="Showing the recent log tail; the original full log is unchanged.";
            for(auto& line:batch.Lines)History.Append(std::move(line));
        }catch(const std::exception& error){ReadError=error.what();}
    }
    const auto now=std::chrono::steady_clock::now();
    if(!state.Paused&&!Worker.valid()&&now>=NextRead){NextRead=now+std::chrono::milliseconds(250);const auto path=Path(index);const auto cursor=Cursor;Worker=std::async(std::launch::async,[path,cursor]{return LiveLog::Read(path,cursor);});}
}
inline void Export(size_t index,const LiveLog::Filter& filter){
    auto& Status=Streams[index].Status;
    wchar_t file[32768]{};wcscpy_s(file,index?L"UE4SS-filtered.log":L"RuneSchema-filtered.log");OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Log files (*.log)\0*.log\0Text files (*.txt)\0*.txt\0\0";dialog.lpstrDefExt=L"log";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
    if(!GetSaveFileNameW(&dialog))return;
    const std::filesystem::path destination=file;
    std::error_code error;const auto canonical=std::filesystem::weakly_canonical(destination,error);
    if(error){Status="Export destination could not be inspected.";return;}
    const auto mod=std::filesystem::weakly_canonical(HostServices::ModDirectory(),error);
    if(error){Status="Live log paths could not be inspected; no export was written.";return;}
    if(LiveLog::ProtectedExport(canonical,mod)){Status="Choose a different file; live logs, archives and rotation backups cannot be overwritten.";return;}
    // Freeze the filtered snapshot. The worker never borrows UI state.
    auto text=Streams[index].History.Export(filter);ExportStream=index;
    ExportWorker=std::async(std::launch::async,[destination,text=std::move(text)]{
        const auto handle=CreateFileW(destination.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)return std::string("Export failed: destination is unavailable or in use.");
        DWORD written=0;const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();CloseHandle(handle);
        return ok?std::string("Filtered log exported. Review player names, IDs and local paths before sharing."):std::string("Export failed; the destination may contain an incomplete export.");
    });Status="Exporting filtered log...";
}
inline void RenderColors(){
    static bool pendingSave=false;
    if(!ImGui::CollapsingHeader("Live Log colors")){
        if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}
        return;
    }
    auto& rules=HelpyImGuiTheme::Current.logColors;
    ImGui::TextWrapped("Color whole entries by keyword or source/subsystem. Matching is case-insensitive and literal (not regex). Keywords take priority over sources; first rule of each kind wins. Applies to both log tabs; default severity colors remain when no rule matches.");
    ImGui::BeginDisabled(rules.size()>=32);
    if(ImGui::SmallButton("Add keyword color")){rules.push_back({true,false,"keyword"});pendingSave=true;}
    ImGui::SameLine();if(ImGui::SmallButton("Add source color")){rules.push_back({true,true,"RuneSchema"});pendingSave=true;}
    ImGui::EndDisabled();
    for(size_t i=0;i<rules.size();){auto& rule=rules[i];ImGui::PushID(static_cast<int>(i));
        pendingSave|=ImGui::Checkbox("##enabled",&rule.Enabled);ImGui::SameLine();
        int kind=rule.Source?1:0;ImGui::SetNextItemWidth(100);if(ImGui::Combo("##kind",&kind,"Keyword\0Source\0")){rule.Source=kind==1;pendingSave=true;}
        ImGui::SameLine();char match[129]{};strncpy_s(match,rule.Match.c_str(),_TRUNCATE);ImGui::SetNextItemWidth(180);
        if(ImGui::InputText("##match",match,sizeof(match))){rule.Match=match;pendingSave=true;}
        ImGui::SameLine();pendingSave|=ImGui::ColorEdit4("##color",rule.RGBA.data(),ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();const bool remove=ImGui::SmallButton("Remove");ImGui::PopID();
        if(remove){rules.erase(rules.begin()+i);pendingSave=true;}else ++i;
    }
    if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}
}
inline void RenderStream(size_t index){
    Poll(index);const auto& theme=HelpyImGuiTheme::Current;auto& state=Streams[index];
    auto& History=state.History;auto& Selected=state.Selected;auto& Status=state.Status;auto& ReadError=state.ReadError;
    auto& Paused=state.Paused;auto& Follow=state.Follow;auto& NextRead=state.NextRead;auto& Levels=state.Levels;
    auto& Search=state.Search;auto& Scope=state.Scope;auto& Dirty=state.Dirty;auto& Revision=state.Revision;auto& Scopes=state.Scopes;auto& Visible=state.Visible;
    ImGui::TextDisabled("%s  ·  %s",index?"UE4SS.log":"RuneSchema.log",Paused?"Paused":"Live");
    ImGui::SetNextItemWidth(std::min(theme.X(380),ImGui::GetContentRegionAvail().x));Dirty|=ImGui::InputTextWithHint("##log-search","Search text or diagnostic code...",Search,sizeof(Search));
    if(ImGui::Checkbox("Pause",&Paused))NextRead={};ImGui::SameLine();ImGui::Checkbox("Follow newest",&Follow);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Actions"))ImGui::OpenPopup("Log actions");
    if(ImGui::BeginPopup("Log actions")){
        if(ImGui::MenuItem("Open log file"))ShellExecuteW(nullptr,L"open",Path(index).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(ImGui::MenuItem("Open log folder"))ShellExecuteW(nullptr,L"open",Path(index).parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(!index&&ImGui::MenuItem("Open RuneSchema archives")){
            const auto archives=LiveLog::ArchivePath(HostServices::ModDirectory());std::error_code error;
            if(std::filesystem::is_directory(archives,error))ShellExecuteW(nullptr,L"open",archives.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            else Status="No archive folder yet. Enable Archive logs in Settings > RuneSchema; previous runs are kept from the next launch.";
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Export filtered entries",nullptr,false,!ExportWorker.valid()&&!History.Rows.empty()))Export(index,{Levels,LiveLog::Lower(Search),Scope});
        if(ImGui::MenuItem("Clear view")){History.Clear();Selected=0;Status="Viewer cleared; the continuous log on disk was not changed.";}
        ImGui::TextDisabled("Exports use retained entries, not the full file.");
        if(!index)ImGui::TextDisabled("Archive setting: Settings > RuneSchema.");
        ImGui::EndPopup();
    }
    const bool changed=Revision!=History.Revision;
    if(changed){Scopes.clear();for(const auto& row:History.Rows)if(!row.Scope.empty())Scopes.insert(row.Scope);}
    const bool filtersActive=!Scope.empty()||std::any_of(Levels.begin(),Levels.end(),[](bool enabled){return !enabled;});
    if(ImGui::CollapsingHeader(filtersActive?"Filters & colors (active)###log-filters":"Filters & colors###log-filters")){
        for(size_t i=0;i<Levels.size();++i){if(i)ImGui::SameLine();Dirty|=ImGui::Checkbox(LiveLog::Label(static_cast<LiveLog::Level>(i)),&Levels[i]);}
        ImGui::SetNextItemWidth(theme.X(230));if(ImGui::BeginCombo("Source / subsystem",Scope.empty()?"All sources":Scope.c_str())){
            if(ImGui::Selectable("All sources",Scope.empty())){Scope.clear();Dirty=true;}
            for(const auto& source:Scopes)if(ImGui::Selectable(source.c_str(),source==Scope)){Scope=source;Dirty=true;}ImGui::EndCombo();}
        RenderColors();ImGui::TextDisabled("Viewer limit: 4,000 rows / 2 MiB per tab. Full files stay on disk.");
    }
    const LiveLog::Filter filter{Levels,LiveLog::Lower(Search),Scope};
    if(Dirty||changed){Visible.clear();for(size_t i=0;i<History.Rows.size();++i)if(filter.Matches(History.Rows[i]))Visible.push_back(i);Revision=History.Revision;Dirty=false;}
    ImGui::TextDisabled("%zu matching entries",Visible.size());
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%zu retained; %zu older rows dropped from this view only. Disk logs are unchanged.",History.Rows.size(),History.Dropped);
    if(!ReadError.empty())ImGui::TextColored(theme.warning,"%s",ReadError.c_str());if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    const auto selected=std::find_if(History.Rows.begin(),History.Rows.end(),[&](const auto& row){return row.Id==Selected;});
    const auto height=std::max(theme.Y(100),ImGui::GetContentRegionAvail().y-(selected!=History.Rows.end()?theme.Y(120):0));
    if(ImGui::BeginTable("Live log rows",4,ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,height))){
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,theme.X(80));ImGui::TableSetupColumn("Subsystem",ImGuiTableColumnFlags_WidthFixed,theme.X(120));ImGui::TableSetupColumn("Help code",ImGuiTableColumnFlags_WidthFixed,theme.X(120));ImGui::TableSetupColumn("Message");ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(Visible.size()));while(clipper.Step())for(int index=clipper.DisplayStart;index<clipper.DisplayEnd;++index){const auto& row=History.Rows[Visible[index]];ImGui::PushID(static_cast<int>(row.Id));ImGui::TableNextRow();ImGui::TableNextColumn();
            auto color=row.Severity==LiveLog::Level::Error?theme.error:row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Diagnostic?theme.textDisabled:theme.text;
            if(const auto custom=LiveLog::Color(row,theme.logColors))color={(*custom)[0],(*custom)[1],(*custom)[2],(*custom)[3]};
            ImGui::PushStyleColor(ImGuiCol_Text,color);if(ImGui::Selectable(LiveLog::Label(row.Severity),Selected==row.Id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))Selected=row.Id;ImGui::PopStyleColor();
            ImGui::TableNextColumn();ImGui::TextUnformatted(row.Scope.c_str());ImGui::TableNextColumn();
            if(row.Help>=0){const auto& guide=LiveLog::Guides[row.Help];if(ImGui::SmallButton(guide.Code.data())){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nOpens RuneSchema's troubleshooting website. No log data is sent.",guide.Title.data());}
            ImGui::TableNextColumn();ImGui::TextColored(color,"%s",row.Text.c_str());ImGui::PopID();}
        if(Follow&&changed)ImGui::SetScrollY(ImGui::GetScrollMaxY());ImGui::EndTable();
    }
    if(selected!=History.Rows.end()){
        if(ImGui::BeginChild("Selected log details",ImVec2(0,0),true)){if(ImGui::SmallButton("Copy entry"))ImGui::SetClipboardText(selected->Text.c_str());ImGui::TextWrapped("%s",selected->Text.c_str());}ImGui::EndChild();
    }
}
inline void Render(){
    if(ImGui::BeginTabBar("Live log streams")){
        if(ImGui::BeginTabItem("RuneSchema",nullptr,Preview&&!PreviewUE4SS?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("RuneSchema stream");RenderStream(0);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("UE4SS",nullptr,Preview&&PreviewUE4SS?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("UE4SS stream");RenderStream(1);ImGui::PopID();ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
}
}
