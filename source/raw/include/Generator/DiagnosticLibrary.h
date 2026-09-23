#pragma once
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <functional>
#include <set>
#include <vector>

namespace PS::DiagnosticLibrary {
using Json=nlohmann::json;
struct Limits {size_t Files,Entries,Bytes;};
struct Result {
    std::vector<Json> Documents;
    std::vector<std::string> Errors;
    size_t Rejected=0;
    bool Truncated=false;
};
inline Result Load(const std::filesystem::path& folder,Limits limits,
    const std::function<Json(const std::string&)>& parse) {
    Result result;
    if(!limits.Files || !limits.Entries || !limits.Bytes)throw std::runtime_error("Invalid diagnostic library limits");
    const auto reject=[&](const std::string& reason) {
        ++result.Rejected;if(result.Errors.size()<64)result.Errors.push_back(reason);
    };
    try {
        std::filesystem::create_directories(folder);
        std::vector<std::filesystem::path> paths;
        size_t examined=0;
        for(const auto& entry:std::filesystem::directory_iterator(folder)) {
            if(++examined>limits.Entries){result.Truncated=true;break;}
            if(entry.is_regular_file() && (entry.path().extension()==".json" || entry.path().extension()==".jsonc"))paths.push_back(entry.path());
        }
        // Never expose a filesystem-order-dependent subset of an oversized directory.
        if(result.Truncated) {
            result.Errors.push_back("Directory entry limit exceeded; no files loaded. Split or reduce this library and reload.");
            return result;
        }
        std::sort(paths.begin(),paths.end());
        std::set<std::string> names;
        for(const auto& path:paths) {
            if(result.Documents.size()>=limits.Files){result.Truncated=true;break;}
            try {
                const auto stamp=std::filesystem::last_write_time(path);
                const auto size=std::filesystem::file_size(path);
                auto document=parse(ConfigFiles::Read(path,limits.Bytes));
                if(std::filesystem::last_write_time(path)!=stamp || std::filesystem::file_size(path)!=size)
                    throw std::runtime_error("File changed during load; reload after saving finishes");
                const auto name=document.at("Name").get<std::string>();
                if(!names.insert(name).second)throw std::runtime_error("Duplicate diagnostic Name: "+name);
                result.Documents.push_back(std::move(document));
            }catch(const std::exception& e){reject(path.filename().string()+": "+e.what());}
        }
        std::sort(result.Documents.begin(),result.Documents.end(),[](const auto& a,const auto& b){return a.at("Name")<b.at("Name");});
        if(result.Truncated)result.Errors.push_back("Library capacity reached; remaining files were not checked.");
        if(result.Rejected>64)result.Errors.push_back(std::to_string(result.Rejected-64)+" additional rejections; correct the listed files and reload.");
    }catch(const std::exception& e){result.Documents.clear();reject(e.what());}
    return result;
}
}
