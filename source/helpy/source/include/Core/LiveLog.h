#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace PS::LiveLog {
enum class Level { Info, Diagnostic, Warning, Error };
inline const char* Label(Level level){switch(level){case Level::Diagnostic:return "Diagnostic";case Level::Warning:return "Warning";case Level::Error:return "Error";default:return "Info";}}
inline std::string Lower(std::string_view value){std::string result(value);for(auto& c:result)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return result;}
struct Guide {std::string_view Code,Anchor,Title;};
inline constexpr std::array Guides{
    Guide{"RS-QST-001","rs-qst-001","Conflicting quest ownership"},
    Guide{"RS-REG-001","rs-reg-001","Registry enrollment deferred or rejected"},
    Guide{"RS-NET-001","rs-net-001","Client/server registry mismatch"},
    Guide{"RS-WEAR-001","rs-wear-001","Wearable contract not enrolled"},
    Guide{"RS-PERF-001","rs-perf-001","Slow runtime work"},
    Guide{"RS-CAT-001","rs-cat-001","Item catalogue unavailable or expired"},
    Guide{"RS-SPAWN-001","rs-spawn-001","Spawn or placement waiting for collision"},
    Guide{"RS-LOG-000","rs-log-000","Reading an unfamiliar diagnostic"}
};
inline std::string HelpUrl(const Guide& guide){return "https://gh0sted5456-us.github.io/RuneSchema/TROUBLESHOOTING/#"+std::string(guide.Anchor);}
struct Entry {std::uint64_t Id{};Level Severity=Level::Info;std::string Text,Search,Scope;int Help=-1;};
inline Entry Parse(std::string text){
    Entry entry;entry.Text=std::move(text);entry.Search=Lower(entry.Text);const auto& s=entry.Search;
    if(s.find("[error]")!=s.npos||s.find("[fatal]")!=s.npos)entry.Severity=Level::Error;
    else if(s.find("[warning]")!=s.npos||s.find("[warn]")!=s.npos)entry.Severity=Level::Warning;
    else if(s.find("[diagnostic]")!=s.npos||s.find("[verbose]")!=s.npos)entry.Severity=Level::Diagnostic;
    const auto tag=s.find("[runeschema]");
    if(tag!=s.npos){auto begin=tag+12;
        while(begin<s.size()){while(begin<s.size()&&s[begin]==' ')++begin;if(begin>=s.size()||s[begin]!='[')break;const auto end=s.find(']',begin);if(end==s.npos)break;
            auto token=entry.Text.substr(begin+1,end-begin-1);const auto lower=Lower(token);
            if(lower!="warning"&&lower!="warn"&&lower!="error"&&lower!="diagnostic"&&lower!="verbose"&&lower!="summary"){entry.Scope=std::move(token);break;}begin=end+1;}}
    for(size_t i=0;i<Guides.size();++i)if(s.find(Lower(Guides[i].Code))!=s.npos){entry.Help=static_cast<int>(i);return entry;}
    if(s.find("duplicate quest owner")!=s.npos||s.find("ownership conflict")!=s.npos||s.find("duplicate quest persistence")!=s.npos)entry.Help=0;
    else if(s.find("mismatch")!=s.npos&&(s.find("registry")!=s.npos||s.find("fingerprint")!=s.npos||s.find("network")!=s.npos))entry.Help=2;
    else if(s.find("wearable-bridge")!=s.npos&&s.find("not-enrolled")!=s.npos)entry.Help=3;
    else if(s.find("slow-work")!=s.npos)entry.Help=4;
    else if(s.find("catalog")!=s.npos&&(s.find("expired")!=s.npos||s.find("unavailable")!=s.npos))entry.Help=5;
    else if((s.find("spawn")!=s.npos||s.find("placement")!=s.npos)&&(s.find("collision")!=s.npos||s.find("not confirmed placed")!=s.npos))entry.Help=6;
    else if(s.find("registry")!=s.npos&&(s.find("deferred")!=s.npos||s.find("rejected")!=s.npos||s.find("not certified")!=s.npos))entry.Help=1;
    else if(entry.Severity==Level::Warning||entry.Severity==Level::Error)entry.Help=7;
    return entry;
}
struct Filter {
    std::array<bool,4> Levels{true,true,true,true};std::string Query,Scope;
    bool Matches(const Entry& entry)const{return Levels[static_cast<size_t>(entry.Severity)]&&(Scope.empty()||entry.Scope==Scope)&&(Query.empty()||entry.Search.find(Query)!=std::string::npos||(entry.Help>=0&&Lower(Guides[entry.Help].Code).find(Query)!=std::string::npos));}
};
class History {
    std::uint64_t m_next=1;size_t m_bytes=0;
public:
    std::deque<Entry> Rows;size_t Dropped=0;std::uint64_t Revision=0;
    void Clear(){Rows.clear();m_bytes=0;Dropped=0;++Revision;}
    void Append(std::string text){auto entry=Parse(std::move(text));entry.Id=m_next++;m_bytes+=entry.Text.size();Rows.push_back(std::move(entry));
        while(Rows.size()>4000||m_bytes>2*1024*1024){m_bytes-=Rows.front().Text.size();Rows.pop_front();++Dropped;}++Revision;}
    std::string Export(const Filter& filter)const{std::string output;for(const auto& row:Rows)if(filter.Matches(row)){if(row.Help>=0)output+='['+std::string(Guides[row.Help].Code)+"] ";output+=row.Text;output+='\n';}return output;}
};
struct Cursor {std::uint64_t Offset=0,Identity=0;std::string Partial;bool Started=false,Discard=false;};
struct Batch {Cursor Next;std::vector<std::string> Lines;std::string Error;bool Reset=false,Skipped=false,More=false;};
// Runs only on the viewer's background I/O task. Never touches engine objects.
inline Batch Read(const std::filesystem::path& path,Cursor cursor){
    Batch batch;batch.Next=cursor;
#ifdef _WIN32
    const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE){batch.Error=GetLastError()==ERROR_FILE_NOT_FOUND?"RuneSchema.log has not been created for this installation yet.":"Cannot read RuneSchema.log; check folder permissions.";return batch;}
    struct Close {HANDLE Handle;~Close(){CloseHandle(Handle);}}close{handle};
    BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(handle,&info)){batch.Error="Cannot inspect the log file.";return batch;}
    const auto size=(std::uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    const auto identity=(std::uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow;
    auto& next=batch.Next;
    if(!next.Started||identity!=next.Identity||size<next.Offset){batch.Reset=next.Started;next={};next.Started=true;next.Identity=identity;
        if(size>1024*1024){next.Offset=size-1024*1024;next.Discard=true;batch.Skipped=true;}}
    const auto count=static_cast<DWORD>(std::min<std::uint64_t>(256*1024,size-next.Offset));
    if(!count)return batch;
    LARGE_INTEGER offset{};offset.QuadPart=next.Offset;if(!SetFilePointerEx(handle,offset,nullptr,FILE_BEGIN)){batch.Error="Cannot seek in the log file.";return batch;}
    std::string bytes(count,'\0');DWORD read=0;
    if(!ReadFile(handle,bytes.data(),count,&read,nullptr)){batch.Error="Reading RuneSchema.log failed; retrying shortly.";return batch;}
    bytes.resize(read);next.Offset+=read;batch.More=next.Offset<size;
    for(const char c:bytes){
        if(c=='\n'){
            if(!next.Discard){if(!next.Partial.empty()&&next.Partial.back()=='\r')next.Partial.pop_back();if(!next.Partial.empty())batch.Lines.push_back(std::move(next.Partial));}
            next.Partial.clear();next.Discard=false;
        }else if(!next.Discard){if(next.Partial.size()<16384)next.Partial+=c;else{next.Partial+=" [line clipped at 16 KiB]";batch.Lines.push_back(std::move(next.Partial));next.Partial.clear();next.Discard=true;}}
    }
#else
    batch.Error="The live file reader requires Windows.";
#endif
    return batch;
}
}
