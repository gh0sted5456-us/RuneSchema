#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>
#include <algorithm>

namespace PS::RuneSchemaData {
namespace fs=std::filesystem;
inline void Safe(const fs::path& path){
    for(auto current=fs::absolute(path);!current.empty();){
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)){
            const auto name=current.generic_u8string();
            throw std::runtime_error("RuneSchema data refuses a redirected child path: "+std::string(name.begin(),name.end()));
        }
        const auto parent=current.parent_path();if(parent==current)break;current=parent;
    }
}
inline fs::path LocalDirectory(){
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if(!length||length>32768)throw std::runtime_error("LOCALAPPDATA is unavailable");
    std::vector<wchar_t> value(length);
    if(GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),length)+1!=length)throw std::runtime_error("LOCALAPPDATA changed while being read");
    return fs::path(value.data());
}
inline fs::path GameSavedDirectory(){return LocalDirectory()/L"RSDragonwilds"/L"Saved";}
inline fs::path Resolve(const fs::path& local){
    if(!local.is_absolute())throw std::runtime_error("LOCALAPPDATA must name an absolute directory");
    // Resolve the trusted application root once; child cache/settings paths still reject links.
    std::error_code error;const auto destination=fs::weakly_canonical(local/L"RSDragonwilds"/L"RuneSchema",error);
    if(error)throw std::runtime_error("Cannot resolve the RuneSchema data root: "+error.message());
    Safe(destination);return destination;
}
// Exact on-disk spelling of a child folder, so a case-only rename can be detected on Windows.
inline std::wstring ActualName(const fs::path& root,const std::wstring& name){
    std::error_code error;
    for(fs::directory_iterator it(root,error),end;!error&&it!=end;it.increment(error)){
        const auto found=it->path().filename().wstring();
        if(found.size()==name.size()&&CompareStringOrdinal(found.c_str(),int(found.size()),name.c_str(),int(name.size()),TRUE)==CSTR_EQUAL)return found;
    }
    return {};
}
inline bool PlainDirectory(const fs::path& path){
    const auto attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}
// Moves one legacy folder to its new name. Best effort: a folder in use (a running game or Helpy) stays where it
// is and keeps being used through the location fallbacks below until a later start can move it.
inline void MoveLegacyFolder(const fs::path& root,const std::wstring& from,const fs::path& to){
    const auto actual=ActualName(root,from);if(actual.empty())return;
    const auto source=root/actual;if(!PlainDirectory(source))return;
    std::error_code error;const auto target=root/to;
    if(to.has_parent_path()){
        // A folder moving under core never replaces an established core folder.
        if(fs::exists(target,error))return;
        fs::create_directories(target.parent_path(),error);if(error)return;
        fs::rename(source,target,error);return;
    }
    const auto wanted=to.wstring();const auto existing=ActualName(root,wanted);
    if(existing==wanted)return;
    if(!existing.empty()&&existing!=actual)return; // a different folder already owns the new name
    if(existing.empty()){fs::rename(source,target,error);return;}
    // Case-only rename (server -> Server): Windows needs a temporary name in between.
    const auto temporary=root/(actual+L".migrating");if(fs::exists(temporary,error))return;
    fs::rename(source,temporary,error);if(error)return;
    fs::rename(temporary,target,error);if(error)fs::rename(temporary,source,error);
}
// Folder names for each runtime. Steam and Epic share the Win64 binaries, Game Pass uses WinGDK, and a dedicated
// server keeps its own folder so it never clashes with a client on the same PC.
inline std::wstring PlatformFolder(const std::string& key){
    if(key=="steam"||key=="epic")return L"Win64";
    if(key=="gamepass")return L"WinGDK";
    if(key=="server")return L"Server";
    return {};
}
inline void Migrate(const fs::path& root){
    if(!PlainDirectory(root))return;
    for(const auto* name:{L"HELPY",L"runeschema.profile",L"health"})MoveLegacyFolder(root,name,fs::path(L"core")/name);
    for(const auto* key:{"steam","epic","gamepass","server"}){const std::string text(key);MoveLegacyFolder(root,std::wstring(text.begin(),text.end()),PlatformFolder(text));}
}
inline const fs::path& Directory(){static const fs::path directory=[]{auto root=Resolve(LocalDirectory());try{Migrate(root);}catch(...){}return root;}();return directory;}
inline fs::path SharedDirectoryLocation(const fs::path& root,const std::string& name){
    if(name!="HELPY"&&name!="runeschema.profile"&&name!="health")throw std::runtime_error("Unknown RuneSchema shared folder");
    const auto destination=root/"core"/name,existing=root/name;
    // Reuse established AppData profiles/settings in place, without moving them.
    return !fs::exists(destination)&&fs::is_directory(existing)?existing:destination;
}
inline fs::path SharedDirectory(const fs::path& root,const std::string& name){
    const auto path=SharedDirectoryLocation(root,name);
    Safe(path);return path;
}
inline fs::path SharedDirectory(const std::string& name){return SharedDirectory(Directory(),name);}
inline fs::path ResolveSaveRoot(const fs::path& directory){
    // The caller chooses the save root. Resolve it before checking any generated children.
    const auto resolved=fs::weakly_canonical(directory);
    Safe(resolved);
    return resolved;
}
inline fs::path CharacterCopiesDirectory(const fs::path& root=Directory()){
    return ResolveSaveRoot(SharedDirectoryLocation(root,"runeschema.profile")/"characters");
}
inline std::string RuntimeKey(std::string platform){
    std::transform(platform.begin(),platform.end(),platform.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):char(c);});
    if(platform=="steam"||platform=="epic"||platform=="gamepass"||platform=="server")return platform;
    if(platform=="unknown")return "core/runtime/unknown";
    throw std::runtime_error("Unknown RuneSchema runtime folder");
}
// The runtime folder under the AppData root. An unmigrated legacy folder (steam, epic, gamepass, server) is used
// in place while its new folder does not exist yet.
inline fs::path PlatformDirectory(const fs::path& root,const std::string& platform){
    const auto key=RuntimeKey(platform);const auto folder=PlatformFolder(key);
    if(folder.empty())return root/key;
    const auto destination=root/folder;
    if(!ActualName(root,folder).empty())return destination;
    const auto legacy=ActualName(root,std::wstring(key.begin(),key.end()));
    return !legacy.empty()&&PlainDirectory(root/legacy)?root/legacy:destination;
}
inline fs::path PlatformDirectory(const std::string& platform){return PlatformDirectory(Directory(),platform);}
inline fs::path MappingDirectory(const fs::path& root,const std::string& platform){
    const auto path=PlatformDirectory(root,platform)/"cache"/"mappings";Safe(path);return path;
}
inline fs::path MappingDirectory(const std::string& platform){return MappingDirectory(Directory(),platform);}
}
