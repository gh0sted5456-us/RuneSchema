#pragma once
#include "Core/DiagnosticCenterModel.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

// HELPY-owned, self-contained diagnostic history. This layer does not read
// RuneSchema.log, access Unreal objects, or own the live log-reader lifecycle.
namespace PS::DiagnosticCenterHistory {
using json=nlohmann::json;
using Store=DiagnosticCenterModel::Store;
using Clock=DiagnosticCenterModel::Clock;
inline std::int64_t Millis(Clock::time_point t){return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();}
inline Clock::time_point Instant(std::int64_t ms){return Clock::time_point(std::chrono::milliseconds(ms));}
struct Retention {int AgeDays=14,MaxSessions=10,MaxStorageMB=20;};
struct Session {std::string Id;std::filesystem::path Path;bool Current=false;};
inline bool ValidId(std::string_view value){return !value.empty()&&value.size()<100&&std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='z')||c=='-';});}
inline json Snapshot(const Store& store,const std::string& id){
    json doc={{"version",2},{"session",id},{"recordedAt",Millis(Clock::now())},{"dropped",store.Dropped()},{"events",json::array()},{"states",json::array()}};
    for(const auto& e:store.Events()){
        json row={{"at",Millis(e.Timestamp)},{"severity",static_cast<int>(e.Severity)},
            {"source",e.Source},{"text",e.Text},{"help",e.Help}};
        if(!e.StructuredJson.empty())row["structured"]=e.StructuredJson;
        doc["events"].push_back(std::move(row));
    }
    for(const auto& g:store.Groups())if(g.Viewed||g.Acknowledged)doc["states"].push_back({{"key",g.Key},{"viewed",g.Viewed},{"ack",g.Acknowledged}});
    return doc;
}
inline bool Restore(const json& doc,Store& store){
    if(!doc.is_object()||(doc.value("version",0)!=1&&doc.value("version",0)!=2)||!doc.contains("events")||!doc["events"].is_array())return false;
    // All snapshots have bounded size; reject corrupt or intentionally enormous input.
    if(doc["events"].size()>4000)return false;
    Store restored;
    for(const auto& row:doc["events"]){
        if(!row.is_object())return false;
        auto text=row.at("text").get<std::string>();
        if(text.size()>16384)return false;
        if(row.contains("structured")&&row["structured"].is_string()&&!row["structured"].get<std::string>().empty()){
            auto structured=StructuredDiagnostic::Parse(row["structured"].get<std::string>());
            if(structured){restored.AcceptStructured(*structured,Instant(row.value("at",std::int64_t{0})));continue;}
        }
        LiveLog::Entry entry=LiveLog::Parse(std::move(text));
        entry.Scope=row.value("source",entry.Scope);
        entry.Help=row.value("help",entry.Help);
        const int level=row.value("severity",static_cast<int>(entry.Severity));
        if(level<0||level>3)return false;
        entry.Severity=static_cast<LiveLog::Level>(level);
        restored.Accept(entry,Instant(row.value("at",std::int64_t{0})));
    }
    if(doc.contains("states")&&doc["states"].is_array()){
        for(const auto& state:doc["states"]){
            if(!state.is_object())continue;
            const auto key=state.value("key",std::string{});
            for(const auto& group:restored.Groups())if(group.Key==key){
                if(state.value("viewed",false)||state.value("ack",false))restored.MarkViewed(group.Id,state.value("ack",false));
                break;
            }
        }
    }
    store=std::move(restored);return true;
}
inline bool AtomicReplace(const std::filesystem::path& path,const std::string& data){
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec)return false;
    const auto temp=path.wstring()+L".pending";
    {std::ofstream output(std::filesystem::path(temp),std::ios::binary|std::ios::trunc);if(!output)return false;
     output.write(data.data(),static_cast<std::streamsize>(data.size()));output.flush();if(!output)return false;}
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        std::filesystem::remove(std::filesystem::path(temp),ec);return false;
    }
#else
    std::filesystem::rename(std::filesystem::path(temp),path,ec);if(ec){std::filesystem::remove(std::filesystem::path(temp),ec);return false;}
