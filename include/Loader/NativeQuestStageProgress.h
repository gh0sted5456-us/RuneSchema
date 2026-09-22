#pragma once
#include "Loader/QuestStageProgress.h"
#include "Loader/QuestNativeAdapter.h"

namespace DragonWilds::Quests::Stages {
// The owning receipt must validate the definition fingerprint, player, phase,
// and repeat run before opening this store. This adapter never initializes or
// gives a quest, pays a reward, or writes a runtime JSON file.
inline Progress OpenNative(const QuestNative::Adapter& api,Graph graph,int run,bool requireInitialized=true) {
    api.ValidateCounters();
    if(requireInitialized && !api.IsInitialized())throw std::runtime_error("Quest stage save record is not initialized");
    const auto name=[](const std::string& key){
        return RC::Unreal::FName(RC::to_generic_string(key).c_str(),RC::Unreal::FNAME_Add);
    };
    return Progress(std::move(graph),run,
        [api,name](const std::string& key){return api.GetInt(name(key));},
        [api,name](const std::string& key,int value){api.SetInt(name(key),value);});
}
}
