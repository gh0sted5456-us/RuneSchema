#pragma once
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>

namespace PS::DiagnosticLog {
// File-only sink: no engine or UE4SS calls from its flushing thread.
class File {
    std::mutex m_gate;
    std::condition_variable m_changed;
    std::ofstream m_stream;
    std::thread m_worker;
    std::filesystem::path m_path;
    std::chrono::steady_clock::time_point m_deadline{};
    std::size_t m_pending=0, m_bytes=0, m_limit;
    bool m_stop=true, m_failed=false;

    void Fail() { m_failed=true; m_stream.close(); m_pending=0; }
    bool Rotate() {
        m_stream.close();
        auto previous=m_path; previous += ".previous";
        std::error_code error;
        if(std::filesystem::exists(previous,error))std::filesystem::remove(previous,error);
        if(error){Fail();return false;}
        std::filesystem::rename(m_path,previous,error);
        if(error){Fail();return false;}
        m_stream.open(m_path,std::ios::binary|std::ios::trunc);
        m_bytes=0;m_pending=0;
        if(!m_stream){Fail();return false;}return true;
    }
    void Flush() {
        if(m_stream.is_open()){m_stream.flush();if(!m_stream)Fail();}
        m_pending=0;
    }
public:
    explicit File(std::size_t limit=8*1024*1024):m_limit(limit){}
    File(const File&)=delete;File& operator=(const File&)=delete;
    ~File(){Close();}
    bool Open(const std::filesystem::path& path) noexcept {
        Close();
        try {
            std::lock_guard lock(m_gate);
            m_path=path;m_failed=false;m_stop=false;
            // The caller supplies an existing mod root; do not create folders
            // or silently redirect to a different installation.
            std::error_code error;
            m_bytes=std::filesystem::exists(path,error)?std::filesystem::file_size(path,error):0;
            if(error){Fail();return false;}
            m_stream.clear();m_stream.open(path,std::ios::binary|std::ios::app);
            if(!m_stream){Fail();return false;}
            if(m_bytes>=m_limit&&!Rotate())return false;
            m_worker=std::thread([this]{
                std::unique_lock lock(m_gate);
                while(!m_stop){
                    m_changed.wait(lock,[this]{return m_stop||m_pending!=0;});
                    if(m_stop)break;
                    if(!m_changed.wait_until(lock,m_deadline,[this]{return m_stop||m_pending==0;}))Flush();
                }
                Flush();
            });
            return true;
        }catch(...){std::lock_guard lock(m_gate);Fail();return false;}
    }
    bool Write(std::string_view text,bool urgent=false) noexcept {
        try {
            std::lock_guard lock(m_gate);
            if(m_stop||!m_stream.is_open())return false;
            if(m_bytes>=m_limit&&!Rotate())return false;
            const auto now=std::chrono::system_clock::now();
            const auto time=std::chrono::system_clock::to_time_t(now);
            std::tm utc{};
#ifdef _WIN32
            gmtime_s(&utc,&time);
#else
            gmtime_r(&time,&utc);
#endif
            std::ostringstream prefix;
            prefix<<'['<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%S")<<'.'
                <<std::setfill('0')<<std::setw(3)
                <<std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000
                <<"Z thread="<<std::this_thread::get_id()<<"] ";
            const auto stamp=prefix.str();
            m_stream.write(stamp.data(),stamp.size());m_stream.write(text.data(),text.size());
            const bool newline=text.empty()||text.back()!='\n';if(newline)m_stream.put('\n');
            m_bytes+=stamp.size()+text.size()+newline;
            if(!m_stream){Fail();return false;}
            if(!m_pending)m_deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
            if(++m_pending>=64||urgent)Flush();
            m_changed.notify_one();return !m_failed;
        }catch(...){std::lock_guard lock(m_gate);Fail();return false;}
    }
    // Exactly one caller consumes a sink failure, preventing console floods.
    bool TakeFailure() noexcept {std::lock_guard lock(m_gate);const auto failed=m_failed;m_failed=false;return failed;}
    void Close() noexcept {
        {std::lock_guard lock(m_gate);m_stop=true;m_changed.notify_one();}
        if(m_worker.joinable())m_worker.join();
        std::lock_guard lock(m_gate);Flush();m_stream.close();
    }
};
}
