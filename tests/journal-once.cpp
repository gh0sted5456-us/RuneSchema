
#include <filesystem>
#include <string>
#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>
namespace RC {using StringType=std::string;}
enum class EEngineLifecyclePhase{PostEngineInit,GameInstanceInit};
namespace PS::JsonHelpers{template<class F>void ParseJsonFilesInPath(const std::filesystem::path&,F f){f(nlohmann::json::object());}}
struct DragonWildsJournalModLoader {
 bool m_initialJournalApplied=false;int applied=0,queued=0,errors=0,patchCalls=0;
 struct Result{int ErrorCount;};
 void QueueData(const nlohmann::json&){++queued;}
 void ApplyPendingPatches(){++patchCalls;}
 Result ApplyAll(){++applied;return {errors};}
 void OnLoad(const std::filesystem::path&,const RC::StringType&,const EEngineLifecyclePhase&);
};
    void DragonWildsJournalModLoader::OnLoad(const std::filesystem::path& loaderPath,
        const RC::StringType&, const EEngineLifecyclePhase& phase)
    {
        if (phase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath,
                [this](const nlohmann::json& data) { QueueData(data); });
        }
        else if (phase == EEngineLifecyclePhase::GameInstanceInit && !m_initialJournalApplied)
        {
            ApplyPendingPatches();
            m_initialJournalApplied = ApplyAll().ErrorCount == 0;
        }
    }


int main(){DragonWildsJournalModLoader x;
for(int i=0;i<6;++i)x.OnLoad({},"",EEngineLifecyclePhase::PostEngineInit);
assert(x.queued==6&&x.applied==0);
for(int i=0;i<6;++i)x.OnLoad({},"",EEngineLifecyclePhase::GameInstanceInit);
assert(x.applied==1&&x.patchCalls==1);
DragonWildsJournalModLoader y;y.errors=1;y.OnLoad({},"",EEngineLifecyclePhase::GameInstanceInit);assert(!y.m_initialJournalApplied);
y.errors=0;y.OnLoad({},"",EEngineLifecyclePhase::GameInstanceInit);assert(y.m_initialJournalApplied&&y.applied==2);
std::cout<<"PASS production journal dispatch: queue all files, one successful application, retain retry after error\n";}
