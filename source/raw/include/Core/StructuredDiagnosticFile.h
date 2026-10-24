#pragma once
// Bounded independent JSONL sink. Does no formatting or filesystem I/O on Emit.
#include "Core/StructuredDiagnosticContract.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace PS::StructuredDiagnostic {
class File {
    std::filesystem::path m_path;
    std::deque<Record> m_queue;
    mutable std::mutex m_gate;
    std::condition_variable m_changed;
    std::thread m_worker;
    std::size_t m_maxBytes=10*1024*1024,m_queueMax=2048,m_writtenBytes=0;
    unsigned m_backups=3;
    void (*m_onWritten)(const Record&) noexcept=nullptr;
    bool m_stopping=false,m_running=false;
    std::atomic<std::uint64_t> m_dropped{0},m_errors{0},m_written{0};
    static std::filesystem::path Backup(const std::filesystem::path& path,unsigned n){auto result=path;result+=L"."+std::to_wstring(n);return result;}
    bool Rotate(std::ofstream& output) noexcept {
        try{
            output.close();std::error_code ec;
            std::filesystem::remove(Backup(m_path,m_backups),ec);ec.clear();
            for(unsigned i=m_backups;i>1;--i){
                const auto from=Backup(m_path,i-1);if(!std::filesystem::exists(from,ec))continue;
                ec.clear();std::filesystem::rename(from,Backup(m_path,i),ec);if(ec)return false;
            }
            if(std::filesystem::exists(m_path,ec)){
                ec.clear();std::filesystem::rename(m_path,Backup(m_path,1),ec);if(ec)return false;
            }
            output.open(m_path,std::ios::binary|std::ios::trunc);m_writtenBytes=0;
            return static_cast<bool>(output);
        }catch(...){return false;}
    }
    void Run() noexcept {
        try{
            std::ofstream output(m_path,std::ios::binary|std::ios::app);
            if(!output){++m_errors;std::lock_guard lock(m_gate);m_running=false;m_dropped+=m_queue.size();m_queue.clear();return;}
            std::error_code ec;m_writtenBytes=static_cast<std::size_t>(std::filesystem::file_size(m_path,ec));
            if(ec)m_writtenBytes=0;
            if(m_writtenBytes>=m_maxBytes&&!Rotate(output)){
                ++m_errors;std::lock_guard lock(m_gate);m_running=false;m_dropped+=m_queue.size();m_queue.clear();return;
            }
            std::uint32_t flushCount=0;
            const auto write=[&](const Record& record)->bool{
                auto encoded=Encode(record);
                if(!encoded){
                    // Only shorten optional narrative/evidence on the writer
                    // thread. Do not truncate the primary diagnostic identity.
                    Record compact=record;
                    const auto limitUtf8=[](std::string& value,std::size_t maximum){
                        if(value.size()<=maximum)return;
                        std::size_t cut=maximum;
                        while(cut>0&&(static_cast<unsigned char>(value[cut])&0xC0u)==0x80u)--cut;
                        value.resize(cut);
                    };
                    for(const auto* field:{"File","Target"}){
                        if(auto it=compact.Context.find(field);it!=compact.Context.end()&&it->is_string()){
                            auto value=it->get<std::string>();limitUtf8(value,1024);*it=std::move(value);
                        }
                    }
                    if(auto it=compact.Cause.find("Description");it!=compact.Cause.end()&&it->is_string()){
                        auto value=it->get<std::string>();limitUtf8(value,2048);*it=std::move(value);
                    }
                    limitUtf8(compact.Message,8192);
                    compact.Evidence["Truncated"]=true;
                    encoded=Encode(compact);
                }
                if(!encoded){++m_dropped;return true;}
                if(m_writtenBytes+encoded->size()>m_maxBytes&&!Rotate(output)){++m_errors;return false;}
                output.write(encoded->data(),static_cast<std::streamsize>(encoded->size()));
                if(!output){++m_errors;return false;}
                m_writtenBytes+=encoded->size();++m_written;
                if(m_onWritten)m_onWritten(record);
                if(++flushCount>=32||record.Severity=="Error"){
                    output.flush();flushCount=0;if(!output){++m_errors;return false;}
                }
                return true;
            };
            struct Bucket {Record Sample;std::uint64_t Count=0;std::string First,Last;std::chrono::steady_clock::time_point Expiry;};
            std::unordered_map<std::string,Bucket> buckets;
            // First event is immediately written; repeats within a bounded
            // one-second window are persisted in a summary with exact counts.
            const auto flush=[&](const std::string& key)->bool{
                auto it=buckets.find(key);if(it==buckets.end())return true;
                auto bucket=std::move(it->second);buckets.erase(it);
                if(bucket.Count){
                    bucket.Sample.Evidence["RepeatCount"]=bucket.Count;
                    bucket.Sample.Evidence["FirstSeen"]=bucket.First;
                    bucket.Sample.Evidence["LastSeen"]=bucket.Last;
                    return write(bucket.Sample);
                }
                return true;
            };
            for(;;){
                Record record;bool hasRecord=false,stopping=false;
                {std::unique_lock lock(m_gate);
                 m_changed.wait_for(lock,std::chrono::milliseconds(200),[&]{return m_stopping||!m_queue.empty();});
                 if(!m_queue.empty()){
                     record=std::move(m_queue.front());m_queue.pop_front();hasRecord=true;
                 }
                 stopping=m_stopping;
                }
                const auto now=std::chrono::steady_clock::now();
                for(auto it=buckets.begin();it!=buckets.end();){
                    if(it->second.Expiry<=now||stopping){const auto key=it->first;++it;if(!flush(key))goto shutdown;}
                    else ++it;
                }
                if(hasRecord){
                    // Exact observation semantics: never coalesce different
                    // operations or differing evidence even if issue key matches.
                    std::string key=IssueKey(record)+"\x1e"+record.OperationId+"\x1e"+
                                    record.Outcome+"\x1e"+record.Message+"\x1e"+
                                    record.Context.dump()+record.Cause.dump()+record.Evidence.dump();
                    auto it=buckets.find(key);
                    if(it!=buckets.end()&&it->second.Expiry>now){
                        const auto observedAt=record.Timestamp;
                        if(it->second.Count==0){it->second.Sample=std::move(record);it->second.First=observedAt;}
                        it->second.Last=observedAt;
                        ++it->second.Count;
                    }else{
                        if(it!=buckets.end()&&!flush(key))goto shutdown;
                        if(buckets.size()>=128){
                            auto oldest=buckets.begin();
                            if(!flush(oldest->first))goto shutdown;
                        }
                        Bucket bucket;bucket.Expiry=now+std::chrono::seconds(1);
                        buckets.emplace(std::move(key),std::move(bucket));
                        if(!write(record))goto shutdown;
                    }
                }
                if(!hasRecord&&flushCount){
                    output.flush();flushCount=0;
                    if(!output){++m_errors;goto shutdown;}
                }
                if(stopping&&!hasRecord)break;
            }
shutdown:
            for(auto it=buckets.begin();it!=buckets.end();){const auto key=it->first;++it;if(!flush(key))break;}
            output.flush();
        }catch(...){++m_errors;}
        std::lock_guard lock(m_gate);m_dropped+=m_queue.size();m_queue.clear();m_running=false;
    }

public:
    void OnWritten(void (*callback)(const Record&) noexcept) noexcept {
        m_onWritten=callback;
    }
    File()=default;
    File(const File&)=delete;File& operator=(const File&)=delete;
    ~File(){Close();}
    bool Open(const std::filesystem::path& path,std::size_t maxBytes=10*1024*1024,unsigned backups=3,std::size_t maxQueued=2048) noexcept {
        Close();
        if(maxBytes<MaxRecord||backups==0||backups>16||maxQueued==0||maxQueued>65536)return false;
        try{
            {std::lock_guard lock(m_gate);m_path=path;m_maxBytes=maxBytes;m_backups=backups;m_queueMax=maxQueued;
             m_stopping=false;m_running=true;}
            m_worker=std::thread([this]{Run();});return true;
        }catch(...){++m_errors;std::lock_guard lock(m_gate);m_running=false;return false;}
    }
    bool Emit(Record record) noexcept {
        try{
            if(!Validate(record))return false;
            std::lock_guard lock(m_gate);if(!m_running||m_stopping)return false;
            if(m_queue.size()>=m_queueMax){
                // Never displace an error to accept a lower-priority diagnostic.
                const auto it=std::find_if(m_queue.begin(),m_queue.end(),[](const Record& v){return v.Severity=="Info";});
                if(it!=m_queue.end()&&record.Severity!="Info")m_queue.erase(it);
                else if(record.Severity=="Error"){
                    const auto warning=std::find_if(m_queue.begin(),m_queue.end(),[](const Record& v){return v.Severity=="Warning";});
                    if(warning!=m_queue.end())m_queue.erase(warning);
                    else{++m_dropped;return false;}
                }else{++m_dropped;return false;}
                ++m_dropped;
            }
            m_queue.emplace_back(std::move(record));m_changed.notify_one();return true;
        }catch(...){++m_errors;return false;}
    }
    void Close() noexcept {
        {std::lock_guard lock(m_gate);m_stopping=true;}m_changed.notify_all();
        if(m_worker.joinable())m_worker.join();
        std::lock_guard lock(m_gate);m_running=false;m_queue.clear();
    }
    std::uint64_t Dropped()const noexcept{return m_dropped.load();}
    std::uint64_t Errors()const noexcept{return m_errors.load();}
    std::uint64_t Written()const noexcept{return m_written.load();}
};
}
