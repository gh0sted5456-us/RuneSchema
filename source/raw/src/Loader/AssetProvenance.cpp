#include "Loader/AssetProvenance.h"
#include "Unreal/UObject.hpp"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/UObjectArray.hpp"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include <mutex>
#include <unordered_map>

namespace PS::AssetProvenance {
namespace {
struct Entry {
    RC::Unreal::UObject* token{}; // comparison only; validate the shared weak identity before use
    PS::WeakObjectHandle handle;
    std::string source,creator,lastSource,lastMod;
    bool registered{};
    int errors{};
    std::string path,internalName,persistenceId;
};
std::mutex mutex;
std::unordered_map<RC::Unreal::UObject*,Entry> entries;
bool warned{};
bool dirty=true;
nlohmann::json Describe(const Entry& entry) {
    return {{"Kind","RuneSchemaAssetClone"},{"Confirmed",true},{"SourceAsset",entry.source},
        {"CreatingMod",entry.creator},{"LastCloneDefinitionSource",entry.lastSource},
        {"LastCloneDefinitionMod",entry.lastMod},{"Registered",entry.registered},
        {"LastCloneDefinitionErrors",entry.errors},{"AssetPath",entry.path},
        {"InternalName",entry.internalName},{"PersistenceID",entry.persistenceId},
        {"IdentitySource","Clone definition; inspect current item values for later changes"},
        {"Scope","Creation observed this session; not a complete patch history or pak ownership record"}};
}
bool Matches(const Entry& entry,RC::Unreal::UObject* object) {
    if(!object || entry.token!=object || entry.handle.Get()!=object)return false;
    auto* slot=RC::Unreal::FUObjectArray::IndexToObject(entry.handle.ObjectIndex);
    return slot && slot->IsValid(false);
}
}
void Record(RC::Unreal::UObject* object,const std::string& source,const std::string& mod,
    bool createdNow,bool registered,int errors,const nlohmann::json& definition) {
    if(!object)return;
    std::lock_guard lock(mutex);
    auto found=entries.find(object);
    if(found!=entries.end() && !Matches(found->second,object)){entries.erase(found);found=entries.end();}
    if(found==entries.end()) {
        if(!createdNow)return; // A later definition cannot establish the original clone source.
        if(entries.size()>=4096 || source.size()>1024 || mod.size()>256) {
            if(!warned){Log<RC::LogLevel::Warning>(TEXT("Clone provenance limit reached; untracked origins remain unknown.\n"));warned=true;}
            return;
        }
        const auto index=object->GetInternalIndex();
        auto* slot=index>=0?RC::Unreal::FUObjectArray::IndexToObject(index):nullptr;
        if(!slot || slot->GetUObject()!=object || !slot->IsValid(false))return;
        Entry created{};created.token=object;created.handle.Assign(object);
        if(created.handle.Get()!=object)return;
        created.source=source;created.creator=mod;
        created.path=RC::to_string(object->GetPathName());
        found=entries.emplace(object,std::move(created)).first;
    }
    auto& entry=found->second;
    entry.lastSource=source.substr(0,1024);entry.lastMod=mod.substr(0,256);
    entry.registered=registered;entry.errors=errors;
    for(const auto* key:{"InternalName","PersistenceID"})if(definition.contains(key) && definition[key].is_string()) {
        const auto text=definition[key].get<std::string>().substr(0,1024);
        if(std::string_view(key)=="InternalName")entry.internalName=text;else entry.persistenceId=text;
    }
    dirty=true;
}
nlohmann::json Lookup(RC::Unreal::UObject* object) {
    std::lock_guard lock(mutex);
    auto found=entries.find(object);
    if(found==entries.end())return nullptr;
    const auto& entry=found->second;
    if(!Matches(entry,object)){entries.erase(found);return nullptr;}
    return Describe(entry);
}
void Flush() {
    std::lock_guard lock(mutex);
    dirty=false;
}
void Clear() {std::lock_guard lock(mutex);entries.clear();warned=false;}
}
