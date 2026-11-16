#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/ModIdentity.h"
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>
#include <set>
#include <functional>

namespace PS::ModSaveFiles {
namespace fs=std::filesystem;using Json=nlohmann::json;
using Roots=std::map<std::string,fs::path>;
inline std::string Utf8(const fs::path& path){const auto text=path.generic_u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};}
inline std::string Fold(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c+=32;return text;}
inline bool Leaf(const std::string& text){
    if(text.empty()||text.size()>180||text=="."||text==".."||text.back()=='.'||text.back()==' ')return false;
    for(unsigned char c:text)if(c<32||c==127||std::string_view("<>:\"/\\|?*").find(c)!=std::string_view::npos)return false;
    const auto device=Fold(text.substr(0,text.find('.')));
    return device!="con"&&device!="prn"&&device!="aux"&&device!="nul"&&!(device.size()==4&&(device.starts_with("com")||device.starts_with("lpt"))&&device[3]>='0'&&device[3]<='9');
}
inline fs::path Relative(std::string text){
    if(text.size()>1024)throw std::runtime_error("Declared save path is too long.");
    std::replace(text.begin(),text.end(),'\\','/');if(text.empty())return {};
    if(text.front()=='/'||text.back()=='/')throw std::runtime_error("Declared save path must be relative to its root.");
    fs::path path;std::istringstream input(text);std::string part;
    while(std::getline(input,part,'/')){if(!Leaf(part))throw std::runtime_error("Declared save path contains an unsafe component.");path/=fs::u8path(part);}return path;
}
inline Json Validate(const Json& document){
    if(!document.is_object()||document.value("SchemaVersion",0)!=1||!document.contains("Files")||!document["Files"].is_array()||document["Files"].size()>64)throw std::runtime_error("Save declarations require SchemaVersion 1 and up to 64 files.");
    Json files=Json::array();std::set<std::string> keys;
    for(const auto& row:document["Files"]){
        if(!row.is_object())throw std::runtime_error("Invalid save declaration.");
        const auto name=row.at("Name").get<std::string>(),extension=row.at("Extension").get<std::string>(),root=row.at("Root").get<std::string>(),path=row.value("Path",std::string{});
        if(!Leaf(name)||extension.size()<2||extension.size()>17||extension[0]!='.'||extension.substr(1).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")!=std::string::npos||!Leaf(name+extension))throw std::runtime_error("A save declaration needs a literal filename and extension, without wildcards.");
        if(root!="Mod"&&root!="GameSaved"&&root!="LocalAppData"&&root!="RoamingAppData")throw std::runtime_error("Save root must be Mod, GameSaved, LocalAppData or RoamingAppData.");
        const auto relative=Relative(path).generic_string();const auto key=Fold(root+"/"+relative+"/"+name+extension);
        if(!keys.insert(key).second)throw std::runtime_error("Duplicate declared save location.");
        files.push_back({{"Name",name},{"Extension",extension},{"Root",root},{"Path",relative}});
    }return {{"SchemaVersion",1},{"Files",files}};
}
inline Json Read(const fs::path& folder){
    Json files=Json::array();const auto sidecar=folder/"cache/savefiles.json",identity=folder/"ID.txt";
    if(fs::exists(sidecar)){const auto value=Validate(Json::parse(CacheBundleIO::Read(sidecar,64*1024)));files=value["Files"];}
    if(fs::exists(identity)){
        std::istringstream input(CacheBundleIO::Read(identity,64*1024));std::string line;Json entry=Json::object();bool tips=false;
        while(std::getline(input,line)){
            if(line.starts_with("\xef\xbb\xbf"))line.erase(0,3);line=ModIdentity::Trim(line);if(line.empty()||line.starts_with('#')||line.starts_with(';'))continue;
            if(line.front()=='['&&line.back()==']'){tips=Fold(ModIdentity::Trim(line.substr(1,line.size()-2)))=="tips";continue;}
            if(tips)continue;const auto split=line.find_first_of("=:");if(split==line.npos)continue;
            const auto key=Fold(ModIdentity::Trim(line.substr(0,split))),value=ModIdentity::Trim(line.substr(split+1));
            if(key=="savefilename")entry["Name"]=value;else if(key=="savefileextension")entry["Extension"]=value;else if(key=="savefileroot")entry["Root"]=value;else if(key=="savefilepath")entry["Path"]=value;
        }
        if(!entry.empty()){const auto normalized=Validate({{"SchemaVersion",1},{"Files",Json::array({entry})}})["Files"][0];if(std::find(files.begin(),files.end(),normalized)==files.end())files.push_back(normalized);}
    }return Validate({{"SchemaVersion",1},{"Files",files}});
}
inline fs::path Resolve(const Json& row,const Roots& roots){
    const auto key=row.at("Root").get<std::string>();const auto found=roots.find(key);
    if(found==roots.end()||found->second.empty()||!found->second.is_absolute())throw std::runtime_error("This save root is unavailable on the current installation.");
    const auto root=fs::absolute(found->second).lexically_normal();const auto target=(root/Relative(row.value("Path",std::string{}))/fs::u8path(row.at("Name").get<std::string>()+row.at("Extension").get<std::string>())).lexically_normal();
    const auto relative=target.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Declared save escaped its root.");
    try{CacheBundleIO::Safe(target);}catch(...){throw std::runtime_error("Declared save location is redirected or cannot be inspected. Choose its physical location in the mod metadata.");}return target;
}
struct File {
    HANDLE Handle=INVALID_HANDLE_VALUE;
    explicit File(const fs::path& path){Handle=CreateFileW(CacheBundleIO::NativePath(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(Handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot read the save exclusively against writers. Close its owning application and retry.");}
    File(const File&)=delete;~File(){if(Handle!=INVALID_HANDLE_VALUE)CloseHandle(Handle);}
};
inline std::string Hash(File& file){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Close{BCRYPT_ALG_HANDLE& A;BCRYPT_HASH_HANDLE& H;~Close(){if(H)BCryptDestroyHash(H);if(A)BCryptCloseAlgorithmProvider(A,0);}}close{algorithm,hash};
    LARGE_INTEGER start{};if(!SetFilePointerEx(file.Handle,start,nullptr,FILE_BEGIN)||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Cannot start save verification.");
    std::array<unsigned char,65536> buffer{};DWORD read=0;
    for(;;){if(!ReadFile(file.Handle,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr))throw std::runtime_error("Cannot read save bytes.");if(!read)break;if(BCryptHashData(hash,buffer.data(),read,0)<0)throw std::runtime_error("Cannot hash save bytes.");}
    std::array<unsigned char,32> digest{};if(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)throw std::runtime_error("Cannot finish save verification.");
    constexpr char digits[]="0123456789abcdef";std::string result;for(auto c:digest){result+=digits[c>>4];result+=digits[c&15];}return result;
}
inline Json Scan(const fs::path& folder,const Roots& roots){
    auto document=Read(folder);Json entries=Json::array();
    for(const auto& row:document["Files"]){const auto path=Resolve(row,roots);const bool exists=fs::exists(path);if(exists&&!fs::is_regular_file(path))throw std::runtime_error("Declared save is not a regular file.");entries.push_back({{"Path",Utf8(path)},{"Exists",exists}});}
    document["Resolved"]=entries;return document;
}
inline fs::path Backup(const fs::path& folder,const Roots& roots,const fs::path& destination,const std::string& mod,const std::string& version,const Json& expected,const std::function<bool()>& stopped){
    if(!stopped())throw std::runtime_error("Close Dragonwilds before backing up mod saves.");
    if(Scan(folder,roots)!=expected)throw std::runtime_error("Save declarations or file availability changed. Review them again before backing up.");
    static std::atomic<uint64_t> sequence{0};FILETIME time{};GetSystemTimeAsFileTime(&time);
    const auto stamp=(uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime;
    const auto base=destination/CacheBundleIO::Hash(mod);const auto target=base/(std::to_string(stamp)+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++sequence));
    CacheBundleIO::Safe(target);fs::create_directories(base);if(!fs::create_directory(target))throw std::runtime_error("Could not create a unique backup folder.");
    Json entries=Json::array();std::vector<fs::path> created;
    try{
        for(size_t i=0;i<expected.at("Resolved").size();++i){const auto& entry=expected["Resolved"][i];if(!entry.at("Exists").get<bool>()){entries.push_back({{"Source",entry["Path"]},{"Missing",true}});continue;}
            const auto source=Resolve(expected["Files"][i],roots);File locked(source);const auto hash=Hash(locked);const auto copy=target/(std::to_wstring(i)+L"-"+source.filename().wstring());CacheBundleIO::Safe(copy);
            created.push_back(copy);if(!CopyFileW(CacheBundleIO::NativePath(source).c_str(),CacheBundleIO::NativePath(copy).c_str(),TRUE))throw std::runtime_error("Could not copy a declared save. Original files were retained.");
            File verified(copy);if(Hash(verified)!=hash||Hash(locked)!=hash)throw std::runtime_error("Save verification failed. Original files were retained.");
            entries.push_back({{"Source",Utf8(source)},{"File",Utf8(copy.filename())},{"Bytes",fs::file_size(copy)},{"SHA256",hash}});
        }
        if(std::none_of(entries.begin(),entries.end(),[](const auto& row){return row.contains("File");}))throw std::runtime_error("None of the declared saves exist yet.");
        if(!stopped()||Scan(folder,roots)!=expected)throw std::runtime_error("The game or save declarations changed during backup. Retry with Dragonwilds closed.");
        const Json manifest={{"SchemaVersion",1},{"Kind","RuneSchemaModSaveBackup"},{"Mod",mod},{"Version",version},{"CreatedFileTimeUTC",stamp},{"Declarations",expected["Files"]},{"Files",entries}};
        CacheBundleIO::Publish(target/"manifest.json",manifest.dump(2));return target;
    }catch(...){for(const auto& file:created){std::error_code error;fs::remove(file,error);}std::error_code error;fs::remove(target,error);throw;}
}
}
