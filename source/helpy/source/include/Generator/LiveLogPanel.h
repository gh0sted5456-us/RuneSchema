#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/LiveLog.h"
#include "Core/DiagnosticLegacyIntake.h"
#include "Core/StructuredDiagnosticRegistry.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Runtime/HostServices.h"
#include <commdlg.h>
#include <shellapi.h>
#include <future>
#include <fstream>
#include <functional>
#include <deque>
#include <cmath>
#include <set>

namespace PS::LiveLogPanel {
struct StreamState {
    LiveLog::History History;
    LiveLog::Cursor Cursor;
    PS::ProcessExitFuture<LiveLog::Batch> Worker;
    std::chrono::steady_clock::time_point NextRead{};
    std::string Status,ReadError,Scope;
    std::filesystem::path File,Override;
    char Search[256]{};
    bool Paused=false,Follow=true,Dirty=true,GroupRepeats=true,RawDetails=false;
    std::array<bool,4> Levels{true,true,true,true};
    std::vector<LiveLog::Group> Visible;
    std::array<size_t,4> Counts{};
    std::set<std::string> Scopes;
    std::deque<std::string> PausedLines;
    size_t PausedBytes=0;
    std::uint64_t Selected=0,Revision=~std::uint64_t{};
};
inline std::array<StreamState,4> Streams;
// Retain the preview fixture's RuneSchema stream entry points.
inline LiveLog::History& History=Streams[0].History;
inline std::uint64_t& Selected=Streams[0].Selected;
inline PS::ProcessExitFuture<std::string> ExportWorker;
inline size_t ExportStream=0;
inline bool Preview=false;
inline bool PreviewUE4SS=false;
inline bool PreviewOptions=false;
inline int PreviewStream=-1;
inline std::function<void(const LiveLog::Entry&)> OnEntry;
inline std::function<void(const LiveLog::Entry&)> OnBackfillEntry;
inline bool RuneSchemaLiveReady=false;
inline std::uint64_t RevealId=0;
inline bool SelectRuneSchemaTab=false;
inline auto Path(size_t index=0){return Streams[index].File.empty()?LiveLog::LogPath(HostServices::ModDirectory(),static_cast<LiveLog::Stream>(index)):Streams[index].File;}
inline void Shutdown(){OnEntry={};OnBackfillEntry={};for(auto& stream:Streams)if(stream.Worker.valid())try{(void)stream.Worker.get();}catch(...){}if(ExportWorker.valid())try{(void)ExportWorker.get();}catch(...){};}
inline void Poll(size_t index,bool background=false){
    auto& state=Streams[index];auto& Worker=state.Worker;auto& Cursor=state.Cursor;auto& History=state.History;
    auto& ReadError=state.ReadError;auto& Status=state.Status;auto& Selected=state.Selected;auto& NextRead=state.NextRead;
    if(ExportWorker.valid()&&ExportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Streams[ExportStream].Status=ExportWorker.get();}catch(...){Streams[ExportStream].Status="Export failed.";}}
    if(Preview)return;
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto batch=Worker.get();Cursor=std::move(batch.Next);ReadError=std::move(batch.Error);
            if(batch.Reset||(!state.File.empty()&&state.File!=batch.File)){History.Clear();Selected=0;Status="Log rotated or changed; showing the new file.";}
            state.File=std::move(batch.File);
            if(batch.Skipped)Status="Showing the recent log tail; the original full log is unchanged.";
            for(auto& line:batch.Lines){
                if(index==0 && state.Paused){
                    const auto entry=LiveLog::Parse(line);
                    if(RuneSchemaLiveReady){if(OnEntry)OnEntry(entry);}
                    else if(OnBackfillEntry)OnBackfillEntry(entry);
                    state.PausedBytes+=line.size();state.PausedLines.push_back(std::move(line));
                    while(state.PausedLines.size()>4000||state.PausedBytes>2*1024*1024){
                        state.PausedBytes-=state.PausedLines.front().size();state.PausedLines.pop_front();
                    }
                }else{
                    History.Append(std::move(line));
                    if(index==0){
                        const auto& entry=History.Rows.back();
                        if(RuneSchemaLiveReady){if(OnEntry)OnEntry(entry);}
                        else if(OnBackfillEntry)OnBackfillEntry(entry);
                    }
                }
            }
            if(index==0&&!batch.More&&(!batch.Error.empty()||batch.Next.Started))RuneSchemaLiveReady=true;
        }catch(const std::exception& error){ReadError=error.what();}
    }
    if(index==0&&!state.Paused&&!state.PausedLines.empty()){
        while(!state.PausedLines.empty()){
            state.PausedBytes-=state.PausedLines.front().size();
            History.Append(std::move(state.PausedLines.front()));state.PausedLines.pop_front();
        }
    }
    const auto now=std::chrono::steady_clock::now();
    if((index==0||!state.Paused)&&!Worker.valid()&&now>=NextRead){
        NextRead=now+std::chrono::milliseconds(background?1000:250);const auto path=Path(index),override=state.Override,mod=HostServices::ModDirectory();const auto cursor=Cursor;const auto previous=state.File;
        wchar_t local[32768]{};const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);const std::filesystem::path localRoot=length&&length<32768?local:L"";
        Worker=std::async(std::launch::async,[path,override,mod,localRoot,cursor,previous,index]{
            const auto file=!override.empty()?override:index>=2?LiveLog::ResolveGameLog(mod,localRoot):path;
            auto batch=LiveLog::Read(file,previous.empty()||previous==file?cursor:LiveLog::Cursor{},
                                     true,index==0?4ULL*1024*1024:1024ULL*1024);
            if(!previous.empty()&&previous!=file)batch.Reset=true;
            if(index==3)std::erase_if(batch.Lines,[](const auto& line){return !LiveLog::EOSLine(line);});return batch;
        });
    }
}
inline void Poll(){Poll(0);}
inline void DrainPending(){
    auto& stream=Streams[0];
    if(!stream.Worker.valid())return;
    stream.Worker.wait();
    stream.NextRead=std::chrono::steady_clock::now()+std::chrono::hours(24);
    Poll(0);
}
inline bool Reveal(std::uint64_t id){
    if(!id)return false;
    auto& state=Streams[0];
    const auto found=std::find_if(state.History.Rows.begin(),state.History.Rows.end(),[&](const auto& row){return row.Id==id;});
    if(found==state.History.Rows.end())return false;
    state.Selected=id;RevealId=id;SelectRuneSchemaTab=true;state.Follow=false;
    state.GroupRepeats=false;state.Scope.clear();state.Search[0]=0;
    state.Levels={true,true,true,true};state.Dirty=true;
    return true;
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
    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s",Path(index).string().c_str());
    if(index==3){ImGui::TextDisabled("EOS events from the game log");if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Use Actions to choose a separate EOS log.");}
    ImGui::SetNextItemWidth(std::max(theme.X(60),std::min(theme.X(380),ImGui::GetContentRegionAvail().x-theme.X(60))));Dirty|=ImGui::InputTextWithHint("##log-search","Search words, phrases or codes…",Search,sizeof(Search));
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("All words must match. Use quotes for a phrase, -word to exclude, or source:, code: and level:. Example: source:loganimation guid -optional");
    ImGui::SameLine();ImGui::BeginDisabled(!Search[0]);if(ImGui::SmallButton("Clear")){Search[0]=0;Dirty=true;}ImGui::EndDisabled();
    int preset=std::all_of(Levels.begin(),Levels.end(),[](bool value){return value;})?0:Levels==std::array<bool,4>{false,false,true,true}?1:Levels==std::array<bool,4>{false,false,false,true}?2:3;
    ImGui::SetNextItemWidth(theme.X(180));if(ImGui::Combo("Severity",&preset,"All\0Warnings + errors\0Errors\0Custom\0")){if(preset<3){Levels={preset==0,preset==0,preset<=1,true};Dirty=true;}}
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
    if(changed){Scopes.clear();for(const auto& row:History.Rows)if(!row.Scope.empty())Scopes.insert(row.Scope);}
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
    if(Dirty||changed){Visible=LiveLog::Grouped(History,filter,state.GroupRepeats);state.Counts={};for(const auto& group:Visible)state.Counts[static_cast<size_t>(History.Rows[group.First].Severity)]+=group.Count;Revision=History.Revision;Dirty=false;}
    ImGui::TextDisabled("%zu groups  |  %zu warnings  |  %zu errors",Visible.size(),state.Counts[2],state.Counts[3]);
    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%zu retained; %zu older rows dropped from this view only. Disk logs are unchanged.",History.Rows.size(),History.Dropped);
    if(!ReadError.empty())ImGui::TextColored(theme.warning,"%s",ReadError.c_str());if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    const auto selected=std::find_if(History.Rows.begin(),History.Rows.end(),[&](const auto& row){return row.Id==Selected;});
    const auto available=ImGui::GetContentRegionAvail();const bool compact=available.x<theme.X(720);
    const auto height=std::max(theme.Y(80),available.y-(selected!=History.Rows.end()?std::min(theme.Y(260),available.y*.48f):0));
    if(ImGui::BeginTable("Live log rows",compact?3:6,ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,height))){
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,theme.X(70));
        if(!compact){ImGui::TableSetupColumn("Time",ImGuiTableColumnFlags_WidthFixed,theme.X(76));ImGui::TableSetupColumn("Source",ImGuiTableColumnFlags_WidthFixed,theme.X(105));ImGui::TableSetupColumn("Reference",ImGuiTableColumnFlags_WidthFixed,theme.X(125));}
        ImGui::TableSetupColumn("Event");ImGui::TableSetupColumn("Count",ImGuiTableColumnFlags_WidthFixed,theme.X(42));ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(Visible.size()));while(clipper.Step())for(int visibleIndex=clipper.DisplayStart;visibleIndex<clipper.DisplayEnd;++visibleIndex){const auto& group=Visible[visibleIndex];const auto& row=History.Rows[group.First];ImGui::PushID(static_cast<int>(row.Id));ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto color=row.Severity==LiveLog::Level::Error?theme.error:row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Diagnostic?theme.textDisabled:theme.text;
            if(const auto custom=LiveLog::Color(row,theme.logColors))color={(*custom)[0],(*custom)[1],(*custom)[2],(*custom)[3]};
            ImGui::PushStyleColor(ImGuiCol_Text,color);if(ImGui::Selectable(LiveLog::Label(row.Severity),Selected==row.Id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))Selected=row.Id;ImGui::PopStyleColor();if(index==0&&row.Id==RevealId){ImGui::SetScrollHereY(.5f);RevealId=0;}
            if(!compact){ImGui::TableNextColumn();ImGui::TextDisabled("%s",row.Time.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(row.Scope.c_str());ImGui::TableNextColumn();
                if(!row.ReportedCode.empty() && StructuredDiagnosticRegistry::Find(row.ReportedCode)){
                    const auto* def=StructuredDiagnosticRegistry::Find(row.ReportedCode);
                    ImGui::TextColored(row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Error?theme.error:theme.info,
                        "%s",row.ReportedCode.c_str());
                    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Code emitted in the original log. The structured registry may define the same identifier separately.\n%.*s",static_cast<int>(def->Description.size()),def->Description.data());
                }else if(row.Help>=0){const auto& guide=LiveLog::Guides[row.Help];const auto label=(row.Suggested?"~ ":"")+std::string(guide.Code);if(ImGui::SmallButton(label.c_str())){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
                    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s\n%s\nOpens the code reference. No log content is sent.",guide.Title.data(),row.Suggested?"Suggested reference only; not an emitted diagnosis.":"Exact emitted diagnostic code.");
                }else if(!row.ReportedCode.empty()){ImGui::TextUnformatted(row.ReportedCode.c_str());if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Code present in the original line, but not registered locally. No match is fabricated.");}}
            ImGui::TableNextColumn();
            if(compact&&!row.ReportedCode.empty()&&StructuredDiagnosticRegistry::Find(row.ReportedCode)){
                ImGui::TextColored(row.Severity==LiveLog::Level::Warning?theme.warning:row.Severity==LiveLog::Level::Error?theme.error:theme.info,
                    "%s",row.ReportedCode.c_str());ImGui::SameLine();
            }
            const auto title=row.Help>=0&&!row.Suggested?LiveLog::Guides[row.Help].Title:std::string_view(row.Message);ImGui::TextUnformatted(title.data(),title.data()+title.size());
            if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s\nSelect the event for meaning, next steps and the original entry.",row.Message.c_str());
            ImGui::TableNextColumn();if(group.Count>1)ImGui::TextColored(theme.accentHover,"x%zu",group.Count);else ImGui::TextDisabled("1");ImGui::PopID();}
        if(index==0&&RevealId){
            const auto it=std::find_if(Visible.begin(),Visible.end(),[&](const auto& group){return History.Rows[group.First].Id==RevealId;});
            if(it!=Visible.end()){
                const auto target=static_cast<float>(it-Visible.begin())*ImGui::GetTextLineHeightWithSpacing();
                ImGui::SetScrollY(target);
                // Keep pending until a row is drawn in the virtualized list.
            }else{state.Status="The original log row has left the bounded viewer. Read the retained diagnostic evidence instead.";RevealId=0;}
        }else if(Follow&&changed)ImGui::SetScrollY(ImGui::GetScrollMaxY());ImGui::EndTable();
    }
    if(selected!=History.Rows.end()){
        if(ImGui::BeginChild("Selected log details",ImVec2(0,0),true)){
            if(ImGui::SmallButton("Copy original"))ImGui::SetClipboardText(selected->Text.c_str());ImGui::SameLine();
            if(ImGui::SmallButton("Close details"))Selected=0;ImGui::SameLine();ImGui::Checkbox("Technical text",&state.RawDetails);
            if(selected->Help>=0){const auto& guide=LiveLog::Guides[selected->Help];ImGui::SeparatorText(selected->Suggested?"Suggested reference - not a diagnosed cause":guide.Title.data());
                ImGui::TextWrapped("%s",guide.Summary.data());ImGui::TextWrapped("Runtime response: %s",guide.Impact.data());ImGui::TextColored(theme.accentHover,"Next step");ImGui::TextWrapped("%s",guide.Action.data());
                if(ImGui::SmallButton("Open code reference")){const auto url=LiveLog::HelpUrl(guide);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}ImGui::SameLine();if(ImGui::SmallButton("Copy code"))ImGui::SetClipboardText(guide.Code.data());
            }else{ImGui::TextWrapped("%s",selected->Message.c_str());if(!selected->ReportedCode.empty())ImGui::TextDisabled("Unrecognized code: %s",selected->ReportedCode.c_str());if(ImGui::SmallButton("Open reference")){const auto url=LiveLog::ReferenceUrl(*selected);ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}}
            if(state.RawDetails){ImGui::SeparatorText("Original entry");ImGui::TextWrapped("%s",selected->Text.c_str());}
        }ImGui::EndChild();
    }
}
inline void RenderCrashDumpOptions(){
    static nlohmann::json snapshot;static std::string status;static bool error=false;
    const auto read=[&](){nlohmann::json result;
        if(RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","CrashDumpRead"}},result)&&result.is_object()&&result.value("Ok",false)){snapshot=std::move(result);error=false;status.clear();}
        else{error=true;status=result.is_object()?result.value("Status",std::string("Crash capture settings require a matching RuneSchema core.")):"Crash capture settings are unavailable.";}
    };
    if(snapshot.is_null()&&!error)read();
    ImGui::SeparatorText("Crash capture");
    bool full=snapshot.is_object()&&snapshot.value("FullMemoryDump",false)&&snapshot.value("EnableDumping",true);
    ImGui::BeginDisabled(!snapshot.is_object()||error);
    if(ImGui::Checkbox("Full-memory crash dumps (next launch)",&full)){
        nlohmann::json result;
        if(RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","CrashDumpSave"},{"FullMemoryDump",full},{"ExpectedContents",snapshot.at("ExpectedContents")}},result)&&result.is_object()&&result.value("Ok",false)){
            snapshot=std::move(result);error=false;status=snapshot.value("Status",std::string{});
        }else{error=true;status=result.is_object()?result.value("Status",std::string("Change unconfirmed. Reload before retrying.")):"Change unconfirmed. Reload before retrying.";}
    }
    ImGui::EndDisabled();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+ImGui::GetFontSize()*28);
    ImGui::TextWrapped("Includes process memory, may contain private data and can create multiple GB files. Dumps stay local. Turn off after capturing the problem; restart the game to apply.");
    if(!status.empty())ImGui::TextWrapped("%s",status.c_str());ImGui::PopTextWrapPos();
    if(HelpyImGuiTheme::SoftButton("Reload crash capture settings"))read();
}
inline void Render(){
    static nlohmann::json options=nlohmann::json::object();static std::string optionStatus;static bool optionError=false;static double nextOptionsRead=0;
    const auto hint=[](const std::string& text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){PS::HelpyImGuiTheme::BeginDetailsTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);ImGui::TextUnformatted(text.c_str());ImGui::PopTextWrapPos();PS::HelpyImGuiTheme::EndDetailsTooltip();}};
    const auto readOptions=[&](){nlohmann::json result;if(RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","AdditionalLoggingRead"}},result)&&result.is_object()&&result.value("Ok",false)){options=result;optionError=false;optionStatus.clear();}
        else{optionError=true;optionStatus=result.is_object()?result.value("Status",std::string("Additional logging requires a matching RuneSchema core.")):"Additional logging requires a matching RuneSchema core.";}};
    if(options.empty()&&ImGui::GetTime()>=nextOptionsRead){readOptions();nextOptionsRead=ImGui::GetTime()+2;}
    const auto saveOption=[&](const char* key,bool enabled){auto document=options.at("Document");document[key]=enabled;nlohmann::json result;
        if(RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","AdditionalLoggingSave"},{"Document",document},{"ExpectedContents",options.at("ExpectedContents")}},result)&&result.is_object()&&result.value("Ok",false)){
            options=result;optionError=false;optionStatus=result.value("Status",std::string{});if(RuneSchemaSettingsPanel::Patch().empty())RuneSchemaSettingsPanel::Load();
        }else{optionError=true;optionStatus=result.is_object()?result.value("Status",std::string("Change unconfirmed. Reload before retrying.")):"Change unconfirmed. Reload before retrying.";}
    };
    bool enabled=!options.empty()&&options.at("Document").value("enabled",false);
    ImGui::BeginDisabled(options.empty()||optionError||!RuneSchemaSettingsPanel::Patch().empty());
    if(ImGui::Checkbox("Additional Logging Information",&enabled)){saveOption("enabled",enabled);if(enabled&&!optionError)ImGui::OpenPopup("Additional logging options");}
    hint("Optional context in RuneSchema.log only. Off by default. Select individual fields in Options; saved changes apply on the next sampler tick. Full local paths can reveal personal folder names. Turning this off does not erase existing entries or redact paths already present in ordinary native/mod messages.");
    ImGui::EndDisabled();ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Options##additional-logging"))ImGui::OpenPopup("Additional logging options");
    if(Preview&&PreviewOptions)ImGui::OpenPopup("Additional logging options");
    if(ImGui::BeginPopup("Additional logging options")){
        const auto option=[&](const char* label,const char* key,const char* description){bool selected=!options.empty()&&options.at("Document").value(key,false);
            ImGui::BeginDisabled(options.empty()||optionError||!RuneSchemaSettingsPanel::Patch().empty());if(ImGui::Checkbox(label,&selected))saveOption(key,selected);hint(description);ImGui::EndDisabled();};
        option("Installation / log paths","filePaths","Full executable, RuneSchema, cache and log paths. Can reveal usernames in directory names.");
        option("Windows region","region","Configured Windows country/region. No IP lookup, GPS, location permission or geolocation service.");
        option("Upload / download rates","networkRates","Observed traffic across active physical Ethernet/Wi-Fi interfaces, in Mbps. Device-wide traffic, not game-only traffic or connection capacity. Needs two comparable samples; no speed test is performed.");
        option("Save provider paths","savePaths","Known character/world provider directories, not save contents. Game Pass payload filenames remain provider-owned and may not be exposed.");
        option("World average network latency","averageLatency","Mean native player ping in a verified multiplayer world, with observed/total sample counts. Zero/invalid samples are excluded. Client-replicated ping can be stale; unavailable data is never reported as zero latency.");
        if(HelpyImGuiTheme::SoftButton("Reload logging options"))readOptions();
        if(!optionStatus.empty()){ImGui::TextDisabled(optionError?"Change needs attention — hover for details.":"Saved — hover for details.");hint(optionStatus);}
        RenderCrashDumpOptions();
        ImGui::EndPopup();
    }
    if(optionError){ImGui::TextColored(HelpyImGuiTheme::Current.warning,"Logging options need attention. Hover for details.");hint(optionStatus);}
    if(ImGui::BeginTabBar("Live log streams",ImGuiTabBarFlags_Reorderable)){
        if(ImGui::BeginTabItem("Game",nullptr,PreviewStream==2?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("Game stream");RenderStream(2);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("UE4SS",nullptr,Preview&&PreviewUE4SS?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("UE4SS stream");RenderStream(1);ImGui::PopID();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("RuneSchema",nullptr,(SelectRuneSchemaTab||Preview&&!PreviewUE4SS&&PreviewStream<0)?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("RuneSchema stream");RenderStream(0);ImGui::PopID();ImGui::EndTabItem();SelectRuneSchemaTab=false;}
        if(ImGui::BeginTabItem("EOS",nullptr,PreviewStream==3?ImGuiTabItemFlags_SetSelected:0)){ImGui::PushID("EOS stream");RenderStream(3);ImGui::PopID();ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
}
}
