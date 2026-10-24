#pragma once

#include "Core/DiagnosticToastPatterns.h"
#include "Core/DiagnosticLegacyIntake.h"
#include "Core/StructuredDiagnosticContract.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A bounded, UI-thread-owned diagnostic read model. It consumes the same
// LiveLog::Entry objects as the M4 notification feed; it never opens another
// RuneSchema.log reader and never touches Unreal Engine objects.
namespace PS::DiagnosticCenterModel {
using Clock = std::chrono::system_clock;

struct Event {
    std::uint64_t Id=0,GroupId=0,SourceLogId=0;
    Clock::time_point Timestamp{};
    LiveLog::Level Severity=LiveLog::Level::Info;
    int Help=-1;
    std::string Source,Category,Text,StructuredId,StructuredJson,OperationId,ModId,MirrorId;
};
struct Group {
    std::uint64_t Id=0,Count=0;
    Clock::time_point FirstSeen{},LastSeen{};
    LiveLog::Level Severity=LiveLog::Level::Info;
    int Help=-1;
    std::string Title,Source,Category,Key,Code;
    std::string Message,Outcome,Phase,CauseStatus,CauseDescription;
    std::string ModId,File,Target,Property;
    std::deque<std::uint64_t> EvidenceIds;
    bool Viewed=false,Acknowledged=false;
};
struct Limits {
    std::size_t MaxEvents=4000;
    std::size_t MaxBytes=2*1024*1024;
    std::size_t MaxEvidencePerGroup=64;
};

inline Clock::time_point StructuredTimestamp(std::string_view stamp){
    // UTC conversion without timezone-dependent mktime or platform APIs.
    if(!StructuredDiagnostic::IsUtc(stamp))return Clock::now();
    const auto number=[&](std::size_t start,std::size_t length)->int{
        int value=0;for(std::size_t i=start;i<start+length&&i<stamp.size();++i){
            if(stamp[i]<'0'||stamp[i]>'9')return -1;
            value=value*10+(stamp[i]-'0');
        }return value;
    };
    const auto year=number(0,4),month=number(5,2),day=number(8,2);
    const auto hour=number(11,2),minute=number(14,2),second=number(17,2);
    if(year<1970||month<1||day<1||hour<0||hour>23||minute<0||minute>59||second<0||second>60)return Clock::now();
    const auto date=std::chrono::year{year}/std::chrono::month{static_cast<unsigned>(month)}/
                    std::chrono::day{static_cast<unsigned>(day)};
    if(!date.ok())return Clock::now();
    auto millis=0;
    if(stamp.size()>=24&&stamp[19]=='.'){
        const auto count=stamp.find('Z')-20;
        for(std::size_t i=0;i<3;++i){
            const char digit=i<count?stamp[20+i]:'0';
            if(digit<'0'||digit>'9')return Clock::now();
            millis=millis*10+(digit-'0');
        }
    }
    return Clock::time_point(std::chrono::sys_days{date}+
        std::chrono::hours(hour)+std::chrono::minutes(minute)+
        std::chrono::seconds(second)+std::chrono::milliseconds(millis));
}
inline bool Actionable(LiveLog::Level level){return level==LiveLog::Level::Error||level==LiveLog::Level::Warning;}
inline std::string CategoryOf(const LiveLog::Entry& entry){
    if(const auto* code=Diagnostics::Find(entry.ReportedCode))return std::string(code->Subsystem);
    const auto body=DiagnosticToastPatterns::Body(entry.Text);
    const auto lower=LiveLog::Lower(body);
    if(lower.find("import map")!=std::string::npos||lower.find("[$impdry]")!=std::string::npos||
       lower.find("import dependency")!=std::string::npos)return "Import map";
    if(lower.find("blueprint")!=std::string::npos)return "Blueprint modifications";
    if(lower.find("asset")!=std::string::npos)return "Asset resolution";
    if(lower.find("property")!=std::string::npos)return "Property modifications";
    if(entry.Help>=0 && entry.Help<static_cast<int>(LiveLog::Guides.size()) && LiveLog::Guides[entry.Help].Value!=Diagnostics::Id::Unclassified)
        return std::string(LiveLog::Guides[entry.Help].Title);
    return entry.Scope.empty()?"General RuneSchema":"Subsystem: "+entry.Scope;
}
inline std::string TitleOf(const LiveLog::Entry& entry){
    auto body=DiagnosticToastPatterns::Body(entry.Text);
    if(body.empty())body=entry.Text;
    // Titles derive only from text actually reported; do not infer a root cause.
    constexpr std::size_t length=160;
    return std::string(body.substr(0,std::min(length,body.size())));
}
inline std::string IdentityOf(const LiveLog::Entry& entry){
    // Conservative equivalence: same severity, subsystem, and complete logged
    // body. A timestamp/thread envelope is not part of the issue identity.
    // Never strip mod names, target paths, or other discriminating content.
    const auto body=DiagnosticToastPatterns::Body(entry.Text);
    return std::to_string(static_cast<int>(entry.Severity))+"\x1f"+entry.Scope+"\x1f"+
           std::string(body.empty()?std::string_view(entry.Text):body);
}

class Store {
    std::uint64_t m_nextEvent=1,m_nextGroup=1;
    std::size_t m_bytes=0;
    Limits m_limits;
    std::unordered_map<std::string,std::uint64_t> m_keys;
    std::unordered_set<std::string> m_structuredIds;
    std::unordered_map<std::string,std::uint64_t> m_mirrorEvents;
    std::unordered_map<std::uint64_t,std::size_t> m_indices;
    std::vector<Group> m_groups;
    std::deque<Event> m_events;
    std::uint64_t m_revision=0,m_dropped=0;
    void RemoveOldest(){
        if(m_events.empty())return;
        auto& event=m_events.front();
        m_bytes-=event.Text.size()+event.Source.size()+event.Category.size()+event.StructuredJson.size()+event.OperationId.size()+event.ModId.size();
        if(!event.StructuredId.empty())m_structuredIds.erase(event.StructuredId);
        if(!event.MirrorId.empty())m_mirrorEvents.erase(event.MirrorId);
        if(auto it=m_indices.find(event.GroupId);it!=m_indices.end()){
            auto& group=m_groups[it->second];
            if(!group.EvidenceIds.empty()&&group.EvidenceIds.front()==event.Id)group.EvidenceIds.pop_front();
            if(group.EvidenceIds.empty()){
                m_keys.erase(group.Key);m_indices.erase(group.Id);
                const auto removed=it->second;
                if(removed+1<m_groups.size()){
                    std::swap(m_groups[removed],m_groups.back());
                    m_indices[m_groups[removed].Id]=removed;
                }
                m_groups.pop_back();
            }
        }
        m_events.pop_front();++m_dropped;
    }
    void ReplaceMirror(const std::string& reference){
        const auto mirror=m_mirrorEvents.find(reference);
        if(mirror==m_mirrorEvents.end())return;
        const auto eventId=mirror->second;
        m_mirrorEvents.erase(mirror);
        const auto it=std::find_if(m_events.begin(),m_events.end(),[&](const Event& e){return e.Id==eventId;});
        if(it==m_events.end())return;
        const auto groupId=it->GroupId;
        m_bytes-=it->Text.size()+it->Source.size()+it->Category.size();
        m_events.erase(it);
        if(const auto gi=m_indices.find(groupId);gi!=m_indices.end()){
            auto& group=m_groups[gi->second];
            std::erase(group.EvidenceIds,eventId);
            if(group.Count>0)--group.Count;
            if(group.EvidenceIds.empty()){
                m_keys.erase(group.Key);
                const auto index=gi->second;
                m_indices.erase(gi);
                if(index+1<m_groups.size()){
                    std::swap(m_groups[index],m_groups.back());
                    m_indices[m_groups[index].Id]=index;
                }
                m_groups.pop_back();
            }
        }
        ++m_revision;
    }
public:
    explicit Store(Limits limits={}):m_limits(limits){}
    const std::deque<Event>& Events()const{return m_events;}
    const std::vector<Group>& Groups()const{return m_groups;}
    std::uint64_t Revision()const{return m_revision;}
    void SetLimits(Limits value){m_limits=value;while(!m_events.empty()&&(m_events.size()>std::max<std::size_t>(1,m_limits.MaxEvents)||m_bytes>std::max<std::size_t>(1,m_limits.MaxBytes)))RemoveOldest();++m_revision;}
    std::uint64_t Dropped()const{return m_dropped;}
    const Group* FindGroup(std::uint64_t id)const{
        const auto it=m_indices.find(id);return it==m_indices.end()?nullptr:&m_groups[it->second];
    }
    const Event* FindEvent(std::uint64_t id)const{
        const auto it=std::find_if(m_events.begin(),m_events.end(),[&](const Event& e){return e.Id==id;});
        return it==m_events.end()?nullptr:&*it;
    }
    std::uint64_t Accept(const LiveLog::Entry& entry,Clock::time_point timestamp=Clock::now()){
        const auto mirror=DiagnosticLegacyIntake::MirrorReference(entry);
        if(!mirror.empty()&&m_structuredIds.contains(mirror))return 0;
        const auto key=mirror.empty()?IdentityOf(entry):"mirror:"+mirror;
        if(!mirror.empty()&&m_mirrorEvents.contains(mirror))return 0;
        std::uint64_t groupId=0;
        if(const auto it=m_keys.find(key);it!=m_keys.end())groupId=it->second;
        if(!groupId){
            Group group;group.Id=m_nextGroup++;group.Key=key;group.Title=TitleOf(entry);
            group.Source=entry.Scope;group.Category=CategoryOf(entry);group.Severity=entry.Severity;
            group.Help=entry.Help;group.FirstSeen=timestamp;
            if(DiagnosticLegacyIntake::HasEmittedCode(entry))group.Code=entry.ReportedCode;
            groupId=group.Id;m_indices[groupId]=m_groups.size();m_keys.emplace(key,groupId);
            m_groups.push_back(std::move(group));
        }
        auto& group=m_groups[m_indices[groupId]];
        group.LastSeen=timestamp;++group.Count;
        if(group.Viewed && Actionable(entry.Severity))group.Viewed=false; // new recurrence
        if(group.Acknowledged && Actionable(entry.Severity))group.Acknowledged=false;
        Event event;event.Id=m_nextEvent++;event.GroupId=groupId;event.SourceLogId=entry.Id;
        event.Timestamp=timestamp;event.Severity=entry.Severity;event.Help=entry.Help;
        event.Source=entry.Scope;event.Category=group.Category;event.Text=entry.Text;
        if(!mirror.empty()){event.MirrorId=mirror;m_mirrorEvents.emplace(mirror,event.Id);}
        group.EvidenceIds.push_back(event.Id);
        if(group.EvidenceIds.size()>std::max<std::size_t>(1,m_limits.MaxEvidencePerGroup))group.EvidenceIds.pop_front();
        m_bytes+=event.Text.size()+event.Source.size()+event.Category.size();
        m_events.push_back(std::move(event));
        while(!m_events.empty() && (m_events.size()>std::max<std::size_t>(1,m_limits.MaxEvents)||
                                      m_bytes>std::max<std::size_t>(1,m_limits.MaxBytes)))RemoveOldest();
        ++m_revision;return groupId;
    }
    // Authoritative structured event: code+source+known target identifiers are
    // the issue identity; EventId/OperationId do not fragment recurrence.
    std::uint64_t AcceptStructured(const StructuredDiagnostic::Record& record,Clock::time_point timestamp={}){
        if(!StructuredDiagnostic::Validate(record))return 0;
        if(timestamp==Clock::time_point{})timestamp=StructuredTimestamp(record.Timestamp);
        const auto eventIdentity=record.SessionId+":"+record.EventId;
        if(m_structuredIds.contains(eventIdentity))return 0;
        ReplaceMirror(eventIdentity);
        const auto key="structured:"+StructuredDiagnostic::IssueKey(record);
        std::uint64_t groupId=0;
        if(auto it=m_keys.find(key);it!=m_keys.end())groupId=it->second;
        const auto severity=record.Severity=="Error"?LiveLog::Level::Error:
                            record.Severity=="Warning"?LiveLog::Level::Warning:LiveLog::Level::Info;
        if(!groupId){
            Group group;group.Id=m_nextGroup++;group.Key=key;group.Title=record.Title;
            group.Source=record.Source;group.Category=Diagnostics::Category(record.Code,true);group.Code=record.Code;
            group.Severity=severity;group.FirstSeen=timestamp;groupId=group.Id;
            m_indices[groupId]=m_groups.size();m_keys.emplace(key,groupId);m_groups.push_back(std::move(group));
        }
        auto& group=m_groups[m_indices[groupId]];
        group.Title=record.Title;
        group.Message=record.Message;
        group.Outcome=record.Outcome;
        const auto contextField=[&](const char* key)->std::string {
            const auto it=record.Context.find(key);
            return it!=record.Context.end()&&it->is_string()?it->get<std::string>():std::string{};
        };
        group.ModId=contextField("ModId");group.File=contextField("File");
        group.Target=contextField("Target");group.Property=contextField("Property");
        group.Phase=contextField("Phase");
        const auto causeField=[&](const char* key)->std::string {
            const auto it=record.Cause.find(key);
            return it!=record.Cause.end()&&it->is_string()?it->get<std::string>():std::string{};
        };
        group.CauseStatus=causeField("Status");
        group.CauseDescription=causeField("Description");
        // A coalesced writer summary describes the real first/last times, not
        // merely the time the summary was delivered to the reader.
        auto first=timestamp,last=timestamp;
        if(auto it=record.Evidence.find("FirstSeen");it!=record.Evidence.end()&&it->is_string())
            first=StructuredTimestamp(it->get<std::string>());
        if(auto it=record.Evidence.find("LastSeen");it!=record.Evidence.end()&&it->is_string())
            last=StructuredTimestamp(it->get<std::string>());
        group.FirstSeen=std::min(group.FirstSeen,first);
        group.LastSeen=std::max(group.LastSeen,last);
        std::uint64_t repeats=1;
        if(auto it=record.Evidence.find("RepeatCount");it!=record.Evidence.end()&&it->is_number_unsigned())
            repeats=it->get<std::uint64_t>();
        else if(auto it=record.Evidence.find("RepeatCount");it!=record.Evidence.end()&&it->is_number_integer())
            repeats=static_cast<std::uint64_t>(it->get<std::int64_t>());
        group.Count+=repeats;
        if(group.Viewed&&Actionable(severity))group.Viewed=false;
        if(group.Acknowledged&&Actionable(severity))group.Acknowledged=false;
        Event event;event.Id=m_nextEvent++;event.GroupId=groupId;event.Timestamp=timestamp;
        event.Severity=severity;event.Help=-1;event.Source=record.Source;event.Category=group.Category;
        event.StructuredId=eventIdentity;event.OperationId=record.OperationId;event.ModId=group.ModId;
        event.Text="["+record.Code+"] "+record.Title+" — "+record.Message;
        for(const auto* field:{"ModId","File","Target","Property","Phase"}){
            if(auto it=record.Context.find(field);it!=record.Context.end()&&it->is_string())
                event.Text+="\n"+std::string(field)+": "+it->get<std::string>();
        }
        if(auto cause=record.Cause.find("Description");cause!=record.Cause.end()&&cause->is_string())
            event.Text+="\nCause: "+cause->get<std::string>();
        if(auto text=StructuredDiagnostic::Encode(record))event.StructuredJson=std::move(*text);
        m_structuredIds.insert(eventIdentity);
        group.EvidenceIds.push_back(event.Id);
        if(group.EvidenceIds.size()>std::max<std::size_t>(1,m_limits.MaxEvidencePerGroup))group.EvidenceIds.pop_front();
        m_bytes+=event.Text.size()+event.Source.size()+event.Category.size()+event.StructuredJson.size()+event.OperationId.size()+event.ModId.size();
        m_events.push_back(std::move(event));
        while(!m_events.empty()&&(m_events.size()>std::max<std::size_t>(1,m_limits.MaxEvents)||
               m_bytes>std::max<std::size_t>(1,m_limits.MaxBytes)))RemoveOldest();
        ++m_revision;return groupId;
    }
    bool MarkViewed(std::uint64_t groupId,bool acknowledged=false){
        const auto it=m_indices.find(groupId);if(it==m_indices.end())return false;
        auto& group=m_groups[it->second];group.Viewed=true;
        if(acknowledged)group.Acknowledged=true;
        ++m_revision;return true;
    }
    std::size_t UnreadIssues()const{
        return static_cast<std::size_t>(std::count_if(m_groups.begin(),m_groups.end(),[](const Group& g){return Actionable(g.Severity)&&!g.Viewed;}));
    }
    void Clear(){m_keys.clear();m_structuredIds.clear();m_mirrorEvents.clear();m_indices.clear();m_groups.clear();m_events.clear();m_bytes=0;++m_revision;}
};
}