#endif
    return true;
}
inline std::vector<Session> List(const std::filesystem::path& root){
    std::vector<Session> sessions;std::error_code ec;
    if(!std::filesystem::is_directory(root,ec))return sessions;
    for(const auto& e:std::filesystem::directory_iterator(root,ec)){
        if(ec)break;
        if(!e.is_regular_file(ec)||e.path().extension()!=L".json")continue;
        auto name=e.path().stem().string();if(!ValidId(name)||name.rfind("s-",0)!=0)continue;
        sessions.push_back({name,e.path(),false});
        if(sessions.size()>4096)break;
    }
    std::sort(sessions.begin(),sessions.end(),[](const Session& a,const Session& b){return a.Id>b.Id;});
    return sessions;
}
inline void Prune(const std::filesystem::path& root,const std::string& current,Retention limits){
    const auto sessions=List(root);
    const auto oldest=std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
        std::filesystem::file_time_type::clock::now()-std::chrono::hours(24*std::clamp(limits.AgeDays,1,365)));
    const auto maxFiles=static_cast<std::size_t>(std::clamp(limits.MaxSessions,1,100));
    const std::uintmax_t maxBytes=std::uintmax_t(std::clamp(limits.MaxStorageMB,5,500))*1024*1024;
    std::uintmax_t bytes=0;std::size_t kept=0;
    for(const auto& item:sessions){
        std::error_code ec;const auto size=std::filesystem::file_size(item.Path,ec);if(ec)continue;
        const auto stamp=std::filesystem::last_write_time(item.Path,ec);
        const bool expired=!ec&&stamp<oldest;
        const bool live=item.Id==current;
        if(live){++kept;bytes+=size;continue;}
        if(expired||kept>=maxFiles||bytes+size>maxBytes){std::filesystem::remove(item.Path,ec);continue;}
        ++kept;bytes+=size;
    }
}
class Service {
    std::filesystem::path m_root;
    std::string m_current;
    Retention m_limits;
    std::thread m_writer;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop=false,m_pruneRequested=false;
    // UI thread queues a bounded copy; JSON serialization and all file I/O
    // belong to the writer so autosaving does not stall ImGui or the game.
    std::optional<Store> m_pending;
    std::uint64_t m_lastRevision=0;
    std::chrono::steady_clock::time_point m_next{};
    std::string m_error;
    void Run(){
        for(;;){
            std::optional<Store> payload;
            Retention limits;
            {std::unique_lock lock(m_mutex);
             m_cv.wait(lock,[&]{return m_stop||m_pending.has_value()||m_pruneRequested;});
             if(m_stop&&!m_pending&&!m_pruneRequested)break;
             payload=std::move(m_pending);m_pending.reset();
             limits=m_limits;m_pruneRequested=false;}
            bool ok=true;
            if(payload){
                try{ok=AtomicReplace(m_root/(m_current+".json"),Snapshot(*payload,m_current).dump());}
                catch(...){ok=false;}
            }
            if(ok)Prune(m_root,m_current,limits);
            {std::lock_guard lock(m_mutex);
             m_error=ok?std::string{}:"Could not save diagnostic history. Check folder permissions.";}
        }
    }
public:
    Service()=default;
    Service(const Service&)=delete;Service& operator=(const Service&)=delete;
    ~Service(){Close();}
    void Open(std::filesystem::path parent,Retention limits={}){
        Close();m_root=std::move(parent)/"diagnostic-history";m_limits=limits;
        const auto ms=Millis(Clock::now());
        const auto tick=static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        m_current="s-"+std::to_string(ms)+"-"+std::to_string(tick%1000000000);
        m_stop=false;m_pruneRequested=true;m_lastRevision=0;m_next={};m_error.clear();
        // Enforce retention on startup even when the session emits no events.
        try{m_writer=std::thread([this]{Run();});}
        catch(...){m_error="Could not start diagnostic history writer.";m_current.clear();}
    }
    bool Opened()const{return !m_current.empty();}
    const std::string& CurrentId()const{return m_current;}
    const std::filesystem::path& Directory()const{return m_root;}
    void SetRetention(Retention limits){
        {std::lock_guard lock(m_mutex);m_limits=limits;m_pruneRequested=true;}
        m_cv.notify_one();
    }
    Retention Limits(){std::lock_guard lock(m_mutex);return m_limits;}
    void Queue(const Store& store,bool force=false){
        if(!Opened())return;
        const auto now=std::chrono::steady_clock::now();
        if(!force && (m_lastRevision==store.Revision()||now<m_next))return;
        // At most one queued snapshot: newer revisions supersede stale work.
        // Store is bounded independently of history retention.
        {std::lock_guard lock(m_mutex);m_pending=store;}
        m_cv.notify_one();m_lastRevision=store.Revision();m_next=now+std::chrono::seconds(8);
    }
    void Close(){
        if(!m_writer.joinable()){m_current.clear();return;}
        {std::lock_guard lock(m_mutex);m_stop=true;}
        m_cv.notify_one();m_writer.join();m_current.clear();
    }
    std::vector<Session> Sessions()const{return List(m_root);}
    bool Load(const std::string& id,Store& store)const{
        if(!ValidId(id)||id==m_current)return false;
        const auto file=m_root/(id+".json");
        std::error_code ec;if(!std::filesystem::is_regular_file(file,ec)||std::filesystem::file_size(file,ec)>10*1024*1024||ec)return false;
        try{std::ifstream in(file,std::ios::binary);if(!in)return false;
            const auto data=json::parse(in);if(data.value("session",std::string{})!=id)return false;
            return Restore(data,store);
        }catch(...){return false;}
    }
    bool Delete(const std::string& id){if(!ValidId(id)||id==m_current)return false;std::error_code ec;return std::filesystem::remove(m_root/(id+".json"),ec);}
    void ClearHistory(){for(const auto& s:Sessions())if(s.Id!=m_current)Delete(s.Id);}
    std::string Error(){std::lock_guard lock(m_mutex);return m_error;}
};
} // namespace PS::DiagnosticCenterHistory
