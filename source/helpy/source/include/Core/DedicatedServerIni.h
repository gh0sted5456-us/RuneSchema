#pragma once
#include "Core/LiveLog.h"
#include <stdexcept>

namespace PS::DedicatedServerIni {
inline std::string Trim(std::string_view value){const auto begin=value.find_first_not_of(" \t\r");if(begin==value.npos)return {};const auto end=value.find_last_not_of(" \t\r");return std::string(value.substr(begin,end-begin+1));}
inline bool Sensitive(std::string_view key){const auto s=LiveLog::Lower(key);return s.find("pass")!=s.npos||s.find("token")!=s.npos||s.find("secret")!=s.npos||s.find("key")!=s.npos||s.find("credential")!=s.npos||s.find("auth")!=s.npos;}
inline bool Writable(std::string_view section,std::string_view key){if(LiveLog::Lower(section)!="/script/dominion.dedicatedserversettings")return false;
    const auto s=LiveLog::Lower(key);return s=="ownerid"||s=="servername"||s=="defaultworldname"||s=="adminpassword"||s=="worldpassword";}
inline bool Managed(std::string key){key=LiveLog::Lower(Trim(key));while(!key.empty()&&(key[0]=='+'||key[0]=='-'||key[0]=='.'||key[0]=='!'))key.erase(0,1);
    return key=="knownplayerlist"||key=="adminlist"||key=="banneduserlist"||key=="serverguid";}
inline const char* Description(std::string_view key){const auto s=LiveLog::Lower(key);
    if(s=="ownerid")return "Owner account/player ID from the in-game Settings screen; not a character GUID. Required by the official guide.";
    if(s=="servername")return "Server display name. Does not select a different saved world.";
    if(s=="defaultworldname")return "Name for a newly created default world; not a command to replace an existing save.";
    if(s=="adminpassword")return "Password for the game's Server Management access. Native Admin is separate from RuneSchema action permissions.";
    if(s=="worldpassword")return "Optional world join password; empty allows joining without this password. Overrides the world's own password.";
    if(s=="serverguid")return "Runtime-managed server identity. Preserved; not an editable permission.";
    if(s=="platformpolicy")return "Reflected EDomPlatformFamily property. Allowed values/config binding need validation; preserved read-only.";
    if(s=="ballowsendingcrashdumps")return "Reflected crash-reporting property; not a player permission. Preserved read-only pending config-binding validation.";
    if(Managed(std::string(key)))return "Native account/admin/ban records may embed credentials. Redacted and preserved; not editable as raw tuples.";
    return "Observed or unknown key. Preserved read-only; presence in a sample does not establish game support.";
}
struct Line {std::string Original,Ending,Section,Key,Prefix,Value,Initial;bool Editable=false;};
struct Document {
    std::vector<Line> Lines;
    explicit Document(std::string_view contents){
        if(contents.size()>256*1024||contents.find('\0')!=contents.npos||contents.starts_with("\xff\xfe")||contents.starts_with("\xfe\xff"))throw std::runtime_error("Only text INI files up to 256 KiB are supported. UTF-16 must be edited externally.");
        std::string section;size_t offset=0;
        while(offset<contents.size()){
            const auto end=contents.find('\n',offset);const auto count=end==contents.npos?contents.size()-offset:end-offset;
            Line line;line.Original=contents.substr(offset,count);line.Ending=end==contents.npos?"":"\n";
            if(!line.Original.empty()&&line.Original.back()=='\r'){line.Original.pop_back();line.Ending="\r"+line.Ending;}
            auto trimmed=Trim(line.Original);if(offset==0&&trimmed.starts_with("\xef\xbb\xbf"))trimmed.erase(0,3);
            if(trimmed.starts_with('[')&&trimmed.ends_with(']'))section=trimmed.substr(1,trimmed.size()-2);
            else if(!trimmed.empty()&&trimmed[0]!=';'&&trimmed[0]!='#'){
                const auto equals=line.Original.find('=');if(equals!=std::string::npos){line.Key=Trim(std::string_view(line.Original).substr(0,equals));
                    if(!line.Key.empty()){line.Editable=true;line.Prefix=line.Original.substr(0,equals+1);line.Initial=line.Value=line.Original.substr(equals+1);}}
            }
            line.Section=section;Lines.push_back(std::move(line));if(end==contents.npos)break;offset=end+1;
        }
    }
    bool Dirty()const{for(const auto& line:Lines)if(line.Editable&&line.Value!=line.Initial)return true;return false;}
    void ValidateChanges()const{for(const auto& line:Lines)if(line.Editable&&line.Value!=line.Initial){
        if(!Writable(line.Section,line.Key))throw std::runtime_error("Only the five documented scalar server settings can be staged.");
        size_t count=0;for(const auto& other:Lines)if(other.Editable&&LiveLog::Lower(other.Section)==LiveLog::Lower(line.Section)&&LiveLog::Lower(other.Key)==LiveLog::Lower(line.Key))++count;
        if(count!=1)throw std::runtime_error("A reviewed scalar key has multiple declarations. Resolve the ambiguity externally first.");
        if(line.Value.size()>8192||line.Value.find_first_of("\r\n")!=std::string::npos||line.Value.find('\0')!=std::string::npos)throw std::runtime_error("Invalid server scalar value.");}}
    std::string Serialize()const{std::string result;for(const auto& line:Lines){
        if(line.Value.find_first_of("\r\n")!=std::string::npos||line.Value.find('\0')!=std::string::npos)throw std::runtime_error("INI values cannot contain new lines or NUL bytes.");
        result+=line.Editable&&line.Value!=line.Initial?line.Prefix+line.Value:line.Original;result+=line.Ending;}if(result.size()>256*1024)throw std::runtime_error("Edited INI exceeds 256 KiB.");return result;}
};
#ifdef _WIN32
struct Handle {HANDLE Value=INVALID_HANDLE_VALUE;~Handle(){if(Value!=INVALID_HANDLE_VALUE)CloseHandle(Value);}};
inline std::string ReadHandle(HANDLE handle){LARGE_INTEGER size{};if(!GetFileSizeEx(handle,&size)||size.QuadPart>256*1024||size.QuadPart<0)throw std::runtime_error("INI is unreadable or exceeds 256 KiB.");
    std::string contents(static_cast<size_t>(size.QuadPart),'\0');DWORD read=0;if(!ReadFile(handle,contents.data(),static_cast<DWORD>(contents.size()),&read,nullptr)||read!=contents.size())throw std::runtime_error("Could not read the complete INI.");return contents;}
inline bool ValidPath(const std::filesystem::path& path){return LiveLog::Lower(path.filename().string())=="dedicatedserver.ini"&&LiveLog::Lower(path.parent_path().filename().string())=="windowsserver";}
inline std::string Read(const std::filesystem::path& path){if(!ValidPath(path))throw std::runtime_error("Select DedicatedServer.ini inside Config/WindowsServer.");
    Handle file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.Value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open this server INI. Select the active profile's existing file.");return ReadHandle(file.Value);}
// Compare against the loaded bytes while excluding competing writers. ReplaceFile
// preserves the old file as a unique backup; nothing applies to a running server.
inline std::filesystem::path Save(const std::filesystem::path& path,std::string_view expected,std::string_view contents){
    if(!ValidPath(path))throw std::runtime_error("Not a WindowsServer/DedicatedServer.ini path.");
    const Document check(contents);(void)check;
    Handle original{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(original.Value==INVALID_HANDLE_VALUE)throw std::runtime_error("INI is unavailable or being written. No changes were saved.");
    if(ReadHandle(original.Value)!=expected)throw std::runtime_error("INI changed on disk. Reload before saving; your edits were not written.");
    const auto suffix=L".helpy-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    auto temporary=path;temporary+=suffix+L".tmp";auto backup=path;backup+=suffix+L".bak";
    if(std::filesystem::exists(backup))throw std::runtime_error("Backup filename already exists; retry saving.");
    {Handle output{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(output.Value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create an adjacent temporary file. Check directory permissions.");
        DWORD written=0;const bool ok=WriteFile(output.Value,contents.data(),static_cast<DWORD>(contents.size()),&written,nullptr)&&written==contents.size()&&FlushFileBuffers(output.Value);
        if(!ok){CloseHandle(output.Value);output.Value=INVALID_HANDLE_VALUE;DeleteFileW(temporary.c_str());throw std::runtime_error("Writing the INI failed. Original unchanged.");}}
    if(!ReplaceFileW(path.c_str(),temporary.c_str(),backup.c_str(),0,nullptr,nullptr)){DeleteFileW(temporary.c_str());throw std::runtime_error("Atomic replacement failed. Check the original INI and adjacent backup before retrying.");}
    return backup;
}
#endif
}
