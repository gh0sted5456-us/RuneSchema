#pragma once
#include "Loader/QuestScroll.h"
#include "Loader/DialogueSaveIdentity.h"

namespace DragonWilds::QuestCancellations {
inline constexpr auto Owner="RuneSchemaQuestCancellations";
inline constexpr auto Category="Quest Cancellations";
inline constexpr auto Board="/ColorsOfMoney/Core/JobBoard/Data/DT_QuestBoard.DT_QuestBoard";
using Json=nlohmann::json;
inline Json BoardPlacements(const Json& body) {
    Json result=Json::array();
    if(!body.contains("AddTo"))return result;
    if(!body.at("AddTo").is_array())return result;
    const auto& rows=body.at("AddTo");
    for(const auto& row:rows) {
        if(!row.is_object() || row.value("Row",std::string{})!="QuestBoard" || !row.value("Array",std::string{}).empty())continue;
        const auto table=row.value("DataTable",std::string{});
        if(table.empty()?row.value("Table",std::string{})!="DT_QuestBoard":table!=Board)continue;
        if(row.value("Category",std::string{})==Category)continue;
        const Json destination={{"DataTable",Board},{"Row","QuestBoard"},{"Category",Category}};
        if(std::find(result.begin(),result.end(),destination)==result.end())result.push_back(destination);
    }
    return result;
}
struct Plan {std::string Path,RecipeKey;Json Asset,Recipe;};
inline int CompletedRunsBeforeCancellation(int activeRun) {
    if(activeRun<1)throw std::runtime_error("Active cancellation requires a valid quest run");
    return activeRun-1;
}
inline std::optional<Plan> Build(const QuestScrolls::Contract& parent,const std::string& parentPath,
    const std::string& title,const Json& placements) {
    if(parent.Action!="Accept" || placements.empty())return std::nullopt;
    const auto id=DialogueSave::PersistenceIdForSeed("QuestCancellation/Item/"+parent.QuestKey);
    // Hex-encode the canonical ID so package sanitization cannot collapse '-' and '_'.
    std::string suffix;for(unsigned char c:id){constexpr char hex[]="0123456789abcdef";suffix+=hex[c>>4];suffix+=hex[c&15];}
    const auto name="quest_cancel_"+suffix;
    Plan plan;plan.Path=std::string("/Game/RuneSchema/")+Owner+"/Items/"+name+"."+name;
    plan.RecipeKey="RECIPE_"+name;
    plan.Asset={{"$Clone",parentPath},{"PersistenceID",id},{"InternalName",name},{"Name","Cancel: "+title},
        {"UseVerb","Cancel"},{"FlavourText","Read to cancel " + title + ". Clears this character's active quest progress and its encounter. No reward or refund. Inactive and completed quests are unchanged."},
        {"$QuestScroll",{{"QuestID",parent.QuestKey},{"PersistenceID",id},{"Action","Abandon"}}}};
    plan.Recipe={{"PersistenceID",DialogueSave::PersistenceIdForSeed("QuestCancellation/Recipe/"+parent.QuestKey)},
        {"Unlock",true},{"ItemsConsumed",Json::array({{{"ItemData","/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash"},{"Count",1}}})},
        {"ItemsCreated",Json::array({{{"ItemData",plan.Path},{"Count",1}}})},{"AddTo",placements}};
    return plan;
}
// Cancellation never turns in a quest or charges an inactive/completed/pending run.
// Mark the observed result before presentation, so a UI error cannot replay use.
template<class Active,class Ready,class Reset,class Confirm,class Consume,class Commit>
bool Apply(Active active,Ready ready,Reset reset,Confirm confirm,Consume consume,Commit commit) {
    if(!active() || !ready())return false;
    reset();
    if(!confirm())throw std::runtime_error("Quest cancellation was not confirmed; no scroll was consumed");
    if(!consume())throw std::runtime_error("Quest cancelled but scroll removal was not confirmed; no automatic retry or refund");
    commit();return true;
}
}
