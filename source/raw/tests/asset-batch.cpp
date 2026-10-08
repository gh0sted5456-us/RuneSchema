
#include <vector>
#include "Utility/ConsumeQueue.h"
#include <map>
#include <mutex>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <format>
#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>
using nlohmann::json;
#define STR(x) x
#define TEXT(x) x
namespace RC {using StringType=std::string;}
enum class LogLevel {Normal,Warning,Error,Verbose};
std::vector<std::string> logs;
namespace PS {
std::string ToWideSafe(const char* s){return s;}
template<LogLevel L,class... A>void Log(const char* fmt,A... args){logs.push_back(std::vformat(fmt,std::make_format_args(args...)));}
template<class... A>void RoutineLog(const char*,const char* fmt,A... args){Log<LogLevel::Normal>(fmt,args...);}
}
struct Type {std::string GetName(){return "ItemData";}};
struct UObject {std::string name;Type type;std::string GetName(){return name;}Type* GetClassPrivate(){return &type;}};
class DragonWildsAssetModLoader {
public:
struct PendingAsset{std::string Target,ObjectPath,ModName;json Properties;bool IsPatch=false;};
struct LoadResult{int PropertiesWritten=0,ErrorCount=0;};
std::vector<PendingAsset> m_pendingAssets;std::vector<UObject*> m_createdAssets;std::mutex m_mutex;
UObject existing{"existing"},createdA{"createdA"},createdB{"createdB"},subsystem{"subsystem"};
int searches=0,registers=0,applies=0,creates=0;std::vector<std::string> applied;
UObject* FindItemSubsystem(){++searches;return &subsystem;}
UObject* Resolve(const PendingAsset& p){return p.Target=="existing"?&existing:nullptr;}
UObject* CreateFromClone(const PendingAsset& p,UObject* s){assert(s==&subsystem);if(p.Target=="bad")throw std::runtime_error("bad clone");auto* o=creates++?&createdB:&createdA;m_createdAssets.push_back(o);return o;}
bool IsReadyForPatch(UObject*){return true;}bool IsSupportedTarget(UObject*){return true;}
void Apply(UObject*,const PendingAsset& p,LoadResult& r){if(p.ModName=="throw")throw std::runtime_error("apply");applied.push_back(p.ModName);++applies;r.PropertiesWritten=2;}
bool RegisterCreatedItem(UObject*,const PendingAsset&,UObject* s){assert(s==&subsystem);++registers;return true;}
void TryApplyPending();
};
    void DragonWildsAssetModLoader::TryApplyPending()
    {
        struct BatchResult {
            int TargetsUpdated = 0;
            int Created = 0;
            int ClonesUpdated = 0;
            int Patched = 0;
            int PropertiesWritten = 0;
            int ErrorCount = 0;
        };

        std::map<RC::StringType, BatchResult> batchResults;
        std::scoped_lock lock{m_mutex};

        UObject* itemSubsystem = nullptr;
        bool subsystemSearched = false;
        PS::ConsumeQueue(m_pendingAssets, [&](PendingAsset& pending)
        {
            auto* it = &pending;
            UObject* object = nullptr;
            const bool isClone = it->Properties.contains("$Clone");
            bool createdNow = false;
            if (isClone && !subsystemSearched) {
                itemSubsystem = FindItemSubsystem();
                subsystemSearched = true;
            }
            try
            {
                object = Resolve(*it);
                if (object && isClone
                    && std::find(m_createdAssets.begin(), m_createdAssets.end(), object)
                        == m_createdAssets.end())
                    throw std::runtime_error(
                        "the clone target collides with an existing loaded asset");
                if (!object && isClone) {
                    object = CreateFromClone(*it, itemSubsystem);
                    createdNow = object != nullptr;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Clone '{}' from {} was rejected safely: {}\n"),
                    it->Target, it->ModName, PS::ToWideSafe(error.what()));
                batchResults[it->ModName].ErrorCount++;
                return true;
            }
            if (!object || !IsReadyForPatch(object))
            {
                return false;
            }

            if (!IsSupportedTarget(object))
            {
                auto* objectClass = object->GetClassPrivate();
                auto className = objectClass ? objectClass->GetName() : TEXT("<unknown>");
                PS::Log<LogLevel::Error>(STR("'{}' resolved to '{}' (class '{}'), which is not a DataAsset, a Curve, or a subobject owned by one. Skipping.\n"),
                    it->Target, object->GetName(), className);
                batchResults[it->ModName].ErrorCount++;
                return true;
            }

            LoadResult result{};
            Apply(object, *it, result);
            if (isClone && !RegisterCreatedItem(object, *it, itemSubsystem))
                result.ErrorCount++;

            auto& batchResult = batchResults[it->ModName];
            if (isClone) {
                if (createdNow) ++batchResult.Created;
                else ++batchResult.ClonesUpdated;
            } else if (it->IsPatch) ++batchResult.Patched;
            else ++batchResult.TargetsUpdated;
            batchResult.PropertiesWritten += result.PropertiesWritten;
            batchResult.ErrorCount += result.ErrorCount;

            return true;
        });

        for (auto& [modName, result] : batchResults)
        {
            if (result.Created || result.ClonesUpdated)
                PS::RoutineLog("assets", STR("{} $Clone: {} new, {} updated.\n"),
                    modName, result.Created, result.ClonesUpdated);
            if (result.Patched)
                PS::RoutineLog("patches", STR("{} $Patch: {} updated.\n"), modName, result.Patched);
            if (result.ErrorCount)
                PS::Log<LogLevel::Warning>(STR("{} assets: {} updated, {} properties written, {} errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten, result.ErrorCount);
            else if (result.TargetsUpdated)
                PS::RoutineLog("assets", STR("{} assets: {} updated, 0 errors.\n"), modName, result.TargetsUpdated);
        }
    }

int main(){
 DragonWildsAssetModLoader x;
 auto clone=json{{"$Clone","source"}};
 x.m_pendingAssets={{"newA","","Armor",clone},{"newB","","Armor",clone},{"existing","","Patch",json::object(),true},{"missing","","Other",json::object()}};
 x.TryApplyPending();assert(x.searches==1&&x.creates==2&&x.registers==2&&x.applies==3);assert(x.m_pendingAssets.size()==1);
 assert(std::find(logs.begin(),logs.end(),"Armor $Clone: 2 new, 0 updated.\n")!=logs.end());
 assert(std::find(logs.begin(),logs.end(),"Patch $Patch: 1 updated.\n")!=logs.end());
 x.m_pendingAssets.clear();x.m_createdAssets.push_back(&x.existing);x.m_pendingAssets.push_back({"existing","","Armor",clone});x.TryApplyPending();assert(x.creates==2);
 assert(std::find(logs.begin(),logs.end(),"Armor $Clone: 0 new, 1 updated.\n")!=logs.end());
 x.m_pendingAssets.push_back({"bad","","Armor",clone});x.TryApplyPending();assert(x.m_pendingAssets.empty());

 x.m_pendingAssets={{"missing","","unresolvedA",json::object()},{"existing","","first",json::object()},{"missing","","unresolvedB",json::object()},{"existing","","second",json::object()}};
 x.TryApplyPending();assert(x.m_pendingAssets.size()==2&&x.m_pendingAssets[0].ModName=="unresolvedA"&&x.m_pendingAssets[1].ModName=="unresolvedB");
 assert(x.applied[x.applied.size()-2]=="first"&&x.applied.back()=="second");
 x.m_pendingAssets={{"existing","","done",json::object()},{"missing","","keep",json::object()},{"existing","","throw",json::object()},{"existing","","tail",json::object()}};
 try{x.TryApplyPending();assert(false);}catch(const std::runtime_error&){}
 assert(x.m_pendingAssets.size()==3&&x.m_pendingAssets[0].ModName=="keep"&&x.m_pendingAssets[1].ModName=="throw"&&x.m_pendingAssets[2].ModName=="tail");
 x.m_pendingAssets[1].ModName="recovered";x.TryApplyPending();assert(x.m_pendingAssets.size()==1&&x.m_pendingAssets[0].ModName=="keep");
 std::cout<<"PASS production asset batch: one subsystem lookup, clone create/update counts, patch count, unresolved retention, failed clone removal\n";
}
