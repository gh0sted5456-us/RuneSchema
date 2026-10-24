#pragma once
// Filesystem-only maintenance. Never run a cache walk, sort or eviction on the
// game thread. Worker inputs are owned values, not engine/world references.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <mutex>
#include <vector>
#include <windows.h>

namespace PS::IconDiskMaintenance {
struct Request {
    std::filesystem::path Root,ProtectedFile;
    std::uintmax_t Limit=0;
    bool ClearOldSession=false;
    std::filesystem::file_time_type Before=std::filesystem::file_time_type::clock::now();
};
inline void Run(const Request& request) {
    namespace fs=std::filesystem;
    if(request.Root.empty()||request.ProtectedFile.empty())return;
    std::error_code ec;
    const auto root=fs::absolute(request.Root,ec).lexically_normal();
    if(ec||root.empty()||root==root.root_path()||!request.Limit)return;
    // All ancestors must be real directories, not junctions into another tree.
    for(auto parent=root;!parent.empty()&&parent!=parent.root_path();parent=parent.parent_path()){
        const auto attr=GetFileAttributesW(parent.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return;
    }
    const auto protectedFile=fs::absolute(request.ProtectedFile,ec).lexically_normal();if(ec)return;
    struct Entry {fs::path File;fs::file_time_type Used;std::uintmax_t Size;};
    std::vector<Entry> entries;std::uintmax_t total=0;
    for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
        const auto attr=GetFileAttributesW(it->path().c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT)){it.disable_recursion_pending();continue;}
        if(!it->is_regular_file(ec)||ec||it->path().extension()!=L".rsicon"){ec.clear();continue;}
        const auto size=it->file_size(ec);if(ec){ec.clear();continue;}
        const auto used=it->last_write_time(ec);if(ec){ec.clear();continue;}
        const auto path=it->path().lexically_normal();
        if(path==protectedFile){fs::last_write_time(path,fs::file_time_type::clock::now(),ec);ec.clear();total+=size;continue;}
        if(request.ClearOldSession&&used<request.Before){fs::remove(path,ec);if(!ec)continue;ec.clear();}
        total+=size;entries.push_back({path,used,size});
    }
    if(total<=request.Limit)return;
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.Used<b.Used;});
    const auto target=request.Limit-request.Limit/20;
    for(const auto& entry:entries){
        if(total<=target)break;
        // A file may have been replaced by an import/capture since enumeration.
        const auto attr=GetFileAttributesW(entry.File.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))continue;
        if(fs::last_write_time(entry.File,ec)!=entry.Used||ec){ec.clear();continue;}
        fs::remove(entry.File,ec);if(!ec)total-=std::min(total,entry.Size);else ec.clear();
    }
}
class Worker {
    std::mutex Mutex;
    bool Stopped=false;
    std::future<void> Active;
    std::chrono::steady_clock::time_point Next{};
    std::filesystem::path Root;
    std::uint64_t Epoch=0;
    std::uintmax_t Limit=0;
public:
    ~Worker(){Shutdown();}
    void Shutdown() noexcept {
        try{std::lock_guard lock(Mutex);Stopped=true;if(Active.valid())try{Active.get();}catch(...){}}catch(...){}
    }
    void Post(Request request,std::uint64_t epoch) noexcept {
        try {
            std::lock_guard lock(Mutex);if(Stopped)return;
            if(Active.valid()){
                if(Active.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
                try{Active.get();}catch(...){} // Cache housekeeping is best effort.
            }
            const auto now=std::chrono::steady_clock::now();
            if(now<Next&&Root==request.Root&&Epoch==epoch&&Limit==request.Limit)return;
            // Never capture this object; shutdown only joins filesystem work.
            Active=std::async(std::launch::async,[request]{Run(request);});
            Next=now+std::chrono::seconds(30);Root=request.Root;Epoch=epoch;Limit=request.Limit;
        }catch(...){} // A failed worker launch must not interrupt item delivery.
    }
};
}
