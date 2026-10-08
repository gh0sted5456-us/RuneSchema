#include "Loader/AssetMetadataRegistry.h"
#include <stdexcept>
#include "SDK/WeakObjectHandle.h"
#include <mutex>
#include <unordered_map>
namespace PS::AssetMetadata {
namespace {
struct RecordValue {WeakObjectHandle handle;Declaration declared;std::string owner;bool installed=false,incomplete=false;};
std::mutex mutex;
std::unordered_map<RC::Unreal::UObject*,RecordValue> records;
auto Find(RC::Unreal::UObject* object) {
    auto it=records.find(object);
    if(it!=records.end()&&it->second.handle.Get()!=object){records.erase(it);return records.end();}
    return it;
}
}
void Record(RC::Unreal::UObject* object,const Declaration& value,const std::string& owner,bool installedDefinition) {
    if(!object)return;
    std::lock_guard lock(mutex);auto it=Find(object);
    if(it==records.end()) {
        if(records.size()>=32768){std::erase_if(records,[](const auto& pair){return pair.second.handle.Get()!=pair.first;});}
        if(records.size()>=32768)throw std::runtime_error("Asset metadata tracking limit reached");
        RecordValue row;row.handle.Assign(object);
        if(row.handle.Get()!=object)throw std::runtime_error("Cannot validate the asset metadata object's lifetime");
        it=records.emplace(object,std::move(row)).first;
    }
    it->second.declared=Merge(it->second.declared,value);
    it->second.owner=owner.substr(0,256);it->second.installed|=installedDefinition;
}
Declaration Lookup(RC::Unreal::UObject* object){std::lock_guard lock(mutex);const auto it=Find(object);return it==records.end()?Declaration{}:it->second.declared;}
bool HasInstalledDefinition(RC::Unreal::UObject* object){std::lock_guard lock(mutex);const auto it=Find(object);return it!=records.end()&&it->second.installed&&!it->second.incomplete;}
bool IsManaged(RC::Unreal::UObject* object){std::lock_guard lock(mutex);return Find(object)!=records.end();}
void MarkIncomplete(RC::Unreal::UObject* object) noexcept {
    // Record() has already allocated this entry before a live clone can publish files.
    try {std::lock_guard lock(mutex);auto it=Find(object);if(it!=records.end())it->second.incomplete=true;}catch(...){}
}
bool IsIncomplete(RC::Unreal::UObject* object){std::lock_guard lock(mutex);const auto it=Find(object);return it!=records.end()&&it->second.incomplete;}
void Forget(RC::Unreal::UObject* object){std::lock_guard lock(mutex);records.erase(object);}
void Clear(){std::lock_guard lock(mutex);records.clear();}
}
