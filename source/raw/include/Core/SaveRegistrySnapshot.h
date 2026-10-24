#pragma once
#include <memory>
#include <mutex>
#include <unordered_set>
#include <string>
#include <map>
#include <optional>
#include "Core/RegistryEnrollment.h"
namespace PS::SaveCleanup {
struct RegistrySnapshot {
    struct Binding {std::string Asset,Class,Owner;bool operator==(const Binding&)const=default;};
    std::map<std::string,Binding> Bindings; // Exact live kind:ID -> asset/class/provider; recovery only, never pruning authority.
    std::unordered_set<std::string> Items,Recipes,Quests,Journals;
    bool QuestsComplete=false,JournalsComplete=false;
    bool ItemsComplete=true,RecipesComplete=true;
    std::optional<int64_t> QuestCompletedState;
    bool Ready()const{return RegistryEnrollment::Healthy()&&ItemsComplete&&RecipesComplete&&Items.size()>=500 && Recipes.size()>=300;}
    bool CanClean(RegistryEnrollment::IdentityKind kind)const{
        if(!RegistryEnrollment::Healthy(kind))return false;
        switch(kind){case RegistryEnrollment::IdentityKind::Item:return ItemsComplete&&Items.size()>=500;
        case RegistryEnrollment::IdentityKind::Recipe:return RecipesComplete&&Recipes.size()>=300;
        case RegistryEnrollment::IdentityKind::Quest:return QuestsComplete&&QuestCompletedState.has_value();
        case RegistryEnrollment::IdentityKind::Journal:return JournalsComplete;}return false;
    }
    bool AnyCleanable()const{return CanClean(RegistryEnrollment::IdentityKind::Item)||CanClean(RegistryEnrollment::IdentityKind::Recipe)||CanClean(RegistryEnrollment::IdentityKind::Quest)||CanClean(RegistryEnrollment::IdentityKind::Journal);}
};
inline std::mutex RegistryMutex;
inline std::shared_ptr<const RegistrySnapshot> CurrentRegistry;
inline void PublishRegistry(RegistrySnapshot snapshot) {
    auto next=snapshot.AnyCleanable()?std::make_shared<const RegistrySnapshot>(std::move(snapshot)):nullptr;
    std::lock_guard lock(RegistryMutex);CurrentRegistry=std::move(next);
}
inline std::shared_ptr<const RegistrySnapshot> ReadRegistry() {
    std::lock_guard lock(RegistryMutex);return CurrentRegistry;
}
using VerifiedKind=RegistryEnrollment::IdentityKind;
// Only call after exact native identity-map readback. This is additive:
// it cannot certify an incomplete registry or turn an old cache into authority.
inline bool IncludeVerifiedIdentity(VerifiedKind kind,const std::string& id) noexcept {
    std::lock_guard lock(RegistryMutex);
    if(!CurrentRegistry||id.empty()||id.size()>1024)return false;
    try{
        auto next=std::make_shared<RegistrySnapshot>(*CurrentRegistry);
        auto& ids=kind==VerifiedKind::Item?next->Items:kind==VerifiedKind::Recipe?next->Recipes:kind==VerifiedKind::Quest?next->Quests:next->Journals;
        if(!ids.insert(id).second)return true;
        CurrentRegistry=std::move(next);return true;
    }catch(...){CurrentRegistry.reset();return false;} // Fail closed for deletion, never fail a committed native registration.
}
}
