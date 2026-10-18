#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "Loader/QuestDefinition.h"
namespace DragonWilds::Quests {
inline nlohmann::json ObjectivePresentation(const Definition& objective) {
    nlohmann::json value={{"Description",objective.ObjectiveText}};
    if(!objective.SubobjectiveDescription.empty())value["SubobjectiveDescription"]=objective.SubobjectiveDescription;
    if(!objective.Hint.empty())value["Hint"]=objective.Hint;
    if(!objective.QuestDescriptionOverride.empty()) {
        value["bOverrideQuestDescription"]=true;
        value["QuestDescriptionOverride"]=objective.QuestDescriptionOverride;
    }
    return value;
}
inline nlohmann::json ObjectivePayload(nlohmann::json rows,bool structured) {
    if(!rows.is_array())throw std::runtime_error("Quest objectives must be map rows");
    for(auto& row:rows) {
        if(!row.is_object()||!row.contains("Key")||!row["Key"].is_string()
            ||!row.contains("Value")||(!row["Value"].is_string()&&!row["Value"].is_object()))
            throw std::runtime_error("Quest objective text row is malformed");
        if(row["Value"].is_object()) {
            const auto& value=row["Value"];
            if(!value.contains("Description")||!value["Description"].is_string())throw std::runtime_error("Quest objective description is missing");
            for(const auto& [key,field]:value.items()) {
                if(key=="bOverrideQuestDescription") {if(!field.is_boolean())throw std::runtime_error("Quest objective override flag must be boolean");}
                else if((key!="Description"&&key!="SubobjectiveDescription"&&key!="Hint"&&key!="QuestDescriptionOverride")||!field.is_string())
                    throw std::runtime_error("Quest objective presentation field is invalid");
            }
            if(!structured) {
                if(value.size()!=1)throw std::runtime_error("Rich objective presentation requires native QuestObjectiveData");
                row["Value"]=value["Description"];
            }
        } else if(structured)row["Value"]=nlohmann::json{{"Description",row["Value"]}};
    }
    return rows;
}
}
