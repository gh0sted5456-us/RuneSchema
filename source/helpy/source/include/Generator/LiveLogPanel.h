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
    std::filesystem::path File,Override;
    char Search[256]{};
    bool Paused=false,Follow=true,Dirty=true,GroupRepeats=true,RawDetails=false;
    std::array<bool,4> Levels{true,true,true,true};
    std::vector<LiveLog::Group> Visible;
    std::array<size_t,4> Counts{};
    std::set<std::string> Scopes;
    std::uint64_t Selected=0,Revision=~std::uint64_t{};
};
inline std::array<StreamState,4> Streams;
// Retain the preview fixture's RuneSchema stream entry points.
inline LiveLog::History& History=Streams[0].History;
inline std::uint64_t& Selected=Streams[0].Selected;
inline std::future<std::string> ExportWorker;
inline size_t ExportStream=0;
inline bool Preview=false;
inline bool PreviewUE4SS=false;
inline int PreviewStream=-1;
inline auto Path(size_t index){return Streams[index].File.empty()?LiveLog::LogPath(HostServices::ModDirectory(),static_cast<LiveLog::Stream>(index)):Streams[index].File;}
inline void Shutdown(){for(auto& stream:Streams)if(stream.Worker.valid())try{(void)stream.Worker.get();}catch(...){}if(ExportWorker.valid())try{(void)ExportWorker.get();}catch(...){};}
inline void Poll(size_t index){
    auto& state=Streams[index];auto& Worker=state.Worker;auto& Cursor=state.Cursor;auto& History=state.History;
    auto& ReadError=state.ReadError;auto& Status=state.Status;auto& Selected=state.Selected;auto& NextRead=state.NextRead;
    if(ExportWorker.valid()&&ExportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Streams[ExportStream].Status=ExportWorker.get();}catch(...){Streams[ExportStream].Status="Export failed.";}}
    if(Preview)return;
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto batch=Worker.get();Cursor=std::move(batch.Next);ReadError=std::move(batch.Error);
            if(batch.Reset||(!state.File.empty()&&state.File!=batch.File)){History.Clear();Selected=0;Status="Log rotated or changed; showing the new file.";}
            state.File=std::move(batch.File);
            if(batch.Skipped)Status="Showing the recent log tail; the original full log is unchanged.";
            for(auto& line:batch.Lines)History.Append(std::move(line));
        }catch(const std::exception& error){ReadError=error.what();}
    }
    const auto now=std::chrono::steady_clock::now();
    if(!state.Paused&&!Worker.valid()&&now>=NextRead){
        NextRead=now+std::chrono::milliseconds(250);const auto path=Path(index),override=state.Override,mod=HostServices::ModDirectory();const auto cursor=Cursor;const auto previous=state.File;
        wchar_t local[32768]{};const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);const std::filesystem::path localRoot=length&&length<32768?local:L"";
        Worker=std::async(std::launch::async,[path,override,mod,localRoot,cursor,previous,index]{
            const auto file=!override.empty()?override:index>=2?LiveLog::ResolveGameLog(mod,localRoot):path;
            auto batch=LiveLog::Read(file,previous.empty()||previous==file?cursor:LiveLog::Cursor{});
            if(!previous.empty()&&previous!=file)batch.Reset=true;
            if(index==3)std::erase_if(batch.Lines,[](const auto& line){return !LiveLog::EOSLine(line);});return batch;
        });
    }
}
inline void Export(size_t index,const LiveLog::Filter& filter){
    auto& Status=Streams[index].Status;
    wchar_t file[32768]{};const auto suggested=std::filesystem::path(std::string(LiveLog::StreamName(static_cast<LiveLog::Stream>(index)))+"-filtered.log").wstring();wcscpy_s(file,suggested.c_str());OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"Log files (*.log)\0*.log\0Text files (*.txt)\0*.txt\0\0";dialog.lpstrDefExt=L"log";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
    if(!GetSaveFileNameW(&dialog))return;
    const std::filesystem::path destination=file;
    std::error_code error;const auto canonical=std::filesystem::weakly_canonical(destination,error);
    if(error){Status="Export destination could not be inspected.";return;}
    const auto mod=std::filesystem::weakly_canonical(HostServices::ModDirectory(),error);
    if(error){Status="Live log paths could not be inspected; no export was written.";return;}
    if(LiveLog::ProtectedExport(canonical,mod)){Status="Choose a different file; live logs, archives and rotation backups cannot be overwritten.";return;}
    for(size_t i=0;i<Streams.size();++i){error.clear();const auto live=std::filesystem::weakly_canonical(Path(i),error);if(!error&&LiveLog::Lower(live.generic_string())==LiveLog::Lower(canonical.generic_string())){Status="[RS-LOG-002] A live source log cannot be overwritten.";return;}}
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
    ImGui::TextWrapped("Color entries by keyword or source/subsystem. Matching is case-insensitive and literal (not regex). Keywords take priority over sources; first rule of each kind wins. Applies to all log tabs.");
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
    ImGui::TextColored(Paused?theme.warning:theme.accentHover,"%s",Paused?"PAUSED":"LIVE");ImGui::SameLine();const auto filename=Path(index).filename().string();ImGui::TextDisabled("%s",filename.c_str());
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Path(index).string().c_str());
    if(index==3)ImGui::TextDisabled("EOS categories from the game log; a separate EOS file can be selected in Actions.");
    if(index>=2&&!ReadError.empty())ImGui::TextWrapped("This game build may not emit a log. No logging or launch flags are changed. Select an existing log in Actions if it lives elsewhere.");
    ImGui::SetNextItemWidth(std::min(theme.X(380),ImGui::GetContentRegionAvail().x));Dirty|=ImGui::InputTextWithHint("##log-search","Search text or diagnostic code...",Search,sizeof(Search));
    if(ImGui::Checkbox("Pause",&Paused))NextRead={};ImGui::SameLine();ImGui::Checkbox("Follow newest",&Follow);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Actions"))ImGui::OpenPopup("Log actions");
    if(ImGui::BeginPopup("Log actions")){
        if(ImGui::MenuItem("Open log file"))ShellExecuteW(nullptr,L"open",Path(index).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(ImGui::MenuItem("Open log folder"))ShellExecuteW(nullptr,L"open",Path(index).parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(index>=2&&ImGui::MenuItem("Choose log file...",nullptr,false,!state.Worker.valid())){
            wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Log files (*.log;*.txt)\0*.log;*.txt\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&dialog)){state.Override=file;state.File=file;state.Cursor={};History.Clear();Selected=0;NextRead={};}
        }
        if(index>=2&&!state.Override.empty()&&ImGui::MenuItem("Use detected game log",nullptr,false,!state.Worker.valid())){state.Override.clear();state.File.clear();state.Cursor={};History.Clear();Selected=0;NextRead={};}
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
    if(changed){Scopes.clear();state.Counts={};for(const auto& row:History.Rows){if(!row.Scope.empty())Scopes.insert(row.Scope);++state.Counts[static_cast<size_t>(row.Severity)];}}
    const bool filtersActive=!Scope.empty()||std::any_of(Levels.begin(),Levels.end(),[](bool enabled){return !enabled;});
    if(ImGui::CollapsingHeader(filtersActive?"Filters & colors (active)###log-filters":"Filters & colors###log-filters")){
        for(size_t i=0;i<Levels.size();++i){if(i)ImGui::SameLine();Dirty|=ImGui::Checkbox(LiveLog::Label(static_cast<LiveLog::Level>(i)),&Levels[i]);}
        Dirty|=ImGui::Checkbox("Group consecutive repeats",&state.GroupRepeats);
        ImGui::SetNextItemWidth(theme.X(230));if(ImGui::BeginCombo("Source / subsystem",Scope.empty()?"All sources":Scope.c_str())){
            if(ImGui::Selectable("All sources",Scope.empty())){Scope.clear();Dirty=true;}
            for(const auto& source:Scopes)if(ImGui::Selectable(source.c_str(),source==Scope)){Scope=source;Dirty=true;}ImGui::EndCombo();}
        RenderColors();ImGui::TextDisabled("Viewer limit: 4,000 rows / 2 MiB per tab. Full files stay on disk.");
    }
    const LiveLog::Filter filter{Levels,LiveLog::Lower(Search),Scope};
    if(Dirty||changed){Visible=LiveLog::Grouped(History,filter,state.GroupRepeats);Revision=History.Revision;Dirty=false;}
    ImGui::TextDisabled("%zu events  |  %zu warnings  |  %zu errors",Visible.size(),state.Counts[2],state.Counts[3]);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%zu retained; %zu older rows dropped from this view only. Disk logs are unchanged.",History.Rows.size(),History.Dropped);
    if(!ReadError.empty())ImGui::TextColored(theme.warning,"%s",ReadError.c_str());if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    const auto selected=std::find_if(History.Rows.begin(),History.Rows.end(),[&](const auto& row){return row.Id==Selected;});
    const auto available=ImGui::GetContentRegionAvail();const bool compact=available.x<theme.X(720);
    const auto height=std::max(theme.Y(80),available.y-(selected!=History.Rows.end()?std::min(theme.Y(260),available.y*.48f):0));
    if(ImGui::BeginTable("Live log rows",compact?3:6,ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,height))){
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,theme.X(70));
        if(!compact){ImGui::TableSetupColumn("Time",ImGuiTableColumnFlags_WidthFixed,theme.X(76));ImGui::TableSetupColumn("Source",ImGuiTableColumnFlags_WidthFixed,theme.X(105));ImGui::TableSetupColumn("Reference",ImGuiTableColumnFlags_WidthFixed,theme.X(125));}
        ImGui::TableSetupColumn("Event");ImGui::TableSetupColumn("Count",ImGuiTableColumnFlags_WidthFixed,theme.X(42));ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(Visible.size()));while(clipper.Step())for(int index=clipper.DisplayStart;index<clipper.DisplayEnd;++index){const auto& group=Visible[index];const auto& row=History.Rows[group.First];ImGui::PushID(static_cast<int>(row.Id));ImGui::TableNextRow();ImGui::TableNextColumn();
            auto color=row.Severity==LiveLog::Level::Error?theme.error:row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Diagnostic?theme.textDisabled:theme.text;
            if(const auto custom=LiveLog::Color(row,theme.logColors))color={(*custom)[0],(*custom)[1],(*custom)[2],(*custom)[3]};
            ImGui::PushStyleColor(ImGuiCol_Text,color);if(ImGui::Selectable(LiveLog::Label(row.Severity),Selected==row.Id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))Selected=row.Id;ImGui::PopStyleColor();
            if(!compact){ImGui::TableNextColumn();ImGui::TextDisabled("%s",row.Time.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(row.Scope.c_str());ImGui::TableNextColumn();
                if(row.Help>=0){const auto& guide=LiveLog::Guides[row.Help];const auto label=(row.Suggested?"~ ":"")+std::string(guide.Code);if(ImGui::SmallButton(label.c_str())){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\nOpens the code reference. No log content is sent.",guide.Title.data(),row.Suggested?"Suggested reference only; not an emitted diagnosis.":"Exact emitted diagnostic code.");
                }else if(!row.ReportedCode.empty())ImGui::TextDisabled("%s",row.ReportedCode.c_str());}
            ImGui::TableNextColumn();const auto title=row.Help>=0&&!row.Suggested?LiveLog::Guides[row.Help].Title:std::string_view(row.Message);ImGui::TextUnformatted(title.data(),title.data()+title.size());
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nSelect the event for meaning, next steps and the original entry.",row.Message.c_str());
            ImGui::TableNextColumn();if(group.Count>1)ImGui::TextColored(theme.accentHover,"x%zu",group.Count);else ImGui::TextDisabled("1");ImGui::PopID();}
        if(Follow&&changed)ImGui::SetScrollY(ImGui::GetScrollMaxY());ImGui::EndTable();
    }
    if(selected!=History.Rows.end()){
        if(ImGui::BeginChild("Selected log details",ImVec2(0,0),true)){
            if(ImGui::SmallButton("Copy original"))ImGui::SetClipboardText(selected->Text.c_str());ImGui::SameLine();
            if(ImGui::SmallButton("Close details"))Selected=0;ImGui::SameLine();ImGui::Checkbox("Technical text",&state.RawDetails);
            if(selected->Help>=0){const auto& guide=LiveLog::Guides[selected->Help];ImGui::SeparatorText(selected->Suggested?"Suggested reference - not a diagnosed cause":guide.Title.data());
                ImGui::TextWrapped("%s",guide.Summary.data());ImGui::TextWrapped("Runtime response: %s",guide.Impact.data());ImGui::TextColored(theme.accentHover,"Next step");ImGui::TextWrapped("%s",guide.Action.data());
                if(ImGui::SmallButton("Open code reference")){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}ImGui::SameLine();if(ImGui::SmallButton("Copy code"))ImGui::SetClipboardText(guide.Code.data());
            }else{ImGui::TextWrapped("%s",selected->Message.c_str());if(!selected->ReportedCode.empty())ImGui::TextDisabled("Unrecognized code retained exactly: %s",selected->ReportedCode.c_str());}
            if(state.RawDetails){ImGui::SeparatorText("Original entry");ImGui::TextWrapped("%s",selected->Text.c_str());}
        }ImGui::EndChild();
    }
}
inline void Render(){
    if(ImGui::BeginTabBar("Live log streams")){
        if(ImGui::BeginTabItem("Game",nullptr,PreviewStream==2?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("Game stream");RenderStream(2);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("UE4SS",nullptr,Preview&&PreviewUE4SS?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("UE4SS stream");RenderStream(1);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("RuneSchema",nullptr,Preview&&!PreviewUE4SS&&PreviewStream<0?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("RuneSchema stream");RenderStream(0);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("EOS",nullptr,PreviewStream==3?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("EOS stream");RenderStream(3);ImGui::PopID();ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
}
}
