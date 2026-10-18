#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <algorithm>
namespace PS::NativeContentProviders {
// Presentation attribution only. No asset loading, registration or save authority.
inline std::string Lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);return value;}
inline std::set<std::string> Manifest(const nlohmann::json& document,const std::string& game){
    std::set<std::string> result;
    if(!document.is_object()||document.size()!=1||!document.contains("Contents")||!document["Contents"].is_array()||document["Contents"].size()>4096)return result;
    const auto prefix="../../../"+Lower(game)+"/plugins/";
    for(const auto& entry:document["Contents"]){
        if(!entry.is_object()||!entry.contains("File")||!entry["File"].is_string()||!entry.contains("Descriptor")||!entry["Descriptor"].is_object())continue;
        const auto& descriptor=entry["Descriptor"];
        if(!descriptor.contains("CanContainContent")||descriptor["CanContainContent"]!=true)continue;
        const auto file=Lower(entry["File"].get<std::string>());
        if(file.size()>2048||!file.starts_with(prefix)||!file.ends_with(".uplugin")||file.substr(prefix.size()).find("..")!=std::string::npos)continue;
        const auto slash=file.find_last_of('/');const auto name=file.substr(slash+1,file.size()-slash-1-8);
        if(name.empty()||name.size()>128||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)continue;
        result.insert(name);if(result.size()>=512)break;
    }return result;
}
inline std::set<std::string> Embedded(const std::string& tail,const std::string& game){
    // Current native .pak stores its bounded plugin manifest uncompressed near
    // the end. A compressed/encrypted or changed layout stays unclassified;
    // never infer providers from arbitrary asset names or mod-owned containers.
    size_t search=0;
    for(int attempt=0;attempt<32;++attempt){
        const auto marker=tail.find("\"Contents\"",search);if(marker==std::string::npos)break;search=marker+10;
        const auto start=tail.rfind('{',marker);if(start==std::string::npos||marker-start>64)continue;
        bool quoted=false,escaped=false;int depth=0;const auto end=std::min(tail.size(),start+2*1024*1024);
        for(size_t at=start;at<end;++at){const char c=tail[at];
            if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
            if(c=='"'){quoted=true;continue;}if(c=='{')++depth;
            if(c=='}'&&--depth==0){
                const auto parsed=nlohmann::json::parse(tail.begin()+start,tail.begin()+at+1,nullptr,false);
                if(!parsed.is_discarded()){auto result=Manifest(parsed,game);if(!result.empty())return result;}break;
            }
        }
    }return {};
}
inline std::set<std::string> Read(const std::filesystem::path& file,const std::string& game){
    struct Entry{uintmax_t Bytes=0;std::filesystem::file_time_type Stamp{};std::set<std::string> Providers;};
    static std::mutex mutex;static std::map<std::filesystem::path,Entry> cache;
    try{
        const auto name=Lower(file.filename().string());const auto prefix=Lower(game)+"-";
        bool nativeBase=false;for(const char* lane:{"windows.pak","wingdk.pak","windowsserver.pak","linuxserver.pak","linux.pak"})if(name==prefix+lane)nativeBase=true;
        if(!nativeBase||Lower(file.parent_path().filename().string())!="paks")return {};
        const auto bytes=std::filesystem::file_size(file);const auto stamp=std::filesystem::last_write_time(file);
        {std::lock_guard lock(mutex);if(const auto found=cache.find(file);found!=cache.end()&&found->second.Bytes==bytes&&found->second.Stamp==stamp)return found->second.Providers;}
        const auto count=static_cast<size_t>(std::min<uintmax_t>(bytes,8*1024*1024));std::string tail(count,'\0');
        std::ifstream input(file,std::ios::binary);input.seekg(static_cast<std::streamoff>(bytes-count));input.read(tail.data(),static_cast<std::streamsize>(count));if(static_cast<size_t>(input.gcount())!=count)return {};
        auto providers=Embedded(tail,game);
        {std::lock_guard lock(mutex);if(cache.size()>=8&&!cache.contains(file))cache.erase(cache.begin());cache[file]={bytes,stamp,providers};}return providers;
    }catch(...){return {};}
}
}
