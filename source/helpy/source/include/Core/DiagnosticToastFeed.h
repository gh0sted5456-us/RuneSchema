#pragma once
#include "Core/LiveLog.h"
#include "Core/DiagnosticToastPatterns.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace PS::DiagnosticToastFeed {
struct Options {
    bool Enabled=true;
    int MaxVisible=4;   // A display limit, not an allocation or logging limit.
    int DurationSeconds=6;
};
struct Notice {
    LiveLog::Level Severity=LiveLog::Level::Info;
    std::string Text,Identity;
    std::uint32_t Repetitions=1;
    std::uint64_t SummaryCount=0,SummaryRepeated=0;
    std::chrono::steady_clock::time_point Until{},FirstSeen{};
    bool Summary=false;
    // Log-confirmed Blueprint modifications can share one toast without
    // discarding the independently preserved original Live Log entries.
    bool BlueprintBatch=false;
    std::uint32_t BatchCount=0;
    std::vector<std::string> BatchAssets; // at most three display examples
    bool BlueprintSummary=false,BlueprintOmissionAdded=false;
    std::uint32_t BatchErrors=0;
    bool ImportReport=false,ImportReady=false,ImportHasStatus=false;
    DiagnosticToastPatterns::ImportCounts ImportStats{};
    std::string ImportModules;
    bool operator==(const Notice&)const=default;
};
// UI-thread only. Never makes logging calls, accesses UObjects, or does file I/O.
class Feed {
    std::deque<Notice> m_visible;
    std::uint64_t m_suppressed=0,m_suppressedRepeated=0;
    std::vector<std::string> m_overflowIdentities; // Bounded duplicate classification, not an event queue.
    struct PendingImport {
        bool Active=false,Published=false,HasStatus=false,HasRequested=false,Ready=false;
        DiagnosticToastPatterns::ImportCounts Stats{};
        std::string Source,Modules;
        std::chrono::steady_clock::time_point Began{};
        std::uint32_t Lines=0;
    } m_import;
    void RecordOverflow(const std::string& identity,std::uint64_t count=1){
        m_suppressed+=count;
        if(std::find(m_overflowIdentities.begin(),m_overflowIdentities.end(),identity)!=m_overflowIdentities.end()){
            m_suppressedRepeated+=count;return;
        }
        m_suppressedRepeated+=count-1;
        if(m_overflowIdentities.size()<24)m_overflowIdentities.push_back(identity);
    }
    static int Priority(LiveLog::Level level){
        switch(level){
            case LiveLog::Level::Error:return 3;
            case LiveLog::Level::Warning:return 2;
            case LiveLog::Level::Diagnostic:return 1;
            default:return 0;
        }
    }
    static std::string BlueprintAsset(const LiveLog::Entry& entry){
        if(entry.Severity!=LiveLog::Level::Diagnostic)return {};
        std::string_view line=entry.Text;
        const auto prefix=line.find("[RuneSchema]");
        if(prefix==std::string_view::npos)return {};
        line.remove_prefix(prefix+sizeof("[RuneSchema]")-1);
        while(!line.empty()&&line.front()==' ')line.remove_prefix(1);
        constexpr std::string_view tag="[diagnostic]";
        if(line.size()<tag.size()||LiveLog::Lower(line.substr(0,tag.size()))!=tag)return {};
        line.remove_prefix(tag.size());
        while(!line.empty()&&line.front()==' ')line.remove_prefix(1);
        constexpr std::string_view start="Applied Blueprint changes to ";
        if(!line.starts_with(start))return {};
        line.remove_prefix(start.size());
        if(!line.empty()&&line.back()=='.')line.remove_suffix(1);
        if(line.empty()||line.size()>256||line.find(' ' )!=std::string_view::npos)return {};
        return std::string(line);
    }
    static std::string IdentityOf(const LiveLog::Entry& entry){
        // Remove the changing RuneSchema.log timestamp/thread prefix for grouping.
        std::string_view text=entry.Text;
        const auto tag=text.find("[RuneSchema]");
        if(tag!=std::string_view::npos)text.remove_prefix(tag);
        return std::to_string(static_cast<int>(entry.Severity))+":"+std::string(text.substr(0,512));
    }
    void Enqueue(Notice item,const Options& raw,const std::chrono::steady_clock::time_point now) {
        const int limit=std::clamp(raw.MaxVisible,1,10);
        const auto duration=std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15));
        for(auto& old:m_visible){
            if(!old.Summary && old.Identity==item.Identity){
                old.Repetitions=std::min<std::uint32_t>(old.Repetitions+1,999999);
                old.Until=std::min(now+duration,old.FirstSeen+std::max(std::chrono::seconds(12),duration*2));
                return;
            }
        }
        if(static_cast<int>(m_visible.size())>=limit){
            auto displaced=m_visible.end();
            for(auto it=m_visible.begin();it!=m_visible.end();++it){
                if(!it->Summary && Priority(item.Severity)>Priority(it->Severity) &&
                    (displaced==m_visible.end()||Priority(it->Severity)<Priority(displaced->Severity)))displaced=it;
            }
            if(displaced!=m_visible.end()){
                RecordOverflow(displaced->Identity,displaced->BlueprintBatch?displaced->BatchCount:displaced->Repetitions);
                m_visible.erase(displaced);
            }else{RecordOverflow(item.Identity);return;}
        }
        item.FirstSeen=now;item.Until=now+duration;
        m_visible.push_back(std::move(item));
    }
    void PublishImport(const Options& raw,const std::chrono::steady_clock::time_point now) {
        if(!m_import.Active || m_import.Published)return;
        Notice item;item.ImportReport=true;item.ImportReady=m_import.Ready;item.ImportHasStatus=m_import.HasStatus;
        item.ImportStats=m_import.Stats;item.ImportModules=m_import.Modules;
        item.Severity=m_import.HasStatus && m_import.Stats.Errors>0?LiveLog::Level::Error:
            m_import.HasStatus && !m_import.Ready?LiveLog::Level::Warning:LiveLog::Level::Info;
        item.Identity="import-dry:"+m_import.Source+":"+std::to_string(item.ImportHasStatus)+":"+
            std::to_string(item.ImportReady)+":"+std::to_string(item.ImportStats.Requested)+":"+
            std::to_string(item.ImportStats.Planned)+":"+item.ImportModules;
        Enqueue(std::move(item),raw,now);
        m_import.Published=true;
    }
    bool ConsumeImport(const LiveLog::Entry& entry,const Options& raw,const std::chrono::steady_clock::time_point now){
        using namespace DiagnosticToastPatterns;
        const std::string_view rawText=entry.Text,body=Body(rawText);
        const bool sourceMarker=body.find("[$ImpDry]")!=body.npos;
        const bool mapTitle=body=="IMPORT MAP";
        if(sourceMarker || mapTitle){
            if(sourceMarker && m_import.Active){PublishImport(raw,now);m_import={};}
            if(!m_import.Active || m_import.Published){m_import={};m_import.Active=true;m_import.Began=now;}
            if(sourceMarker)m_import.Source=std::string(body.substr(0,256));
            return true;
        }
        if(!m_import.Active)return false;
        if(m_import.Lines++>256 || now-m_import.Began>std::chrono::seconds(3)){
            PublishImport(raw,now);m_import={};return false;
        }
        if(m_import.Published){
            if(IsImportContinuation(rawText))return true;
            m_import={};return false;
        }
        ImportCounts stats{};bool ready=false;
        if(ParseImportStatus(rawText,ready,stats)){
            m_import.Ready=ready;m_import.Stats=stats;m_import.HasStatus=true;
        }else{
            std::string modules;
            if(ParseImportRequested(rawText,modules)){
                m_import.Modules=std::move(modules);m_import.HasRequested=true;
            }else if(body=="READY"||body=="BLOCKED"){
                m_import.Ready=body=="READY";
            }else if(ParseImportTotals(rawText,stats)){
                m_import.Stats=stats;m_import.HasStatus=true;
            }else if(!m_import.HasStatus && !m_import.Published && body.starts_with("+ ") && m_import.HasRequested){
                // A verbose report prints "Requested : N modules" followed by
                // indented + module lines. Keep at most 3 sample identifiers.
                const auto token=std::string(Trim(body.substr(2)));
                if(!token.empty()&&token.size()<=128){
                    if(m_import.Modules.find(" modules")!=std::string::npos)m_import.Modules.clear();
                    if(m_import.Modules.size()<270){if(!m_import.Modules.empty())m_import.Modules+=", ";m_import.Modules+=token;}
                }
            }else if(!IsImportContinuation(rawText)){
                PublishImport(raw,now);m_import={};return false;
            }
        }
        if(m_import.HasStatus && m_import.HasRequested)PublishImport(raw,now);
        return true;
    }
