#pragma once
#include "Loader/QuestDefinition.h"
namespace DragonWilds::Quests {
inline Json InspectorModel(const Definition& quest) {
    Json result={{"Key",quest.Key},{"Title",quest.Title},{"Description",quest.Description},
        {"Repeatable",quest.Repeat.Enabled},{"Stages",Json::array()},{"Timeline",Json::array()}};
    const auto objective=[](const Definition& value,bool staged) -> Json {
        Json data={{"Id",value.ObjectiveId},{"Text",value.ObjectiveText},{"Required",value.Required.Count},
            {"Hidden",value.Hidden},{"Optional",value.Optional},{"Kind",value.Kill?"Kill":value.Acquire?"Acquire":"TurnIn"},
            {"Counter",staged?"RuneSchema.Objective:"+value.ObjectiveId:value.ObjectiveId}};
        if(value.Marker)data["Area"]={{"Location",value.Marker->Position},{"RadiusMeters",value.Marker->RadiusMeters.value_or(0)}};
        return data;
    };
    if(quest.Stages.empty())result["Timeline"].push_back({{"Id",quest.ObjectiveId},{"Objectives",Json::array({objective(quest,false)})}});
    else for(const auto& [id,entries]:quest.Stages) {
        result["Stages"].push_back(id);
        auto objectives=Json::array();for(const auto& entry:entries)objectives.push_back(objective(entry,true));
        result["Timeline"].push_back({{"Id",id},{"Objectives",std::move(objectives)}});
    }
    return result;
}
}
