#pragma once
#include "Core/CacheBundleIO.h"
#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>

namespace PS::LocalModProfiles {
using Json=nlohmann::json;
namespace fs=std::filesystem;
inline constexpr size_t MaximumPayloadBytes=256*1024;
struct Profile {bool Found=false;int64_t Revision=0;std::string Payload;};
inline std::string Guid(std::string value){
    if(value.size()!=32)throw std::runtime_error("Profile requires the actual native character GUID");
    for(auto& c:value){if(c>='a'&&c<='f')c-=32;else if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))throw std::runtime_error("Profile character GUID is invalid");}
    if(value.find_first_not_of('0')==value.npos)throw std::runtime_error("Profile character identity is not assigned");
    return value;
}
inline std::string Class(const Json& declaration){
    if(!declaration.is_object()||declaration.size()!=1||!declaration.contains("Class")||!declaration["Class"].is_string())
        throw std::runtime_error("LocalProfile requires exactly Profile.Class");
    const auto value=declaration["Class"].get<std::string>();
    const auto slash=value.find('/',1);
    if(value.size()>1024||value.empty()||value[0]!='/'||slash==value.npos||slash<=1||!value.ends_with("_C")
        ||value.find("..")!=value.npos||value.find_first_of("\r\n\t\\")!=value.npos||value.find('\0')!=value.npos
        ||value.starts_with("/Script/")||value.starts_with("/Engine/"))
        throw std::runtime_error("Profile.Class must be a cooked mod ActorComponent class path");
    return value;
}
inline fs::path File(const fs::path& root,const std::string& owner,const std::string& character){
    if(owner.empty()||owner.size()>512||owner.find('\0')!=owner.npos)throw std::runtime_error("Profile registry namespace is invalid");
    return root/CacheBundleIO::Hash(owner)/(Guid(character)+".json");
}
inline Json Parse(const std::string& bytes,int maximumDepth=64){
    return Json::parse(bytes,[maximumDepth](int depth,Json::parse_event_t,Json&){
        if(depth>maximumDepth)throw std::runtime_error("Profile JSON nesting exceeds its supported depth");return true;
    });
}
struct WriteLock {
    HANDLE Handle=INVALID_HANDLE_VALUE;
    explicit WriteLock(const fs::path& file){
        const auto path=CacheBundleIO::NativePath(fs::path(file.wstring()+L".lock"));
        CacheBundleIO::Safe(path);fs::create_directories(path.parent_path());CacheBundleIO::Safe(path);
        // A file lock also serializes clients in separate Windows/Wine sessions.
        Handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(Handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Profile is busy or its write lock is unavailable; retry after reading");
    }
    WriteLock(const WriteLock&)=delete;
    ~WriteLock(){if(Handle!=INVALID_HANDLE_VALUE)CloseHandle(Handle);}
};
inline Profile ReadUnlocked(const fs::path& root,const std::string& owner,const std::string& character){
    const auto guid=Guid(character);const auto file=File(root,owner,guid);CacheBundleIO::Safe(file);
    if(!fs::exists(CacheBundleIO::NativePath(file)))return {};
    const auto document=Parse(CacheBundleIO::Read(file,MaximumPayloadBytes+4096),65);
    if(!document.is_object()||document.value("Format",std::string{})!="RuneSchemaLocalProfile"
        ||document.value("Namespace",std::string{})!=owner||document.value("CharacterGuid",std::string{})!=guid
        ||!document.contains("Revision")||!document["Revision"].is_number_integer()
        ||(document["Revision"].is_number_unsigned()&&document["Revision"].get<uint64_t>()>uint64_t((std::numeric_limits<int64_t>::max)()))
        ||document["Revision"].get<int64_t>()<=0||!document.contains("Payload")||!document["Payload"].is_object())
        throw std::runtime_error("Existing profile identity or revision is invalid; it was preserved");
    const auto payload=document["Payload"].dump();
    if(payload.size()>MaximumPayloadBytes||document.value("PayloadSHA256",std::string{})!=CacheBundleIO::Hash(payload))
        throw std::runtime_error("Existing profile payload failed verification; it was preserved");
    return {true,document["Revision"].get<int64_t>(),payload};
}
inline Profile Read(const fs::path& root,const std::string& owner,const std::string& character){
    const auto file=File(root,owner,character);CacheBundleIO::MetadataLock lock(file);
    return ReadUnlocked(root,owner,character);
}
inline Profile Write(const fs::path& root,const std::string& owner,const std::string& character,int64_t expected,const std::string& bytes){
    if(expected<0||expected==(std::numeric_limits<int64_t>::max)())throw std::runtime_error("Profile expected revision is invalid");
    if(bytes.empty()||bytes.size()>MaximumPayloadBytes)throw std::runtime_error("Profile payload must contain at most 256 KiB of JSON");
    const auto payload=Parse(bytes);
    if(!payload.is_object())throw std::runtime_error("Profile payload must be a mod-owned JSON object");
    const auto guid=Guid(character);const auto file=File(root,owner,guid);CacheBundleIO::MetadataLock lock(file);WriteLock diskLock(file);
    const auto current=ReadUnlocked(root,owner,guid);
    if(current.Revision!=expected)throw std::runtime_error("Profile revision conflict; read the current profile before applying again");
    const auto canonical=payload.dump();
    if(canonical.size()>MaximumPayloadBytes)throw std::runtime_error("Serialized profile payload exceeds 256 KiB");
    const Json document={{"Format","RuneSchemaLocalProfile"},{"Namespace",owner},{"CharacterGuid",guid},
        {"Revision",expected+1},{"Payload",payload},{"PayloadSHA256",CacheBundleIO::Hash(canonical)}};
    const auto published=document.dump();CacheBundleIO::Publish(file,published);
    if(CacheBundleIO::Read(file,MaximumPayloadBytes+4096)!=published)
        throw std::runtime_error("Profile commit could not be verified; read the profile before retrying");
    return {true,expected+1,canonical};
}
}
