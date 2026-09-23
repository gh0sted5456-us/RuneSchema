#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>
#include "Loader/HumanPose.h"
#include "Loader/NpcVisualEffect.h"

namespace DragonWilds::Dialogue {
using Json=nlohmann::json;
inline void Fields(const Json& value,std::initializer_list<const char*> names) {
    if(!value.is_object())throw std::runtime_error("Dialogue entry must be an object");
    std::set<std::string> allowed(names.begin(),names.end());
    for(const auto& [key,child]:value.items())
        if(!allowed.contains(key))throw std::runtime_error("Unsupported dialogue field: "+key);
}
inline std::string Text(const Json& value,size_t maximum) {
    if(!value.is_string())throw std::runtime_error("Dialogue requires text");
    auto result=value.get<std::string>();
    if(result.empty() || result.size()>maximum || result.find('\0')!=result.npos)
        throw std::runtime_error("Dialogue text length is invalid");
    return result;
}
inline std::string Id(const Json& value) {
    auto result=Text(value,128);
    if(result.find_first_of(":\r\n\t ")!=result.npos)throw std::runtime_error("Invalid dialogue identifier");
    return result;
}
inline std::string Reference(const std::string& mod,const Json& value) {
    const auto ref=Text(value,257);
    const auto colon=ref.find(':');
    const auto owner=colon==ref.npos?mod:ref.substr(0,colon);
    if(owner.empty() || owner.size()>128 || owner.find_first_of(":\r\n\t")!=owner.npos)
        throw std::runtime_error("Invalid dialogue mod name");
    return owner+":"+Id(colon==ref.npos?ref:ref.substr(colon+1));
}
struct Definition {
    std::string Key;
    Json Data;
    std::set<std::string> Stores;
    std::set<std::string> Quests;
    std::set<std::string> Events;
};
inline Definition Parse(const std::string& mod,const Json& data) {
    Fields(data,{"Id","Entry","Nodes","Completion","CompletedEntry"});
    Definition result{Reference(mod,data.at("Id")),data,{}};
    const auto entry=Id(data.at("Entry"));
    const auto& nodes=data.at("Nodes");
    if(!nodes.is_object() || nodes.empty() || nodes.size()>128)
        throw std::runtime_error("Dialogue requires 1..128 nodes");
    if(!nodes.contains(entry))throw std::runtime_error("Dialogue entry node is missing");
    if(data.contains("CompletedEntry") && (!data.contains("Completion") || !nodes.contains(Id(data.at("CompletedEntry")))))
        throw std::runtime_error("CompletedEntry requires Completion and an existing node");
    if(data.contains("Completion")) {
        const auto& completion=data.at("Completion");
        Fields(completion,{"Flag","Item","Count"});
        Reference(mod,completion.at("Flag"));
        const auto item=Text(completion.at("Item"),1024);
        if(item.front()!='/' || item.find_first_of("\r\n\t")!=item.npos)throw std::runtime_error("Completion Item requires an Unreal asset path");
        if(!completion.at("Count").is_number_integer() || completion.at("Count").get<int64_t>()<1 || completion.at("Count").get<int64_t>()>999)
            throw std::runtime_error("Completion Count requires an integer from 1 to 999");
    }
    size_t completionChoices=0;
    for(const auto& [nodeId,node]:nodes.items()) {
        Id(nodeId);
        Fields(node,{"Text","Choices","Pose","Emote"});
        if(node.contains("Pose") && HumanPose::Parse(node.at("Pose")).Path.empty())
            throw std::runtime_error("Dialogue Pose requires a concrete preset or Asset; Equipment is unavailable");
        if(node.contains("Emote")) {
            auto emote=HumanPose::Parse(node.at("Emote"));
            if(emote.Path.empty())throw std::runtime_error("Dialogue Emote requires a concrete preset or Asset");
            if(emote.Mode!=HumanPose::Playback::Once)throw std::runtime_error("Dialogue Emote requires Playback Once");
        }
        Text(node.at("Text"),4096);
        const auto& choices=node.at("Choices");
        if(!choices.is_array() || choices.empty() || choices.size()>4)
            throw std::runtime_error("Dialogue requires 1..4 choices per node");
        std::set<std::string> choiceIds;
        for(const auto& choice:choices) {
            Fields(choice,{"Id","Text","Next","VendorID","End","Complete","Quest","Event","NpcGoAway","VisualEffect","WhenQuest","WhenTimeOfDay","RequirementUnlock"});
            if(choice.contains("VisualEffect")) {
                const auto& visual=choice.at("VisualEffect");Fields(visual,{"Action","Effect"});
                const auto verb=Text(visual.at("Action"),16);
                if(verb!="Activate" && verb!="Deactivate")throw std::runtime_error("Dialogue VisualEffect.Action must be Activate or Deactivate");
                if((verb=="Activate")!=visual.contains("Effect"))throw std::runtime_error("Dialogue VisualEffect Activate requires Effect; Deactivate must omit it");
                if(verb=="Activate") {
                    NpcVisualEffect::Validate(visual.at("Effect"));
                    if(visual.at("Effect").value("Type",std::string{})!="Niagara")throw std::runtime_error("Dialogue action VisualEffect currently supports Niagara only");
                }
            }
            if(choice.contains("NpcGoAway"))Reference(mod,choice.at("NpcGoAway"));
            if(choice.contains("RequirementUnlock")) {
                if(choice.contains("WhenQuest") || choice.contains("WhenTimeOfDay"))
                    throw std::runtime_error("RequirementUnlock cannot be combined with legacy WhenQuest/WhenTimeOfDay gates");
                const auto& unlock=choice.at("RequirementUnlock");
                Fields(unlock,{"Quest","TimeOfDay"});
                if(unlock.empty())throw std::runtime_error("RequirementUnlock requires Quest and/or TimeOfDay");
                if(unlock.contains("TimeOfDay")) {
                    const auto phase=Text(unlock.at("TimeOfDay"),16);
                    if(phase!="Day" && phase!="Night")throw std::runtime_error("RequirementUnlock.TimeOfDay must be Day or Night");
                }
            }
            if(choice.contains("WhenTimeOfDay")) {
                const auto phase=Text(choice.at("WhenTimeOfDay"),16);
                if(phase!="Day" && phase!="Night")throw std::runtime_error("WhenTimeOfDay must be Day or Night");
            }
            const Json* questGate=choice.contains("WhenQuest")?&choice.at("WhenQuest"):
                choice.contains("RequirementUnlock") && choice.at("RequirementUnlock").contains("Quest")?&choice.at("RequirementUnlock").at("Quest"):nullptr;
            if(questGate) {
                const auto& gate=*questGate;Fields(gate,{"Id","States","Stage","ObjectivesComplete","RepeatReady"});
                if(gate.contains("Stage"))Id(gate.at("Stage"));
                for(const auto* field:{"ObjectivesComplete","RepeatReady"})
                    if(gate.contains(field) && !gate.at(field).is_boolean())throw std::runtime_error("WhenQuest conditions must be boolean");
                result.Quests.insert(Reference(mod,gate.at("Id")));
                const auto& states=gate.at("States");
                if(!states.is_array() || states.empty() || states.size()>3)throw std::runtime_error("WhenQuest.States requires 1..3 quest states");
                std::set<std::string> unique;
                for(const auto& state:states) {
                    const auto name=Text(state,16);
                    if((name!="NotStarted" && name!="Active" && name!="Completed") || !unique.insert(name).second)
                        throw std::runtime_error("WhenQuest states must be unique NotStarted, Active or Completed values");
                }
            }
            if(!choiceIds.insert(Id(choice.at("Id"))).second)throw std::runtime_error("Duplicate dialogue choice");
            Text(choice.at("Text"),512);
            if(int(choice.contains("Next"))+int(choice.contains("VendorID"))+int(choice.contains("End"))!=1)
                throw std::runtime_error("Choice requires exactly one destination: Next, VendorID or End");
            if(choice.contains("Next") && !nodes.contains(Id(choice.at("Next"))))
                throw std::runtime_error("Dialogue choice references missing node");
            if(choice.contains("Complete")) {
                if(choice["Complete"]!=true || !data.contains("Completion") || !choice.contains("Next"))
                    throw std::runtime_error("Complete:true requires Completion and a Next response node");
                ++completionChoices;
            }
            if(choice.contains("VendorID"))result.Stores.insert(Reference(mod,choice.at("VendorID")));
            if(choice.contains("Quest")) {
                if((!choice.contains("Next") && !choice.contains("End")) || choice.contains("Complete"))throw std::runtime_error("Quest choice requires Next or End and cannot also complete a story reward");
                const auto& action=choice.at("Quest");Fields(action,{"Id","Action","EntryID"});
                if(action.contains("EntryID")) {
                    Id(action.at("EntryID"));
                    if(action.at("Action")!="Accept")throw std::runtime_error("Quest EntryID is only valid for Accept");
                }
                const auto verb=Text(action.at("Action"),16);
                if(verb!="Accept" && verb!="TurnIn" && verb!="Abandon")throw std::runtime_error("Quest action must be Accept, TurnIn or Abandon");
                result.Quests.insert(Reference(mod,action.at("Id")));
            }
            if(choice.contains("Event")) {
                if((!choice.contains("Next") && !choice.contains("End")) || choice.contains("Complete"))throw std::runtime_error("Event choice requires Next or End and cannot grant a story reward");
                const auto& action=choice.at("Event");Fields(action,{"Id","Action"});
                const auto verb=Text(action.at("Action"),16);
                if(verb!="Start" && verb!="Cancel")throw std::runtime_error("Event action must be Start or Cancel");
                if(choice.contains("Quest")) {
                    const auto& quest=choice.at("Quest");
                    const auto questVerb=quest.at("Action").get<std::string>();
                    if(!((questVerb=="Accept" && verb=="Start") || (questVerb=="Abandon" && verb=="Cancel")))
                        throw std::runtime_error("A combined quest/event choice must Accept/Start or Abandon/Cancel");
                }
                result.Events.insert(Reference(mod,action.at("Id")));
            }
            if(choice.contains("End") && (!choice["End"].is_boolean() || !choice["End"].get<bool>()))
                throw std::runtime_error("End must be true");
        }
        bool fallback=false;
        for(const auto& choice:choices)if(!choice.contains("WhenQuest") && !choice.contains("WhenTimeOfDay") && !choice.contains("RequirementUnlock"))fallback=true;
        if(!fallback)throw std::runtime_error("Dialogue node requires an ungated choice so the player can leave or navigate");
    }
    std::set<std::string> reached;
    std::vector<std::string> pending{entry};
    if(data.contains("CompletedEntry"))pending.push_back(data.at("CompletedEntry").get<std::string>());
    while(!pending.empty()) {
        auto node=std::move(pending.back());pending.pop_back();
        if(!reached.insert(node).second)continue;
        for(const auto& choice:nodes.at(node).at("Choices"))
            if(choice.contains("Next"))pending.push_back(choice.at("Next").get<std::string>());
    }
    if(reached.size()!=nodes.size())throw std::runtime_error("Dialogue contains unreachable nodes");
    if(data.contains("Completion") && !completionChoices)throw std::runtime_error("Completion requires a Complete choice");
    return result;
}
inline void ValidateStores(const Definition& definition,const std::set<std::string>& available) {
    for(const auto& store:definition.Stores)
        if(!available.contains(store))throw std::runtime_error("Dialogue references missing or disabled store: "+store);
}
inline void ValidateNpcStore(const Definition& definition,const std::string& owner) {
    for(const auto& store:definition.Stores)
        if(owner!="store:"+store)throw std::runtime_error("Dialogue shop choices must reference this NPC's enabled VendorID: "+store);
}
}
