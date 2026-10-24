#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace PS::MappingBackbone {
namespace fs=std::filesystem;
struct Mapping {fs::path Path;std::uintmax_t Size{};std::string Fingerprint;bool Available{};};
inline std::string Fingerprint(const fs::path& path){
    std::ifstream stream(path,std::ios::binary);if(!stream)return {};
    uint64_t hash=14695981039346656037ull;std::array<char,64*1024> buffer{};
    while(stream){stream.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        for(std::streamsize index=0;index<stream.gcount();++index){hash^=static_cast<unsigned char>(buffer[static_cast<size_t>(index)]);hash*=1099511628211ull;}}
    if(stream.bad())return {};
    std::ostringstream text;text<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return text.str();
}
inline std::atomic<std::shared_ptr<const Mapping>> Generated;
inline void Activate(Mapping value){Generated.store(std::make_shared<const Mapping>(std::move(value)));}
inline Mapping Current(const fs::path&){if(const auto generated=Generated.load())return *generated;return {};}
}
