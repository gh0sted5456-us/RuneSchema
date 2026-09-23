#pragma once
#include "Loader/QuestDefinition.h"
namespace DragonWilds::Quests {
inline std::string ObjectiveProgressText(const Definition& objective,int value,int run=1) {
    if(value<0 || value>objective.Required.Count)throw std::runtime_error("Invalid quest display counter");
    if(run<1)throw std::runtime_error("Invalid quest display run");
    std::string text=value==objective.Required.Count && !objective.CompleteText.empty()?objective.CompleteText:
        !objective.ProgressText.empty()?objective.ProgressText:objective.ObjectiveText+" ({current}/{required})";
    const auto replace=[&](const std::string& token,int replacement) {
        const auto valueText=std::to_string(replacement);
        for(size_t at=0;(at=text.find(token,at))!=std::string::npos;at+=valueText.size())text.replace(at,token.size(),valueText);
    };
    replace("{current}",value);replace("{required}",objective.Required.Count);replace("{remaining}",objective.Required.Count-value);
    replace("{run}",run);
    return text;
}
template<class Read> std::optional<std::string> ProgressText(const Definition& quest,const std::string& objectiveId,Read read) {
    if(objectiveId=="__ready")return std::nullopt;
    std::string text;
    const auto append=[&](const Definition& objective) {
        if(objective.Hidden)return;
        const int value=read(objective);
        if(value<0 || value>objective.Required.Count)throw std::runtime_error("Invalid quest display counter");
        if(!text.empty())text+='\n';
        text+=ObjectiveProgressText(objective,value);
    };
    if(quest.Stages.empty()) {
        if(objectiveId!=quest.ObjectiveId)return std::nullopt;
        append(quest);
    } else {
        const auto stage=std::find_if(quest.Stages.begin(),quest.Stages.end(),[&](const auto& value){return value.first==objectiveId;});
        if(stage==quest.Stages.end())return std::nullopt;
        for(const auto& objective:stage->second)append(objective);
    }
    return text.empty()?std::nullopt:std::optional<std::string>{text};
}
}
