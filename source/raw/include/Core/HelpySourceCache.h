#pragma once
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <set>
#include <string>
#include <windows.h>
namespace PS::HelpySourceCache {
struct Snapshot {std::string Key;std::set<std::string> Files;};
inline uint64_t Hash(std::string_view value,uint64_t seed=1469598103934665603ull){for(unsigned char c:value){seed^=c;seed*=1099511628211ull;}return seed;}
inline std::string Hex(uint64_t value){char text[17]{};std::snprintf(text,sizeof(text),"%016llx",static_cast<unsigned long long>(value));return text;}
inline std::filesystem::path NativePath(const std::filesystem::path& path){
    auto value=std::filesystem::absolute(path).lexically_normal().wstring();
    if(value.starts_with(L"\\\\?\\"))return value;
    if(value.starts_with(L"\\\\"))return L"\\\\?\\UNC\\"+value.substr(2);
    return L"\\\\?\\"+value;
}
inline Snapshot Inspect(const std::filesystem::path& root){
    const auto source=NativePath(root);std::map<std::string,std::string> records;size_t count=0;
    const auto binary=source.parent_path().parent_path().parent_path();
    const auto game=binary.parent_path().parent_path();
    const auto add=[&](const std::filesystem::directory_entry& entry){
        if(!entry.is_regular_file()||entry.is_symlink())return;
        const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return;
        auto ext=entry.path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto filename=entry.path().filename().string();
        const bool modList=filename=="enabled.txt"||filename=="mods.txt"||filename=="runeschema.txt";
        if(ext!=".json"&&ext!=".jsonc"&&ext!=".pak"&&ext!=".utoc"&&ext!=".ucas"&&ext!=".dll"&&ext!=".exe"&&ext!=".lua"&&!modList)return;
        if(++count>100000)throw std::runtime_error("Source cache manifest exceeds its file bound");
        // Game Pass can expose the same install via XboxGames and WindowsApps.
        // Compare logical game-relative names, not the caller's absolute alias.
        // The Win64/WinGDK lane remains part of every binary/mod record.
        auto name=entry.path().lexically_normal().lexically_relative(game).generic_string();
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        auto record=std::to_string(entry.file_size())+":"+std::to_string(entry.last_write_time().time_since_epoch().count());
        // Small authored files include content, so same-size edits cannot reuse metadata.
        if((ext==".json"||ext==".jsonc"||ext==".lua"||modList)&&entry.file_size()<=4*1024*1024){std::ifstream input(entry.path(),std::ios::binary);if(!input)throw std::runtime_error("Source file is not readable");uint64_t hash=1469598103934665603ull;char data[8192];while(input){input.read(data,sizeof(data));hash=Hash(std::string_view(data,static_cast<size_t>(input.gcount())),hash);}record+=":"+Hex(hash);}
        records[name]=record;
    };
    const auto tree=[&](const std::filesystem::path& folder){if(!std::filesystem::exists(folder))return;
        for(auto it=std::filesystem::recursive_directory_iterator(folder);it!=std::filesystem::recursive_directory_iterator();++it){const auto& entry=*it;
            const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Source path is unavailable: "+entry.path().lexically_relative(game).generic_string()+" (Windows error "+std::to_string(GetLastError())+")");
            if(entry.is_directory()){const auto name=entry.path().filename().string();if((attr&FILE_ATTRIBUTE_REPARSE_POINT)||name=="runtime"||name=="cache"||name=="saved"||name=="logs"||name=="exports"||name=="settings")it.disable_recursion_pending();continue;}add(entry);
        }
    };
    tree(source.parent_path()); // Enabled UE4SS mods, including RuneSchema content and plugins.
    if(std::filesystem::is_regular_file(source/"settings"/"settings.jsonc"))add(std::filesystem::directory_entry(source/"settings"/"settings.jsonc"));
    if(std::filesystem::is_directory(binary))for(const auto& entry:std::filesystem::directory_iterator(binary))add(entry);
    tree(binary.parent_path().parent_path()/"Content"/"Paks");
    Snapshot result;uint64_t hash=Hash("helpy-cache-v3|"+binary.filename().generic_string());
    for(const auto& [name,record]:records){result.Files.insert(name);hash=Hash(name+"|"+record+"\n",hash);}result.Key=Hex(hash);return result;
}
inline Snapshot Current;
inline std::future<Snapshot> Worker;
inline std::chrono::steady_clock::time_point Next{};
inline uint64_t Revision=0,RemovalRevision=0;
inline std::string Error;
// Called only by the owning UI/game thread. The worker only reads files and
// returns a value; it never accesses Unreal, ImGui, or mutable cache state.
inline bool Poll(const std::filesystem::path& root){
    bool changed=false;const auto now=std::chrono::steady_clock::now();
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{
        auto result=Worker.get();if(result.Key!=Current.Key){bool removed=false;for(const auto& path:Current.Files)if(!result.Files.contains(path)){removed=true;break;}
            Current=std::move(result);++Revision;if(removed)RemovalRevision=Revision;changed=true;}Error.clear();
    }catch(const std::exception& error){Error=error.what();}Next=now+std::chrono::seconds(5);}
    if(!Worker.valid()&&now>=Next)Worker=std::async(std::launch::async,[root]{return Inspect(root);});return changed;
}
inline bool Ready(){return !Current.Key.empty()&&Error.empty();}
inline std::string Key(const std::string& icon){return Hex(Hash(Current.Key+"|"+icon));}
inline std::filesystem::path Directory(bool liveOnly){
    wchar_t appdata[32768];const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",appdata,32768);if(!size||size>=32768)throw std::runtime_error("LOCALAPPDATA is unavailable");
    return std::filesystem::path(appdata)/"RuneSchema"/"HELPY"/"runtime"/"saved"/"cache"/(liveOnly?"item-icons-live":"item-icons");
}
}
