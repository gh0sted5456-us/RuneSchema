#pragma once
#include "Core/AlphabeticalItemCatalog.h"
#include "Core/PagedItemCatalog.h"
#include <filesystem>
#include <fstream>
#include <future>
#include <chrono>
#include <functional>
#include <windows.h>
namespace PS::PersistentItemCatalog {
using Json=nlohmann::json;
using Cache=AlphabeticalItemCatalog::Cache;
inline constexpr uintmax_t MaxBytes=64*1024*1024;
// Icon-only ownership/census refinements do not change which items exist.
// Use the content catalogue fingerprint, not the broader icon snapshot key.
inline std::string DiskScope(const Json& published,const Json& paths,const std::string& locale){
    const auto content=published.value("CatalogKey",std::string{});
    if(content.empty())throw std::runtime_error("Verified item catalogue fingerprint is unavailable");
    const auto digest=paths.is_string()&&CacheBundleIO::Digest(paths.get<std::string>())?paths.get<std::string>():CacheBundleIO::Hash(paths.dump());
    return Json::array({"item-browse-v2",content,digest,locale}).dump();
}
inline bool DurableRow(const Json& row){
    if(!row.is_object())return false;const auto path=row.value("Path",std::string{});
    std::string folded=path;for(auto& c:folded)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return !folded.starts_with("/engine/transient")&&!folded.starts_with("/temp/")&&row.value("Available",true);
}
// Presentation-only manifest. All gameplay operations still resolve and
// validate live assets. The exact source/locale/runtime-path scope is mandatory.
inline std::unique_ptr<Cache> Load(const std::filesystem::path& file,const std::string& scope,const std::string& legacyScope={},bool* migrated=nullptr){
    std::error_code ec;const auto bytes=std::filesystem::file_size(file,ec);
    if(ec||bytes>MaxBytes)return {};
    try{std::ifstream input(file,std::ios::binary);auto doc=Json::parse(input);
        if(doc.at("Version")!=1)return {};
        if(doc.at("Scope")!=scope){
            const bool compatible=PagedItemCatalog::CompactScope(doc.at("Scope").get<std::string>())==PagedItemCatalog::CompactScope(scope);
            if(!compatible&&(legacyScope.empty()||doc.at("Scope")!=legacyScope))return {};if(migrated)*migrated=true;
        }
        auto& rows=doc.at("Rows");if(!rows.is_array())return {};
        std::erase_if(rows.get_ref<Json::array_t&>(),[](const Json& row){return !DurableRow(row);});
        auto result=std::make_unique<Cache>();result->Scope(scope);result->Restore(rows);return result;
    }catch(...){return {};}
}
inline void Save(const std::filesystem::path& file,const std::string& scope,const std::vector<std::shared_ptr<const Json>>& rows){
    std::filesystem::path temporary;
    try{Json values=Json::array();for(const auto& row:rows)if(DurableRow(*row))values.push_back(*row);
        const auto data=Json{{"Version",1},{"Scope",scope},{"Rows",std::move(values)}}.dump();if(data.size()>MaxBytes)return;
        std::filesystem::create_directories(file.parent_path());temporary=file;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
        {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write(data.data(),data.size());out.close();if(!out)throw std::runtime_error("Item manifest write failed");}
        if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Item manifest commit failed");
        std::vector<std::shared_ptr<const Json>> durable;for(const auto& row:rows)if(DurableRow(*row))durable.push_back(row);
        PagedItemCatalog::Save(file,scope,durable);
    }catch(...){std::error_code ec;if(!temporary.empty())std::filesystem::remove(temporary,ec);}
}
class Store {
    struct Loaded {std::string Scope;std::unique_ptr<Cache> Value;bool Migrated=false;};
    std::future<Loaded> m_read;std::future<void> m_write;
    std::string m_attempt,m_saved,m_repaired;
public:
    void ResetScope(){m_attempt.clear();m_saved.clear();}
    // False only while a matching disk manifest is being inspected. A stale
    // worker is drained without blocking; its result can never cross scopes.
    bool Restore(const std::filesystem::path& file,const std::string& scope,std::unique_ptr<Cache>& cache,const std::string& legacyScope={}){
        if(m_read.valid()){
            if(m_read.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
            // This path is now the legacy/missing-page fallback. Re-persist once
            // to create or repair the fast sidecar without another native census.
            try{auto result=m_read.get();if(result.Scope==scope&&result.Value){cache=std::move(result.Value);m_saved.clear();}}catch(...){}
        }
        if(m_attempt!=scope){m_attempt=scope;m_read=std::async(std::launch::async,[file,scope,legacyScope]{bool migrated=false;auto value=Load(file,scope,legacyScope,&migrated);return Loaded{scope,std::move(value),migrated};});return false;}
        return true;
    }
    void Persist(const std::filesystem::path& file,const std::string& scope,const Cache& cache){
        if(m_write.valid()){if(m_write.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;try{m_write.get();}catch(...){}}
        if(m_saved==scope||!cache.Persistable())return;
        auto rows=cache.Snapshot();m_saved=scope;
        m_write=std::async(std::launch::async,[file,scope,rows=std::move(rows)]{Save(file,scope,rows);});
    }
    // Repair copied presentation metadata without a UObject census or cache
    // invalidation. The transform owns immutable source tables, no engine data.
    void Repair(const std::filesystem::path& file,const std::string& scope,std::function<bool(Json&)> transform,const std::string& metadataIdentity={}){
        const auto repairKey=scope+"|"+metadataIdentity;if(m_repaired==repairKey)return;
        if(m_write.valid()){if(m_write.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;try{m_write.get();}catch(...){}}
        m_repaired=repairKey;
        m_write=std::async(std::launch::async,[file,scope,transform=std::move(transform)]{
            try{const auto stamp=std::filesystem::last_write_time(file);auto cache=Load(file,scope);if(!cache)return;
                auto rows=cache->Snapshot();bool changed=false;for(auto& row:rows){Json copy=*row;if(transform(copy)){row=std::make_shared<const Json>(std::move(copy));changed=true;}}
                if(changed&&std::filesystem::last_write_time(file)==stamp)Save(file,scope,rows);
            }catch(...){} // A repair failure leaves the existing cache intact.
        });
    }
    void Shutdown()noexcept{if(m_read.valid())try{(void)m_read.get();}catch(...){}if(m_write.valid())try{m_write.get();}catch(...){}m_attempt.clear();m_saved.clear();}
};
}
