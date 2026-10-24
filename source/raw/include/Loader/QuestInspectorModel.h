#pragma once
#include "Loader/QuestDefinition.h"
namespace DragonWilds::Quests {
inline Json BoardMetadata(const Definition& quest){
    return {{"Key",quest.Key},{"Mod",quest.Key.substr(0,quest.Key.find(':'))},{"PersistenceID",quest.PersistenceId},
        {"Title",quest.Title},{"Description",quest.Description},{"Icon",quest.Icon},{"Category",quest.Story?"Story":quest.Task?"Task":"Regular"},
        {"Region",quest.Region},{"Repeatable",quest.Repeat.Enabled}};
}
inline Json InspectorModel(const Definition& quest) {
    Json result={{"Key",quest.Key},{"Title",quest.Title},{"Description",quest.Description},
        {"CompletedDescription",quest.CompletedDescription.empty()?quest.Description:quest.CompletedDescription},
        {"HideInQuestList",quest.HideInQuestList},{"ShowInCompletedQuestList",quest.ShowInCompletedQuestList},
        {"Category",quest.Story?"Story":quest.Task?"Task":"Regular"},
        {"Region",quest.Region},{"Icon",quest.Icon},
        {"Repeatable",quest.Repeat.Enabled},{"Stages",Json::array()},{"Timeline",Json::array()}};
    const auto objective=[](const Definition& value,bool staged) -> Json {
        Json data={{"Id",value.ObjectiveId},{"Text",value.ObjectiveText},{"Required",value.Required.Count},
            {"Hidden",value.Hidden},{"Optional",value.Optional},{"Kind",value.Kill?"Kill":value.Acquire?"Acquire":"TurnIn"},
            {"SubobjectiveDescription",value.SubobjectiveDescription},{"Hint",value.Hint},{"QuestDescriptionOverride",value.QuestDescriptionOverride},
            {"Counter",staged?"RuneSchema.Objective:"+value.ObjectiveId:value.ObjectiveId}};
        if(value.Marker)data["Area"]={{"Location",value.Marker->Position},{"RadiusMeters",value.Marker->RadiusMeters.value_or(0)}};
        return data;
    };
    if(quest.Stages.empty())result["Timeline"].push_back({{"Id",quest.ObjectiveId},{"Objectives",Json::array({objective(quest,false)})}});
    else for(const auto& [id,entries]:quest.Stages) {
        result["Stages"].push_back(id);
        auto objectives=Json::array();for(const auto& entry:entries)objectives.push_back(objective(entry,true));
        result["Timeline"].push_back({{"Id",id},{"Completion",quest.AnyStages.contains(id)?"Any":"All"},{"Objectives",std::move(objectives)}});
    }
    return result;
}
}
