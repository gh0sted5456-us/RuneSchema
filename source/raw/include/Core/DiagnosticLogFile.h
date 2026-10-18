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
#ifdef _WIN32
#include <windows.h>
#endif

namespace PS::DiagnosticLog {
enum class Session { Fresh, ArchivePrevious, Append };
// File-only sink: no engine or UE4SS calls from its flushing thread.
class File {
    std::mutex m_gate;
    std::condition_variable m_changed;
    std::ofstream m_stream;
    std::thread m_worker;
    std::filesystem::path m_path;
    std::chrono::steady_clock::time_point m_deadline{};
    std::size_t m_pending=0;
    bool m_stop=true, m_failed=false;

    void Fail() { m_failed=true; m_stream.close(); m_pending=0; }
    bool ArchivePrevious() {
        const auto directory=m_path.parent_path()/"logs";
        std::error_code error;
        if(std::filesystem::exists(directory,error)){
            if(error||std::filesystem::is_symlink(std::filesystem::symlink_status(directory,error))||!std::filesystem::is_directory(directory,error)||error){Fail();return false;}
        }else if(error||!std::filesystem::create_directory(directory,error)||error){Fail();return false;}
        const auto now=std::chrono::system_clock::now();const auto time=std::chrono::system_clock::to_time_t(now);std::tm utc{};
#ifdef _WIN32
        gmtime_s(&utc,&time);
#else
        gmtime_r(&time,&utc);
#endif
        std::ostringstream group;group<<m_path.stem().string()<<'-'<<std::put_time(&utc,"%Y%m%d-%H%M%S")<<'-'<<std::setfill('0')<<std::setw(3)
            <<std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000<<'Z';
        for(unsigned suffix=0;suffix<1000;++suffix){
            const auto archive=directory/(group.str()+(suffix?"-"+std::to_string(suffix):std::string{})+".log");
            if(std::filesystem::exists(archive,error)){if(error){Fail();return false;}continue;}
            if(error){Fail();return false;}
            // No replacement flag: a racing or colliding archive is never lost.
            // A failed move must not truncate the previous active log.
#ifdef _WIN32
            const auto native=[](const std::filesystem::path& path){auto value=std::filesystem::absolute(path).lexically_normal().native();
                if(value.starts_with(L"\\\\?\\"))return value;
                return value.starts_with(L"\\\\")?std::wstring(L"\\\\?\\UNC\\")+value.substr(2):std::wstring(L"\\\\?\\")+value;};
            if(!MoveFileExW(native(m_path).c_str(),native(archive).c_str(),MOVEFILE_WRITE_THROUGH)){
                const auto code=GetLastError();if(code==ERROR_ALREADY_EXISTS||code==ERROR_FILE_EXISTS)continue;
                Fail();return false;
            }
#else
            if(!std::filesystem::copy_file(m_path,archive,std::filesystem::copy_options::none,error)){
                if(error==std::errc::file_exists)continue;Fail();return false;
            }
            if(!std::filesystem::remove(m_path,error)||error){Fail();return false;}
#endif
            return true;
        }
        Fail();return false;
    }
    void Flush() {
        if(m_stream.is_open()){m_stream.flush();if(!m_stream)Fail();}
        m_pending=0;
    }
public:
    File()=default;
    File(const File&)=delete;File& operator=(const File&)=delete;
    ~File(){Close();}
    bool Open(const std::filesystem::path& path,Session session=Session::Fresh) noexcept {
        Close();
        try {
            std::lock_guard lock(m_gate);
            m_path=path;m_failed=false;
            // The caller supplies an existing mod root; do not create folders
            // or silently redirect to a different installation.
            std::error_code error;
            const auto status=std::filesystem::symlink_status(path,error);
            if(error==std::errc::no_such_file_or_directory)error.clear();
            const bool exists=std::filesystem::exists(status);
            if(error||std::filesystem::is_symlink(status)){Fail();return false;}
            if(exists){
                if(!std::filesystem::is_regular_file(status)){Fail();return false;}
                if(session==Session::ArchivePrevious){
                    const auto bytes=std::filesystem::file_size(path,error);
                    if(error){Fail();return false;}
                    if(bytes&&!ArchivePrevious())return false;
                }
            }
            m_stream.clear();m_stream.open(path,std::ios::binary|(session==Session::Append?std::ios::app:std::ios::trunc));
            if(!m_stream){Fail();return false;}
            m_stop=false;
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
