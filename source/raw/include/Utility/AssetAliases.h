#pragma once
#include <map>
#include <mutex>
#include <string>
#include <stdexcept>

namespace PS::AssetAliases {
class Registry {
    std::map<std::wstring,std::wstring> paths;
public:
    void Add(const std::wstring& authored,const std::wstring& runtime) {
        if(authored.empty() || runtime.empty() || authored.front()!=L'/' || runtime.front()!=L'/')
            throw std::runtime_error("Asset alias requires absolute object paths");
        const auto found=paths.find(authored);
        if(found!=paths.end() && found->second!=runtime)throw std::runtime_error("Asset alias ownership collision");
        paths.insert_or_assign(authored,runtime);
    }
    std::wstring Resolve(const std::wstring& path) const {
        const auto found=paths.find(path);
        return found==paths.end()?path:found->second;
    }
    void Clear(){paths.clear();}
};
inline Registry registry;
inline std::mutex mutex;
inline void Add(const std::wstring& authored,const std::wstring& runtime){std::scoped_lock lock(mutex);registry.Add(authored,runtime);}
inline std::wstring Resolve(const std::wstring& path){std::scoped_lock lock(mutex);return registry.Resolve(path);}
inline void Clear(){std::scoped_lock lock(mutex);registry.Clear();}
}
