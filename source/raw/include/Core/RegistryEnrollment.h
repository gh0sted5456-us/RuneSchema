#pragma once
#include <mutex>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <optional>
namespace PS::RegistryEnrollment {
inline std::mutex Mutex;
inline std::set<std::string> Failures;
enum class IdentityKind { Item,Recipe,Quest,Journal };
inline std::set<std::string> UnscopedFailures;
inline std::map<IdentityKind,std::set<std::string>> KindFailures;
struct Declaration {std::string Owner,Source,Reason;};
inline std::map<std::pair<std::string,std::string>,std::string> Declarations;
inline void RecordDeclaration(const std::string& owner,const std::string& source,const std::string& reason={}){std::scoped_lock lock(Mutex);Declarations.insert_or_assign({owner,source},reason);}
inline std::vector<Declaration> DeclarationSnapshot(){std::scoped_lock lock(Mutex);std::vector<Declaration> result;for(const auto& [key,reason]:Declarations)result.push_back({key.first,key.second,reason});return result;}
inline std::map<std::string,std::pair<IdentityKind,std::string>> Pending;
inline void Reset(){std::scoped_lock lock(Mutex);Failures.clear();UnscopedFailures.clear();KindFailures.clear();Pending.clear();Declarations.clear();}
inline void Reject(const std::string& source,const std::string& reason,std::optional<IdentityKind> kind={}){std::scoped_lock lock(Mutex);const auto failure=source+": "+reason;Failures.insert(failure);if(kind)KindFailures[*kind].insert(failure);else UnscopedFailures.insert(failure);}
inline bool Healthy(){std::scoped_lock lock(Mutex);return Failures.empty();}
inline bool Healthy(IdentityKind kind){std::scoped_lock lock(Mutex);const auto found=KindFailures.find(kind);return UnscopedFailures.empty()&&(found==KindFailures.end()||found->second.empty());}
inline std::optional<IdentityKind> KindForLane(const std::string& lane){if(lane=="Items")return IdentityKind::Item;if(lane=="Recipes")return IdentityKind::Recipe;if(lane=="Quests")return IdentityKind::Quest;if(lane=="Journals"||lane=="Lore")return IdentityKind::Journal;return {};}
// Queued definitions are not orphans. Only their owning registrar may complete
// a token, after native identity verification, never merely after parsing.
inline void Await(const std::string& source,IdentityKind kind,const std::string& id){std::scoped_lock lock(Mutex);Pending.insert_or_assign(source,std::pair{kind,id});}
inline void Complete(const std::string& source){std::scoped_lock lock(Mutex);Pending.erase(source);}
inline size_t PendingCount(){std::scoped_lock lock(Mutex);return Pending.size();}
inline bool IsPending(IdentityKind kind,const std::string& id){std::scoped_lock lock(Mutex);for(const auto& [source,entry]:Pending)if(entry.first==kind&&entry.second==id)return true;return false;}
}
