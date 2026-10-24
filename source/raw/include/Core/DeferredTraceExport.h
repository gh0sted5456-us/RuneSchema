#pragma once
#include "nlohmann/json.hpp"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>

namespace PS {
// Only owned JSON reaches this worker; no Unreal objects or borrowed event memory.
class DeferredTraceExport {
public:
    using Sink=std::function<void(const std::string&,const nlohmann::json&)>;
    using Failure=std::function<void(const std::string&,const std::string&)>;
    DeferredTraceExport(Sink sink,Failure failure):m_sink(std::move(sink)),m_failure(std::move(failure)),m_worker([this]{
        try{Run();}catch(const std::exception& error){try{m_failure("worker",error.what());}catch(...){}}
        catch(...){try{m_failure("worker","Trace worker stopped after an unknown failure");}catch(...){}}
    }){}
    ~DeferredTraceExport(){Stop();}
    void Submit(const std::string& id,nlohmann::json snapshot){
        {std::lock_guard lock(m_mutex);if(m_stopping)return;m_pending.insert_or_assign(id,std::move(snapshot));}
        m_ready.notify_one();
    }
    void Stop(){
        {std::lock_guard lock(m_mutex);m_stopping=true;}
        m_ready.notify_one();if(m_worker.joinable())m_worker.join();
    }
private:
    Sink m_sink;Failure m_failure;
    std::mutex m_mutex;std::condition_variable m_ready;
    bool m_stopping=false;
    std::map<std::string,nlohmann::json> m_pending;
    std::thread m_worker;
    void Run(){
        std::set<std::string> reported;
        for(;;){
            std::unique_lock lock(m_mutex);
            m_ready.wait(lock,[&]{return m_stopping||!m_pending.empty();});
            if(m_pending.empty())return;
            auto batch=std::move(m_pending);m_pending.clear();lock.unlock();
            std::map<std::string,nlohmann::json> failed;
            for(auto& [id,snapshot]:batch)try{m_sink(id,snapshot);reported.erase(id);}
            catch(const std::exception& error){
                failed.emplace(id,std::move(snapshot));
                if(reported.insert(id).second)try{m_failure(id,error.what());}catch(...){}
            }catch(...){
                failed.emplace(id,std::move(snapshot));
                if(reported.insert(id).second)try{m_failure(id,"Unknown trace export failure");}catch(...){}
            }
            lock.lock();
            if(!m_stopping&&!failed.empty()){
                for(auto& [id,snapshot]:failed)if(!m_pending.contains(id))m_pending.emplace(id,std::move(snapshot));
                m_ready.wait_for(lock,std::chrono::milliseconds(500),[&]{return m_stopping;});
            }
        }
    }
};
}
