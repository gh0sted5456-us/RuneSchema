#pragma once
#include "Core/DiagnosticCodes.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <cmath>
#include <optional>
#include <nlohmann/json.hpp>
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
using Guide=Diagnostics::Entry;
inline constexpr auto& Guides=Diagnostics::All;
inline std::string HelpUrl(const Guide& guide){return Diagnostics::Url(guide);}
enum class Stream { RuneSchema, UE4SS, Game, EOS };
inline const char* StreamName(Stream stream){switch(stream){case Stream::UE4SS:return "UE4SS";case Stream::Game:return "Game";case Stream::EOS:return "EOS";default:return "RuneSchema";}}
inline const char* FileName(Stream stream){return stream==Stream::UE4SS?"UE4SS.log":stream==Stream::RuneSchema?"RuneSchema.log":"RSDragonwilds.log";}
inline std::filesystem::path LogPath(const std::filesystem::path& mod,Stream stream){if(stream==Stream::UE4SS)return mod.parent_path().parent_path()/FileName(stream);if(stream==Stream::RuneSchema)return mod/FileName(stream);return mod.parent_path().parent_path().parent_path().parent_path().parent_path()/"Saved"/"Logs"/FileName(stream);}
// Called only by a file worker. Shipping builds may not create a game log.
// EOS is a category-filtered view of that log unless the user chooses a file.
inline std::filesystem::path ResolveGameLog(const std::filesystem::path& mod,const std::filesystem::path& local){
    std::vector<std::filesystem::path> folders;if(!local.empty())folders.push_back(local/"RSDragonwilds"/"Saved"/"Logs");folders.push_back(LogPath(mod,Stream::Game).parent_path());
    std::filesystem::path newest;std::filesystem::file_time_type stamp{};bool found=false;
    for(const auto& folder:folders){std::error_code ec;if(!std::filesystem::is_directory(folder,ec))continue;size_t count=0;
        for(std::filesystem::directory_iterator it(folder,ec),end;!ec&&it!=end&&count++<128;it.increment(ec)){
            const auto name=Lower(it->path().filename().string());if(it->is_symlink(ec)||!it->is_regular_file(ec)||it->path().extension()!=L".log"||name.find("backup")!=name.npos||name.find("crash")!=name.npos)continue;
            const auto time=it->last_write_time(ec);if(!ec&&(!found||time>stamp)){found=true;stamp=time;newest=it->path();}
        }
    }return found?newest:(!local.empty()?local/"RSDragonwilds"/"Saved"/"Logs"/"RSDragonwilds.log":LogPath(mod,Stream::Game));
}
inline bool EOSLine(std::string_view text){const auto value=Lower(text);return value.find("logeos")!=value.npos||value.find("[eos]")!=value.npos||value.find("[eossdk]")!=value.npos||value.find("eos_")!=value.npos;}
inline std::filesystem::path ArchivePath(const std::filesystem::path& mod){return mod/"logs";}
// Caller resolves filesystem aliases first; compare normalized Windows names.
inline bool ProtectedExport(const std::filesystem::path& destination,const std::filesystem::path& mod){
    const auto parent=Lower(destination.lexically_normal().parent_path().generic_string()),name=Lower(destination.filename().string());
    if(parent==Lower(ArchivePath(mod).lexically_normal().generic_string())&&name.starts_with("runeschema-")&&name.ends_with(".log"))return true;
    for(const auto stream:{Stream::RuneSchema,Stream::UE4SS,Stream::Game})
        if(parent==Lower(LogPath(mod,stream).lexically_normal().parent_path().generic_string())
            &&(name.starts_with("runeschema.log")||name.starts_with("ue4ss.log")||name.starts_with("ue4ss-backup")||stream==Stream::Game))return true;
    return false;
}
struct Entry {std::uint64_t Id{};Level Severity=Level::Info;std::string Text,Search,Scope,Source,Time,Message,ReportedCode;int Help=-1;bool Suggested=false;};
inline bool CodeToken(std::string_view token){
    if(!token.starts_with("RS-")||token.size()>32)return false;const auto dash=token.find('-',3);
    return dash!=token.npos&&dash>3&&token.size()==dash+4&&std::all_of(token.begin()+3,token.begin()+dash,[](char c){return c>='A'&&c<='Z';})&&std::all_of(token.begin()+dash+1,token.end(),[](char c){return c>='0'&&c<='9';});
}
inline Entry Parse(std::string text){
    Entry entry;entry.Text=std::move(text);entry.Search=Lower(entry.Text);const auto& s=entry.Search;
    size_t body=0;for(unsigned tags=0;tags<16;){while(body<entry.Text.size()&&entry.Text[body]==' ')++body;if(body>=entry.Text.size()||entry.Text[body]!='[')break;const auto end=entry.Text.find(']',body);if(end==entry.Text.npos||end-body>256)break;
        const auto token=entry.Text.substr(body+1,end-body-1);if(!tags&&!token.empty()&&std::isdigit(static_cast<unsigned char>(token.front()))){const auto space=token.find(' ');entry.Time=token.substr(0,space);if(entry.Time.size()>=19&&entry.Time[10]=='T')entry.Time=entry.Time.substr(11,8);}
        if(CodeToken(token)){entry.ReportedCode=token;if(const auto* code=Diagnostics::Find(token))entry.Help=int(code-Guides.data());}
        body=end+1;++tags;
    }entry.Message=entry.Text.substr(body);if(entry.Message.empty())entry.Message=entry.Text;
    if(s.find("[error]")!=s.npos||s.find("[fatal]")!=s.npos||s.find("error:")!=s.npos||s.find("fatal:")!=s.npos)entry.Severity=Level::Error;
    else if(s.find("[warning]")!=s.npos||s.find("[warn]")!=s.npos||s.find("warning:")!=s.npos)entry.Severity=Level::Warning;
    else if(s.find("[diagnostic]")!=s.npos||s.find("[verbose]")!=s.npos)entry.Severity=Level::Diagnostic;
    // UE4SS adds timestamp brackets, then [Lua] [ModName] or [RegisterHook].
    // Only structured tags identify a source, never arbitrary message text.
    for(size_t begin=0;(begin=s.find('[',begin))!=s.npos;){
        const auto end=s.find(']',begin);if(end==s.npos)break;
        auto token=entry.Text.substr(begin+1,end-begin-1);begin=end+1;
        if(token.empty()||token.size()>128||std::isdigit(static_cast<unsigned char>(token.front())))continue;
        const auto lower=Lower(token);
        if(lower=="error"||lower=="fatal"||lower=="warning"||lower=="warn"||lower=="diagnostic"||lower=="verbose"||lower=="info"||CodeToken(token))continue;
        if(entry.Source.empty()){entry.Source=std::move(token);if(lower!="lua")break;}
        else if(Lower(entry.Source)=="lua"){entry.Source=std::move(token);break;}
    }
    const auto tag=s.find("[runeschema]");
    if(tag!=s.npos){auto begin=tag+12;
        while(begin<s.size()){while(begin<s.size()&&s[begin]==' ')++begin;if(begin>=s.size()||s[begin]!='[')break;const auto end=s.find(']',begin);if(end==s.npos)break;
            auto token=entry.Text.substr(begin+1,end-begin-1);const auto lower=Lower(token);
            if(lower!="warning"&&lower!="warn"&&lower!="error"&&lower!="diagnostic"&&lower!="verbose"&&lower!="summary"&&lower!="info"&&!CodeToken(token)){entry.Scope=std::move(token);break;}begin=end+1;}}
    if(entry.Source.empty()&&entry.Message.starts_with("Log")){const auto colon=entry.Message.find(':');if(colon!=std::string::npos&&colon<96&&std::all_of(entry.Message.begin(),entry.Message.begin()+colon,[](unsigned char c){return std::isalnum(c)||c=='_';})){entry.Source=entry.Message.substr(0,colon);entry.Message.erase(0,colon+1);while(!entry.Message.empty()&&entry.Message.front()==' ')entry.Message.erase(0,1);}}
    if(entry.Scope.empty())entry.Scope=entry.Help>=0?std::string(Guides[entry.Help].Subsystem):entry.Source;
    if(!entry.ReportedCode.empty())return entry;
    for(size_t i=0;i<Guides.size();++i)if(s.find(Lower(Guides[i].Code))!=s.npos){entry.Help=static_cast<int>(i);entry.Suggested=true;return entry;}
    if(s.find("duplicate quest owner")!=s.npos||s.find("ownership conflict")!=s.npos||s.find("duplicate quest persistence")!=s.npos)entry.Help=0;
    else if(s.find("mismatch")!=s.npos&&(s.find("registry")!=s.npos||s.find("fingerprint")!=s.npos||s.find("network")!=s.npos))entry.Help=2;
    else if(s.find("wearable-bridge")!=s.npos&&s.find("not-enrolled")!=s.npos)entry.Help=3;
    else if(s.find("slow-work")!=s.npos)entry.Help=4;
    else if(s.find("catalog")!=s.npos&&(s.find("expired")!=s.npos||s.find("unavailable")!=s.npos))entry.Help=5;
    else if((s.find("spawn")!=s.npos||s.find("placement")!=s.npos)&&(s.find("collision")!=s.npos||s.find("not confirmed placed")!=s.npos))entry.Help=6;
    else if(s.find("registry")!=s.npos&&(s.find("deferred")!=s.npos||s.find("rejected")!=s.npos||s.find("not certified")!=s.npos))entry.Help=1;
    else if(entry.Severity==Level::Warning||entry.Severity==Level::Error)entry.Help=7;
    entry.Suggested=entry.Help>=0;return entry;
}
struct ColorRule {
    bool Enabled=true,Source=false;
    std::string Match;
    std::array<float,4> RGBA{1.0f,0.75f,0.25f,1.0f};
};
inline std::vector<ColorRule> ReadColors(const nlohmann::json& document){
    std::vector<ColorRule> result;if(!document.is_array())return result;
    for(const auto& row:document){if(result.size()>=32)break;
        try{ColorRule rule;rule.Match=row.at("match").get<std::string>();const auto kind=row.value("kind",std::string("keyword"));
            if(rule.Match.empty()||rule.Match.size()>128||rule.Match.find_first_of("\r\n\t")!=rule.Match.npos||rule.Match.find('\0')!=rule.Match.npos||(kind!="keyword"&&kind!="source"))continue;
            rule.Source=kind=="source";rule.Enabled=row.value("enabled",true);const auto& color=row.at("color");if(!color.is_array()||color.size()!=4)continue;
            bool valid=true;for(size_t i=0;i<4;++i){rule.RGBA[i]=color[i].get<float>();valid&=std::isfinite(rule.RGBA[i])&&rule.RGBA[i]>=0&&rule.RGBA[i]<=1;}
            if(valid)result.push_back(std::move(rule));
        }catch(...){}
    }return result;
}
inline nlohmann::json WriteColors(const std::vector<ColorRule>& rules){
    auto output=nlohmann::json::array();for(const auto& rule:rules){if(output.size()>=32)break;output.push_back({{"enabled",rule.Enabled},{"kind",rule.Source?"source":"keyword"},{"match",rule.Match},{"color",rule.RGBA}});}return output;
}
inline std::optional<std::array<float,4>> Color(const Entry& entry,const std::vector<ColorRule>& rules){
    // Keywords override source colors; first matching rule within each kind wins.
    for(const bool source:{false,true})for(const auto& rule:rules){
        if(!rule.Enabled||rule.Source!=source||rule.Match.empty())continue;
        const auto match=Lower(rule.Match);
        if(source?(Lower(entry.Source)==match||Lower(entry.Scope)==match):entry.Search.find(match)!=entry.Search.npos)return rule.RGBA;
    }return {};
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
    std::string Export(const Filter& filter)const{std::string output;for(const auto& row:Rows)if(filter.Matches(row)){if(row.Help>=0&&row.Suggested)output+="[Suggested reference "+std::string(Guides[row.Help].Code)+"] ";output+=row.Text;output+='\n';}return output;}
};
struct Group {size_t First,Last,Count;};
inline std::vector<Group> Grouped(const History& history,const Filter& filter,bool combine){
    std::vector<Group> groups;for(size_t i=0;i<history.Rows.size();++i){const auto& row=history.Rows[i];if(!filter.Matches(row))continue;
        if(combine&&!groups.empty()){auto& last=groups.back();const auto& previous=history.Rows[last.Last];if(last.Last+1==i&&previous.Message==row.Message&&previous.Scope==row.Scope&&previous.Severity==row.Severity&&previous.ReportedCode==row.ReportedCode){last.Last=i;++last.Count;continue;}}
        groups.push_back({i,i,1});}return groups;
}
struct Cursor {std::uint64_t Offset=0,Identity=0;std::string Partial;bool Started=false,Discard=false;};
struct Batch {Cursor Next;std::vector<std::string> Lines;std::string Error;bool Reset=false,Skipped=false,More=false;std::filesystem::path File;};
// Runs only on the viewer's background I/O task. Never touches engine objects.
inline Batch Read(const std::filesystem::path& path,Cursor cursor){
    Batch batch;batch.Next=cursor;batch.File=path;
#ifdef _WIN32
    const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE){batch.Error=GetLastError()==ERROR_FILE_NOT_FOUND?path.filename().string()+" has not been created for this installation yet.":"Cannot read "+path.filename().string()+"; check folder permissions.";return batch;}
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
    if(!ReadFile(handle,bytes.data(),count,&read,nullptr)){batch.Error="Reading "+path.filename().string()+" failed; retrying shortly.";return batch;}
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
