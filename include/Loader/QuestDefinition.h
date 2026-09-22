#pragma once
#include "Loader/DialogueDefinition.h"
#include "Loader/ItemIdentity.h"
#include "Loader/QuestRepeatPolicy.h"
#include "Loader/TimeOfDay.h"
#include <map>
#include <array>
#include <cmath>
#include <optional>
#include <algorithm>
#include <functional>

namespace DragonWilds::Quests {
using Json=nlohmann::json;
struct ItemAmount { std::string Item; int Count; };
struct EntryOption {std::string Id;ItemAmount Cost;};
struct RewardTier {std::string Id,EntryId;int Priority=0;std::optional<int64_t> MaxElapsedSeconds;ItemAmount Reward;std::vector<std::string> Objectives;};
struct Location { std::string Name; std::array<double,3> Position; std::optional<double> RadiusMeters; };
struct KillTarget { std::vector<std::string> Classes; bool IncludeDerived=true; std::string EventKey,SpawnKey; };
inline float RadiusCentimeters(double meters) {
    if(!std::isfinite(meters) || meters<0.01 || meters>10000)
        throw std::runtime_error("Quest RadiusMeters must be from 0.01 to 10000");
    return static_cast<float>(meters*100.0);
}
struct Definition {
    std::string Key, PersistenceId, Title, Description, ObjectiveId, ObjectiveText;
    ItemAmount Required, Reward;
    std::optional<Location> Marker;
    std::optional<KillTarget> Kill;
    RepeatPolicy Repeat;
    std::optional<ItemAmount> StartCost;
    std::vector<EntryOption> EntryOptions;
    std::vector<RewardTier> ResultTiers;
    std::vector<std::pair<std::string,std::vector<Definition>>> Stages;
    bool Story=false,Task=false;
    std::vector<std::string> Prerequisites;
    std::optional<ItemAmount> RepeatReward;
    bool Hidden=false,Optional=false;
    bool Acquire=false;
    bool AutomaticReward=false;
    TimeOfDay::Requirement Time=TimeOfDay::Requirement::Any;
    std::string ProgressText,CompleteText;
    bool AnnounceProgress=false;
};
inline ItemAmount ParseItem(const Json& data) {
    const auto item=Dialogue::Text(data.at("Item"),1024);
    if(item.front()!='/' || item.find_first_of("\r\n\t")!=item.npos)
        throw std::runtime_error("Quest item requires an Unreal asset path");
    const auto& count=data.at("Count");
    if(!count.is_number_integer() || count<1 || count>999)
        throw std::runtime_error("Quest item count must be an integer from 1 to 999");
    return {item,count.get<int>()};
}
inline Definition ParseDefinition(const std::string& mod,const Json& data,bool stageObjective) {
    Dialogue::Fields(data,{"Id","PersistenceID","Title","Description","Objective","Stages","Category","Prerequisites","Reward","RepeatReward","Repeatable","Repeat","StartCost","EntryOptions","ResultTiers","Completion","TimeOfDay"});
    if(data.contains("Stages")) {
        if(data.contains("Objective"))throw std::runtime_error("Quest cannot combine Objective and Stages");
        const auto& stages=data.at("Stages");
        if(!stages.is_array() || stages.empty() || stages.size()>32)throw std::runtime_error("Quest requires 1..32 stages");
        auto base=data;base.erase("Stages");
        base["Objective"]=stages.at(0).at("Objectives").at(0);
        auto result=ParseDefinition(mod,base,true);
        std::set<std::string> ids;size_t total=0;
        for(const auto& stage:stages) {
            Dialogue::Fields(stage,{"Id","Objectives"});const auto id=Dialogue::Id(stage.at("Id"));
            if(id=="__ready" || id.size()>64 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=id.npos || !ids.insert(id).second)
                throw std::runtime_error("Stage IDs must be unique letters, digits, underscores or hyphens");
            const auto& objectives=stage.at("Objectives");
            if(!objectives.is_array() || objectives.empty() || objectives.size()>16 || (total+=objectives.size())>128)throw std::runtime_error("Stage objective limit exceeded");
            std::set<std::string> objectiveIds;std::vector<Definition> parsed;
            for(const auto& objective:objectives) {
                base["Objective"]=objective;auto child=ParseDefinition(mod,base,true);
                const auto local=child.ObjectiveId;
                if(local.size()>64 || local.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=local.npos || !objectiveIds.insert(local).second)
                    throw std::runtime_error("Invalid or duplicate stage objective ID");
                child.Key=result.Key+":"+id+":"+local;
                child.ObjectiveId=id+":"+local;
                if(child.Marker)child.Marker->Name="RuneSchema_Quest_"+result.PersistenceId+"_"+id+"_"+local;
                parsed.push_back(std::move(child));
            }
            if(std::none_of(parsed.begin(),parsed.end(),[](const auto& objective){return !objective.Optional;}))
                throw std::runtime_error("Each stage requires a mandatory objective");
            result.Stages.emplace_back(id,std::move(parsed));
        }
        for(const auto& tier:result.ResultTiers)for(const auto& required:tier.Objectives) {
            bool found=false;
            for(const auto& [id,objectives]:result.Stages)for(const auto& objective:objectives)
                found|=objective.ObjectiveId==required;
            if(!found)throw std::runtime_error("Reward tier references an unknown stage objective");
        }
        result.Kill.reset();result.Marker.reset();result.Required={"",1};
        result.Hidden=false;result.Optional=false;result.Acquire=false;
        return result;
    }
    const auto persistence=Dialogue::Text(data.at("PersistenceID"),22);
    if(!IsCanonicalPersistenceId(persistence))
        throw std::runtime_error("Quest PersistenceID must be a canonical 22-character identity");
    const auto& objective=data.at("Objective");
    Dialogue::Fields(objective,{"Id","Text","Item","Count","Location","RadiusMeters","Type","AIClasses","IncludeDerived","EventID","SpawnID","Hidden","Optional","ProgressText","CompleteText","AnnounceProgress"});
    Dialogue::Fields(data.at("Reward"),{"Item","Count"});
    const auto kind=objective.value("Type",std::string("Collect"));
    if(kind!="Collect" && kind!="Kill" && kind!="Acquire")throw std::runtime_error("Quest objective Type must be Collect, Acquire or Kill");
    ItemAmount required;
    std::optional<KillTarget> kill;
    if(kind!="Kill") {
        if(objective.contains("AIClasses") || objective.contains("IncludeDerived") || objective.contains("EventID") || objective.contains("SpawnID"))throw std::runtime_error("Collect objective cannot use kill filters");
        required=ParseItem(objective);
    } else {
        if(objective.contains("Item"))throw std::runtime_error("Kill objective cannot require an item");
        const auto& count=objective.at("Count");
        if(!count.is_number_integer() || count<1 || count>999)throw std::runtime_error("Kill count must be an integer from 1 to 999");
        required={"",count.get<int>()};
        KillTarget target;
        const auto& classes=objective.at("AIClasses");
        if(!classes.is_array() || classes.empty() || classes.size()>32)throw std::runtime_error("Kill objective requires 1..32 AIClasses");
        std::set<std::string> unique;
        for(const auto& entry:classes) {
            const auto path=Dialogue::Text(entry,1024);
            if(path.front()!='/' || path.find_first_of("\r\n\t")!=path.npos || !unique.insert(path).second)throw std::runtime_error("Kill AIClasses require unique Unreal class paths");
            target.Classes.push_back(path);
        }
        if(objective.contains("IncludeDerived")) {
            if(!objective["IncludeDerived"].is_boolean())throw std::runtime_error("IncludeDerived must be boolean");
            target.IncludeDerived=objective["IncludeDerived"].get<bool>();
        }
        if(objective.contains("EventID"))target.EventKey=Dialogue::Reference(mod,objective["EventID"]);
        if(objective.contains("SpawnID")) {
            if(target.EventKey.empty())throw std::runtime_error("SpawnID currently requires EventID for verified actor provenance");
            target.SpawnKey=Dialogue::Reference(mod,objective["SpawnID"]);
        }
        kill=std::move(target);
    }
    Definition result{Dialogue::Reference(mod,data.at("Id")),persistence,
        Dialogue::Text(data.at("Title"),256),Dialogue::Text(data.at("Description"),4096),
        Dialogue::Id(objective.at("Id")),Dialogue::Text(objective.at("Text"),1024),
        required,ParseItem(data.at("Reward"))};
    result.Kill=std::move(kill);
    result.Acquire=kind=="Acquire";
    const auto completion=data.value("Completion",std::string("ReturnToNPC"));
    if(completion!="ReturnToNPC" && completion!="Automatic")throw std::runtime_error("Quest Completion must be ReturnToNPC or Automatic");
    result.AutomaticReward=completion=="Automatic";
    if(data.contains("TimeOfDay"))result.Time=TimeOfDay::Parse(Dialogue::Text(data.at("TimeOfDay"),16));
    const auto category=data.value("Category",std::string("Regular"));
    if(category!="Regular" && category!="Story" && category!="Task")throw std::runtime_error("Quest Category must be Regular, Story, or Task");
    result.Story=category=="Story";
    result.Task=category=="Task";
    if(data.contains("Prerequisites")) {
        const auto& prerequisites=data.at("Prerequisites");
        if(!prerequisites.is_array() || prerequisites.size()>32)throw std::runtime_error("Quest Prerequisites must be an array of at most 32 IDs");
        std::set<std::string> unique;
        for(const auto& reference:prerequisites) {
            const auto key=Dialogue::Reference(mod,reference);
            if(key==result.Key || !unique.insert(key).second)throw std::runtime_error("Invalid duplicate or self quest prerequisite");
            result.Prerequisites.push_back(key);
        }
    }
    result.Repeat=ParseRepeat(data);
    if(data.contains("StartCost")) {
        Dialogue::Fields(data.at("StartCost"),{"Item","Count"});
        result.StartCost=ParseItem(data.at("StartCost"));
    }
    if(data.contains("EntryOptions")) {
        const auto& entries=data.at("EntryOptions");
        if(result.StartCost || !entries.is_array() || entries.empty() || entries.size()>8)throw std::runtime_error("EntryOptions requires 1..8 choices and cannot combine with StartCost");
        std::set<std::string> ids;
        for(const auto& entry:entries) {
            Dialogue::Fields(entry,{"Id","Cost"});Dialogue::Fields(entry.at("Cost"),{"Item","Count"});
            const auto id=Dialogue::Id(entry.at("Id"));
            if(!ids.insert(id).second)throw std::runtime_error("Duplicate quest entry option");
            result.EntryOptions.push_back({id,ParseItem(entry.at("Cost"))});
        }
    }
    for(const auto* flag:{"Hidden","Optional"})if(objective.contains(flag) && !objective.at(flag).is_boolean())
        throw std::runtime_error("Objective Hidden/Optional must be boolean");
    result.Hidden=objective.value("Hidden",false);result.Optional=objective.value("Optional",false);
    if(objective.contains("ProgressText"))result.ProgressText=Dialogue::Text(objective.at("ProgressText"),1024);
    if(objective.contains("CompleteText"))result.CompleteText=Dialogue::Text(objective.at("CompleteText"),1024);
    if(objective.contains("AnnounceProgress")) {
        if(!objective.at("AnnounceProgress").is_boolean())throw std::runtime_error("AnnounceProgress must be boolean");
        result.AnnounceProgress=objective.at("AnnounceProgress").get<bool>();
    }
    if(result.Hidden && !result.Optional)throw std::runtime_error("Hidden objectives must be optional");
    if(!stageObjective && (result.Hidden || result.Optional))
        throw std::runtime_error("Hidden/optional objectives require Stages with a mandatory objective");
    if(data.contains("RepeatReward")) {
        if(!result.Repeat.Enabled)throw std::runtime_error("RepeatReward requires a repeatable quest");
        Dialogue::Fields(data.at("RepeatReward"),{"Item","Count"});
        result.RepeatReward=ParseItem(data.at("RepeatReward"));
    }
    if(data.contains("ResultTiers")) {
        const auto& tiers=data.at("ResultTiers");
        if(!tiers.is_array() || tiers.empty() || tiers.size()>16)throw std::runtime_error("ResultTiers requires 1..16 tiers");
        std::set<std::string> ids;std::set<int> priorities;
        for(const auto& entry:tiers) {
            Dialogue::Fields(entry,{"Id","Priority","EntryID","MaxElapsedSeconds","Reward","RequiresObjectives"});
            Dialogue::Fields(entry.at("Reward"),{"Item","Count"});
            RewardTier tier;tier.Id=Dialogue::Id(entry.at("Id"));tier.Reward=ParseItem(entry.at("Reward"));
            const auto& priority=entry.at("Priority");
            if(!priority.is_number_integer() || priority<-100000 || priority>100000)throw std::runtime_error("Tier Priority must be an integer from -100000 to 100000");
            tier.Priority=priority.get<int>();
            if(!ids.insert(tier.Id).second || !priorities.insert(tier.Priority).second)throw std::runtime_error("Tier IDs and priorities must be unique");
            if(entry.contains("EntryID")) {
                tier.EntryId=Dialogue::Id(entry.at("EntryID"));
                if(std::none_of(result.EntryOptions.begin(),result.EntryOptions.end(),[&](const auto& e){return e.Id==tier.EntryId;}))throw std::runtime_error("Tier references unknown entry option");
            }
            if(entry.contains("MaxElapsedSeconds")) {
                const auto& seconds=entry.at("MaxElapsedSeconds");
                if(!seconds.is_number_integer() || seconds<1 || seconds>31536000)throw std::runtime_error("Tier MaxElapsedSeconds must be 1..31536000");
                tier.MaxElapsedSeconds=seconds.get<int64_t>();
            }
            if(entry.contains("RequiresObjectives")) {
                if(!stageObjective)throw std::runtime_error("RequiresObjectives requires a staged quest");
                const auto& refs=entry.at("RequiresObjectives");
                if(!refs.is_array() || refs.empty() || refs.size()>128)throw std::runtime_error("RequiresObjectives requires 1..128 stage:objective references");
                std::set<std::string> unique;
                for(const auto& ref:refs) {
                    const auto id=Dialogue::Text(ref,129);
                    if(id.find(':')==id.npos || !unique.insert(id).second)throw std::runtime_error("Invalid or duplicate objective requirement");
                    tier.Objectives.push_back(id);
                }
            }
            result.ResultTiers.push_back(std::move(tier));
        }
    }
    if(objective.contains("Location")) {
        const auto& position=objective.at("Location");
        if(!position.is_array() || position.size()!=3)throw std::runtime_error("Quest Location requires [X,Y,Z]");
        Location marker{"RuneSchema_Quest_"+persistence+"_"+result.ObjectiveId,{}};
        for(size_t i=0;i<3;++i) {
            if(!position[i].is_number())throw std::runtime_error("Quest Location coordinates must be numbers");
            marker.Position[i]=position[i].get<double>();
            if(!std::isfinite(marker.Position[i]) || std::abs(marker.Position[i])>100000000)
                throw std::runtime_error("Quest Location coordinate is outside supported bounds");
        }
        result.Marker=std::move(marker);
    }
    if(objective.contains("RadiusMeters")) {
        const auto& radius=objective.at("RadiusMeters");
        if(!result.Marker || !radius.is_number())throw std::runtime_error("Quest RadiusMeters requires Location and a numeric radius");
        const double meters=radius.get<double>();
        RadiusCentimeters(meters);
        result.Marker->RadiusMeters=meters;
    }
    return result;
}
inline Definition Parse(const std::string& mod,const Json& data) {
    return ParseDefinition(mod,data,false);
}
inline ItemAmount RewardForRun(const Definition& quest,int run,int tier) {
    if(run<1 || tier<0 || tier>static_cast<int>(quest.ResultTiers.size()))throw std::runtime_error("Invalid quest reward run or tier");
    ItemAmount reward=tier?quest.ResultTiers.at(tier-1).Reward:(run>1 && quest.RepeatReward?*quest.RepeatReward:quest.Reward);
    if(run>1 && quest.Repeat.RewardMultiplierPerRun>0) {
        const auto multiplier=std::min(quest.Repeat.MaximumRewardMultiplier,
            1.0+(run-1)*quest.Repeat.RewardMultiplierPerRun);
        const auto scaled=std::llround(reward.Count*multiplier);
        if(scaled<1 || scaled>999)throw std::runtime_error("Scaled repeat reward count must remain from 1 to 999");
        reward.Count=static_cast<int>(scaled);
    }
    return reward;
}
inline int SelectRewardTier(const Definition& quest,const std::string& entry,std::optional<int64_t> elapsed,
    const std::function<bool(const std::string&)>& satisfied={}) {
    int selected=0,priority=std::numeric_limits<int>::min();
    if(elapsed && *elapsed<0)throw std::runtime_error("Invalid quest performance metrics");
    for(size_t i=0;i<quest.ResultTiers.size();++i) {
        const auto& tier=quest.ResultTiers[i];
        if(!tier.EntryId.empty() && tier.EntryId!=entry)continue;
        if(tier.MaxElapsedSeconds && (!elapsed || *elapsed>*tier.MaxElapsedSeconds))continue;
        if(!tier.Objectives.empty() && (!satisfied || !std::all_of(tier.Objectives.begin(),tier.Objectives.end(),satisfied)))continue;
        if(tier.Priority>priority){priority=tier.Priority;selected=static_cast<int>(i)+1;}
    }
    return selected;
}
class Catalog {
    std::map<std::string,Definition> definitions;
    std::set<std::string> identities;
public:
    void Add(const std::string& mod,const Json& data) {
        auto definition=Parse(mod,data);
        if(definitions.contains(definition.Key) || identities.contains(definition.PersistenceId))
            throw std::runtime_error("Duplicate quest ID or PersistenceID");
        identities.insert(definition.PersistenceId);
        definitions.emplace(definition.Key,std::move(definition));
    }
    const Definition& Find(const std::string& mod,const Json& reference) const {
        const auto key=Dialogue::Reference(mod,reference);
        const auto found=definitions.find(key);
        if(found==definitions.end())throw std::runtime_error("Missing quest: "+key);
        return found->second;
    }
};
}
