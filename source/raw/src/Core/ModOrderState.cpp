#include "Loader/ModOrderState.h"
#include "Loader/ModOrderPolicy.h"
#include "Core/ConfigFiles.h"
#include <cstdint>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace DragonWilds::ModOrderState {
namespace {
std::string_view Trim(std::string_view value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    return first==value.npos?std::string_view{}:value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}
bool ValidName(std::string_view name) {
    return !name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\:*?\"<>|")==name.npos
        &&name.find_first_of(std::string_view("\0\r\n",3))==name.npos;
}
// All first-party writers, including separate clients sharing an installation,
// use the same path-derived Windows mutex. No lock handle is held on the file
// itself during atomic replacement (which would prevent replacement on Windows).
class WriteLock {
#ifdef _WIN32
    HANDLE m_handle=nullptr;
#else
    inline static std::mutex Mutex;
    std::unique_lock<std::mutex> m_lock{Mutex};
#endif
public:
    explicit WriteLock(const std::filesystem::path& path) {
#ifdef _WIN32
        auto key=std::filesystem::weakly_canonical(std::filesystem::absolute(path)).native();
        uint64_t hash=14695981039346656037ull;
        for(auto ch:key){if(ch>=L'A'&&ch<=L'Z')ch+=L'a'-L'A';hash^=static_cast<uint64_t>(ch);hash*=1099511628211ull;}
        const auto name=L"Local\\RuneSchema.ModOrder."+std::to_wstring(hash);
        m_handle=CreateMutexW(nullptr,FALSE,name.c_str());
        if(!m_handle)throw std::runtime_error("Cannot create load-order write lock");
        const auto result=WaitForSingleObject(m_handle,5000);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(m_handle);m_handle=nullptr;throw std::runtime_error("Cannot acquire load-order write lock");}
#endif
    }
    ~WriteLock(){
#ifdef _WIN32
        if(m_handle){ReleaseMutex(m_handle);CloseHandle(m_handle);}
#endif
    }
};
}
std::string Fold(std::string_view value) {
    std::string out(value);for(auto& ch:out)if(ch>='A'&&ch<='Z')ch+=('a'-'A');return out;
}
std::vector<Entry> Parse(std::string_view contents) {
    if(contents.find('\0')!=contents.npos||contents.starts_with("\xff\xfe")||contents.starts_with("\xfe\xff"))
        throw std::runtime_error("Load order must be UTF-8 text, not UTF-16 or binary");
    if(contents.starts_with("\xef\xbb\xbf"))contents.remove_prefix(3);
    std::vector<Entry> result;
    while(!contents.empty()) {
        const auto newline=contents.find('\n');auto line=Trim(contents.substr(0,newline));
        contents=newline==contents.npos?std::string_view{}:contents.substr(newline+1);
        if(line.empty()||line.front()==';'||line.front()=='#')continue;
        const auto colon=line.find(':');if(colon==line.npos)continue;
        const auto name=Trim(line.substr(0,colon));if(!ValidName(name))continue;
        // Only an explicit 1 enables code. Invalid values never become enabled,
        // even with the legacy strictValues preference off; matches Lua startup.
        result.push_back({std::string(name),Trim(line.substr(colon+1))=="1"});
    }
    return result;
}
Plan Reconcile(const std::vector<Entry>& entries,const std::vector<std::string>& discovered) {
    Plan plan;
    std::unordered_map<std::string,std::string> present;
    for(const auto& name:discovered)if(ValidName(name))present.try_emplace(Fold(name),name);
    std::unordered_map<std::string,size_t> known;
    for(const auto& entry:entries) {
        if(!ValidName(entry.Name))continue;
        const auto key=Fold(entry.Name);
        const auto [position,inserted]=known.try_emplace(key,plan.Entries.size());
        if(!inserted){plan.Entries[position->second].Enabled&=entry.Enabled;plan.Changed=true;continue;}
        const auto found=present.find(key);
        // Retain missing rows: a folder being restored must not erase a 0.
        const auto name=found==present.end()?entry.Name:found->second;
        plan.Entries.push_back({name,entry.Enabled});plan.Changed|=name!=entry.Name;
    }
    for(const auto& name:discovered) {
        if(!ValidName(name)||!ModOrderPolicy::ShouldAutoPersist(name)||known.contains(Fold(name)))continue;
        known.emplace(Fold(name),plan.Entries.size());plan.Entries.push_back({name,true});plan.Changed=true;
    }
    std::unordered_set<std::string> emitted;
    const auto append=[&](const std::string& name){if(emitted.insert(Fold(name)).second)plan.Active.push_back(name);};
    for(const auto& name:discovered)
        if(present.contains(Fold(name))&&!known.contains(Fold(name))&&ModOrderPolicy::IsImplicit(name)&&ModOrderPolicy::Priority(name)==0)append(name);
    for(const auto& entry:plan.Entries)
        if(entry.Enabled&&present.contains(Fold(entry.Name)))append(present.at(Fold(entry.Name)));
    for(const auto& name:discovered)
        if(present.contains(Fold(name))&&!known.contains(Fold(name))&&ModOrderPolicy::IsImplicit(name)&&ModOrderPolicy::Priority(name)!=0)append(name);
    return plan;
}
std::string Serialize(const std::vector<Entry>& entries,const Snapshot& previous,bool preserveComments) {
    std::ostringstream output;
    if(preserveComments&&previous.Exists) {
        auto contents=std::string_view(previous.Contents);if(contents.starts_with("\xef\xbb\xbf"))contents.remove_prefix(3);
        while(!contents.empty()) {
            const auto newline=contents.find('\n');auto raw=contents.substr(0,newline);const auto line=Trim(raw);
            if(line.empty()||line.front()==';'||line.front()=='#'||line.find(':')==line.npos)output<<raw<<'\n';
            contents=newline==contents.npos?std::string_view{}:contents.substr(newline+1);
        }
    } else output<<"; RuneSchema mod order - loaded top to bottom.\n; Use 1 to enable and 0 to disable.\n; Missing folders retain their saved choices. Unlisted AA_/ZZ_ folders are implicit.\n";
    // Also normalize explicit UI saves: duplicate aliases cannot override a 0.
    for(const auto& entry:Reconcile(entries,{}).Entries)output<<entry.Name<<" : "<<(entry.Enabled?1:0)<<'\n';
    return output.str();
}
Snapshot Read(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path))return {};
    return {true,PS::ConfigFiles::Read(path,8*1024*1024)};
}
bool WriteExpected(const std::filesystem::path& path,const Snapshot& expected,std::string_view contents) {
    const WriteLock lock(path);
    if(Read(path)!=expected)return false;
    PS::ConfigFiles::Write(path,contents);
    return true;
}
}
