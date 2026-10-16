#pragma once
#include "Loader/QuestDefinition.h"
#include "Loader/NativeQuestStageProgress.h"
#include "Loader/QuestDefinitionIdentity.h"
#include <bit>
namespace DragonWilds::Quests {
inline bool StageReceiptActive(const QuestNative::Adapter& api,const Json& document) {
    const auto read=[&](const char* key){return api.GetInt(RC::Unreal::FName(RC::to_generic_string(key).c_str(),RC::Unreal::FNAME_Add));};
    if(read("RuneSchema.Version")!=7001 || read("RuneSchema.Phase")!=1)return false;
    // Authority migrates changed definitions through NativeReceipt. Read-only
    // presentation can briefly observe the old replicated receipt first; hide
    // its stages until the clean reset arrives instead of surfacing a fatal
    // "migration required" error.
    return DefinitionIdentity::Compare(document,read)!=DefinitionIdentity::Match::Changed;
}
inline Stages::Graph StageGraph(const Definition& quest) {
    Stages::Graph graph;
    for(const auto& [id,objectives]:quest.Stages) {
        Stages::Stage stage{id,{},quest.AnyStages.contains(id)};
        for(const auto& definition:objectives) {
            const auto local=definition.ObjectiveId.substr(id.size()+1);
            Stages::Objective objective{local,definition.Kill?Stages::Kind::Kill:definition.Acquire?Stages::Kind::Acquire:Stages::Kind::TurnIn,
                definition.Kill?definition.Kill->Classes:std::vector<std::string>{definition.Required.Item},definition.Required.Count,{}};
            if(definition.Marker && definition.Marker->RadiusMeters)objective.Region=Stages::Area{definition.Marker->Position,*definition.Marker->RadiusMeters};
            objective.Hidden=definition.Hidden;objective.Optional=definition.Optional;
            stage.Objectives.push_back(std::move(objective));
        }
        graph.Stages.push_back(std::move(stage));
    }
    return graph;
}
inline RC::Unreal::FName StageCounter(const Definition& objective) {
    return RC::Unreal::FName(RC::to_generic_string("RuneSchema.Objective:"+objective.ObjectiveId).c_str(),RC::Unreal::FNAME_Add);
}
inline auto StageProgress(const QuestNative::Adapter& api,const Definition& quest,int run,bool requireInitialized=true) {
    return Stages::OpenNative(api,StageGraph(quest),run,requireInitialized);
}
inline void SetStageText(const QuestNative::Adapter& api,const Definition& quest,size_t stage) {
    const auto id=stage<quest.Stages.size()?quest.Stages[stage].first:std::string("__ready");
    api.SetObjective(RC::Unreal::FName(RC::to_generic_string(id).c_str(),RC::Unreal::FNAME_Add));
}
}