public:
    const std::deque<Notice>& Visible() const{return m_visible;}
    bool Empty() const{return m_visible.empty();}
    void Clear(){m_visible.clear();m_import={};m_suppressed=0;m_suppressedRepeated=0;m_overflowIdentities.clear();}
    void Accept(const LiveLog::Entry& entry,const Options& raw,const std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){
        if(!raw.Enabled)return;
        Tick(raw,now);
        if(ConsumeImport(entry,raw,now))return;
        const auto identity=IdentityOf(entry);
        const auto blueprintAsset=BlueprintAsset(entry);
        DiagnosticToastPatterns::BlueprintSummary summary{};
        if(DiagnosticToastPatterns::ParseBlueprintSummary(entry.Text,summary)){
            for(auto& item:m_visible){
                if(!item.Summary&&item.BlueprintBatch&&!item.BlueprintSummary){
                    item.BlueprintSummary=true;
                    item.BatchCount=summary.Updated; // Authoritative final count; never add it to detail events.
                    item.BatchErrors=summary.Errors;
                    if(summary.Errors)item.Severity=LiveLog::Level::Warning;
                    item.Until=std::min(now+std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15)),
                                        item.FirstSeen+std::chrono::seconds(12));
                    return;
                }
            }
        }
        std::uint32_t omitted=0;
        if(DiagnosticToastPatterns::ParseBlueprintOmitted(entry.Text,omitted)){
            for(auto& item:m_visible)if(!item.Summary&&item.BlueprintBatch&&!item.BlueprintSummary){
                // "omitted" is a confirmed additional detail count, but never
                // add it twice; the final loader summary will replace this
                // provisional total rather than being added to it.
                if(!item.BlueprintOmissionAdded){
                    item.BatchCount=std::min<std::uint32_t>(999999,item.BatchCount+std::min<std::uint32_t>(omitted,999999-item.BatchCount));
                    item.BlueprintOmissionAdded=true;
                }
                return;
            }
        }
        for(auto& item:m_visible){
            if(!item.Summary&&item.Identity==identity){
                item.Repetitions=std::min<std::uint32_t>(item.Repetitions+1,999999);
                if(item.BlueprintBatch && !item.BlueprintSummary)
                    item.BatchCount=std::min<std::uint32_t>(item.BatchCount+1,999999);
                const auto duration=std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15));
                item.Until=std::min(now+duration,item.FirstSeen+std::max(std::chrono::seconds(12),duration*2));
                return;
            }
        }
        if(!blueprintAsset.empty()){
            for(auto& item:m_visible){
                if(!item.Summary&&item.BlueprintBatch&&item.Until>now&&!item.BlueprintSummary){
                    item.BatchCount=std::min<std::uint32_t>(item.BatchCount+1,999999);
                    if(std::find(item.BatchAssets.begin(),item.BatchAssets.end(),blueprintAsset)==item.BatchAssets.end()&&item.BatchAssets.size()<3)
                        item.BatchAssets.push_back(blueprintAsset);
                    const auto duration=std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15));
                    item.Until=std::min(now+duration,item.FirstSeen+std::max(std::chrono::seconds(12),duration*2));
                    return;
                }
            }
        }
        Notice item;item.Severity=summary.Errors?LiveLog::Level::Warning:entry.Severity;item.Identity=identity;
        item.Text=entry.Text.substr(0,400);if(entry.Text.size()>400)item.Text+=" ...";
        if(!blueprintAsset.empty()){
            item.BlueprintBatch=true;item.BatchCount=1;item.BatchAssets.push_back(blueprintAsset);
        }
        Enqueue(std::move(item),raw,now);
    }
    void Tick(const Options& raw,const std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){
        if(!raw.Enabled){Clear();return;}
        if(m_import.Active && now-m_import.Began>=std::chrono::seconds(3)){
            PublishImport(raw,now);m_import={};
        }
        std::erase_if(m_visible,[&](const Notice& item){return item.Until<=now;});
        const auto limit=std::clamp(raw.MaxVisible,1,10);
        while(static_cast<int>(m_visible.size())>limit)m_visible.pop_front();
        if(m_suppressed){
            const auto existing=std::find_if(m_visible.begin(),m_visible.end(),[](const Notice& notice){return notice.Summary;});
            if(existing!=m_visible.end() || static_cast<int>(m_visible.size())<limit){
                const auto format=[](const Notice& summary){
                    auto message=std::to_string(summary.SummaryCount)+" additional log "+(summary.SummaryCount==1?"message":"messages");
                    if(summary.SummaryRepeated)message+=" ("+std::to_string(summary.SummaryRepeated)+" repeated)";
                    return message;
                };
                if(existing!=m_visible.end()){
                    existing->SummaryCount+=m_suppressed;existing->SummaryRepeated+=m_suppressedRepeated;
                    existing->Text=format(*existing);
                    existing->Until=now+std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15));
                } else {
                    Notice summary;summary.Summary=true;summary.FirstSeen=now;
                    summary.Until=now+std::chrono::seconds(std::clamp(raw.DurationSeconds,2,15));
                    summary.SummaryCount=m_suppressed;summary.SummaryRepeated=m_suppressedRepeated;
                    summary.Text=format(summary);summary.Severity=LiveLog::Level::Info;
                    m_visible.push_back(std::move(summary));
                }
                m_suppressed=0;m_suppressedRepeated=0;m_overflowIdentities.clear();
            }
        }
    }
};
}
