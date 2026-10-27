#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/MapProjection.h"
#include <atomic>
#include <cstring>
namespace PS::MapPresentationCache {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr const char* Asset="/Game/Art/UI/Map/T_WorldMap_Landmass.T_WorldMap_Landmass";
inline constexpr size_t MaxBytes=16+2048*2048*4;
inline std::atomic<uint64_t> Revision{0};
inline Json Metadata(const Json& value){
    if(!value.is_object()||value.dump().size()>8192||value.value("Version",0)!=1||value.value("Asset",std::string{})!=Asset
        ||!value.contains("Bytes")||!value["Bytes"].is_number_integer()||value["Bytes"]<20||value["Bytes"]>MaxBytes
        ||!CacheBundleIO::Digest(value.value("SHA256",std::string{})))throw std::runtime_error("[RS-CACHE-002] Invalid world-map metadata.");
    const auto projection=MapProjection::Projection::From(value.value("Projection",Json::object()));
    if(!projection.Valid())throw std::runtime_error("[RS-CACHE-002] Invalid cached map alignment.");
    return {{"Version",1},{"Asset",Asset},{"Projection",projection.ToJson()},{"Bytes",value["Bytes"]},{"SHA256",value["SHA256"]}};
}
inline void Pixels(std::string_view bytes){
    uint32_t width=0,height=0,size=0;
    if(bytes.size()<20||bytes.size()>MaxBytes||bytes.substr(0,4)!="RSI3")throw std::runtime_error("[RS-CACHE-002] Invalid world-map image.");
    std::memcpy(&width,bytes.data()+4,4);std::memcpy(&height,bytes.data()+8,4);std::memcpy(&size,bytes.data()+12,4);
    if(!width||!height||width>2048||height>2048||size!=width*height*4||bytes.size()!=16ull+size)throw std::runtime_error("[RS-CACHE-002] Invalid world-map pixels.");
}
inline void Validate(const Json& metadata,std::string_view bytes){
    const auto value=Metadata(metadata);Pixels(bytes);
    if(bytes.size()!=value["Bytes"].get<uint64_t>()||CacheBundleIO::Hash(bytes)!=value["SHA256"].get<std::string>())throw std::runtime_error("[RS-CACHE-004] World-map checksum mismatch.");
}
inline fs::path Folder(const fs::path& cache){return cache/"world-map";}
inline Json Reference(const fs::path& cache){
    const auto folder=Folder(cache),index=folder/"current.json";CacheBundleIO::Safe(index);
    if(!fs::is_regular_file(CacheBundleIO::NativePath(index)))return Json::object();
    auto value=Metadata(Json::parse(CacheBundleIO::Read(index,8192)));
    const auto image=folder/(value["SHA256"].get<std::string>()+".rsmap");CacheBundleIO::Safe(image);
    if(!fs::is_regular_file(CacheBundleIO::NativePath(image))||fs::file_size(CacheBundleIO::NativePath(image))!=value["Bytes"].get<uint64_t>())return Json::object();
    value["Image"]={{"Icon",Asset},{"File",image.string()},{"Status","Ready"},{"CachedReference",true}};return value;
}
inline std::string Bytes(const fs::path& cache,const Json& reference){
    const auto value=Metadata(reference);const auto bytes=CacheBundleIO::Read(Folder(cache)/(value["SHA256"].get<std::string>()+".rsmap"),MaxBytes);Validate(value,bytes);return bytes;
}
inline void Install(const fs::path& cache,const Json& metadata,std::string_view bytes){
    const auto value=Metadata(metadata);Validate(value,bytes);const auto folder=Folder(cache);
    const auto image=folder/(value["SHA256"].get<std::string>()+".rsmap");
    CacheBundleIO::Publish(image,bytes);CacheBundleIO::Publish(folder/"current.json",value.dump());++Revision;
}
inline void Capture(const fs::path& cache,const Json& projection,const fs::path& image){
    const auto bytes=CacheBundleIO::Read(image,MaxBytes);Pixels(bytes);
    Install(cache,{{"Version",1},{"Asset",Asset},{"Projection",projection},{"Bytes",bytes.size()},{"SHA256",CacheBundleIO::Hash(bytes)}},bytes);
}
}
