#pragma once
#include "Loader/DialogueDefinition.h"
#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <algorithm>

namespace DragonWilds::Quests::Stages {
enum class Kind {Kill, Gather, Build, Reach, Interact, Dialogue, Event, Acquire, TurnIn};
struct Area {
    std::array<double,3> Center{};
    double RadiusMeters=1;
    bool Contains(const std::array<double,3>& point) const {
        for(double coordinate:point)if(!std::isfinite(coordinate))return false;
        // Map AORs are horizontal circles, not spheres.
        return std::hypot(point[0]-Center[0],point[1]-Center[1])<=RadiusMeters*100.0;
    }
};
struct Objective {
    std::string Id;
    Kind Type;
    std::vector<std::string> Targets;
    int Count=1;
    std::optional<Area> Region;
    bool Hidden=false,Optional=false;
};
struct Stage {std::string Id;std::vector<Objective> Objectives;};
struct Graph {std::vector<Stage> Stages;};
inline bool ValidId(const std::string& id) {
    return !id.empty() && id.size()<=64 && std::all_of(id.begin(),id.end(),[](unsigned char c){
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-';
    });
}
inline void Validate(const Graph& graph) {
    if(graph.Stages.empty() || graph.Stages.size()>32)throw std::runtime_error("Quest requires 1..32 stages");
    std::set<std::string> stages;
    size_t total=0;
    for(const auto& stage:graph.Stages) {
        if(!ValidId(stage.Id) || !stages.insert(stage.Id).second)throw std::runtime_error("Invalid or duplicate stage ID");
        if(stage.Objectives.empty() || stage.Objectives.size()>16 || (total+=stage.Objectives.size())>128)
            throw std::runtime_error("Quest objective limit exceeded");
        std::set<std::string> ids;
        if(std::none_of(stage.Objectives.begin(),stage.Objectives.end(),[](const auto& objective){return !objective.Optional;}))
            throw std::runtime_error("Stage requires a mandatory objective");
        for(const auto& objective:stage.Objectives) {
            if(objective.Hidden && !objective.Optional)throw std::runtime_error("Hidden objectives must be optional");
            if(!ValidId(objective.Id) || !ids.insert(objective.Id).second || objective.Count<1 || objective.Count>999)
                throw std::runtime_error("Invalid quest objective identity or count");
            if(objective.Type<Kind::Kill || objective.Type>Kind::TurnIn)throw std::runtime_error("Invalid objective type");
            if(objective.Type!=Kind::Reach && objective.Targets.empty())throw std::runtime_error("Objective requires targets");
            if(objective.Targets.size()>32)throw std::runtime_error("Too many objective targets");
            std::set<std::string> targets;
            for(const auto& target:objective.Targets)
                if(target.empty() || target.size()>1024 || !targets.insert(target).second)throw std::runtime_error("Invalid or duplicate objective target");
            if(objective.Type==Kind::Reach && (!objective.Region || objective.Count!=1 || !objective.Targets.empty()))
                throw std::runtime_error("Reach requires an area, count one and no targets");
            if(objective.Region) {
                for(double coordinate:objective.Region->Center)
                    if(!std::isfinite(coordinate) || std::abs(coordinate)>100000000)throw std::runtime_error("Invalid objective area center");
                if(!std::isfinite(objective.Region->RadiusMeters) || objective.Region->RadiusMeters<0.01 || objective.Region->RadiusMeters>10000)
                    throw std::runtime_error("Invalid objective area radius");
            }
        }
    }
}
struct Credit {
    Kind Type;
    std::string Target;
    int Amount=1;
    std::optional<std::array<double,3>> Position;
};
// Storage is supplied by the native QuestInts adapter. There is no filesystem
// fallback. The caller must authenticate player attribution and deduplicate
// game callbacks before crediting; inventory snapshots are not gather events.
class Progress {
    Graph graph;
    std::function<int(const std::string&)> read;
    std::function<void(const std::string&,int)> write;
    int run;
    std::string Key(const Stage& stage,const Objective& objective) const {
        return "RuneSchema.Objective:"+stage.Id+":"+objective.Id;
    }
    void Set(const std::string& key,int value) const {
        write(key,value);
        if(read(key)!=value)throw std::runtime_error("Stage save write was not confirmed");
    }
    void CurrentRun() const {
        if(read("RuneSchema.StageRun")!=run)throw std::runtime_error("Quest stage run is not initialized");
    }
public:
    Progress(Graph definition,int runId,std::function<int(const std::string&)> reader,
        std::function<void(const std::string&,int)> writer):graph(std::move(definition)),read(std::move(reader)),write(std::move(writer)),run(runId) {
        Validate(graph);
        if(run<1 || !read || !write)throw std::runtime_error("Invalid quest stage storage");
    }
    // Called only after the acceptance journal authorizes a new run. Commit
    // run last: interrupted initialization is replayable before gameplay credit.
    void BeginRun() const {
        const auto saved=read("RuneSchema.StageRun");
        if(saved==run)return;
        if(saved<0 || saved>run || (saved!=0 && saved!=run-1))throw std::runtime_error("Quest run mismatch");
        for(const auto& stage:graph.Stages)for(const auto& objective:stage.Objectives)Set(Key(stage,objective),0);
        Set("RuneSchema.StageRun",run);
    }
    int Count(size_t stageIndex,size_t objectiveIndex) const {
        CurrentRun();
        const auto& stage=graph.Stages.at(stageIndex);const auto& objective=stage.Objectives.at(objectiveIndex);
        const int value=read(Key(stage,objective));
        if(value<0 || value>objective.Count)throw std::runtime_error("Invalid saved objective counter");
        return value;
    }
    size_t ActiveStage() const {
        CurrentRun();
        // Derive stage from counters, avoiding a second mutable cursor that
        // could disagree with them after a save or interrupted transition.
        for(size_t s=0;s<graph.Stages.size();++s)
            for(size_t o=0;o<graph.Stages[s].Objectives.size();++o)
                if(!graph.Stages[s].Objectives[o].Optional && Count(s,o)<graph.Stages[s].Objectives[o].Count)return s;
        return graph.Stages.size();
    }
    bool Complete() const {return ActiveStage()==graph.Stages.size();}
    std::vector<std::string> ActiveMarkers() const {
        std::vector<std::string> result;
        const auto s=ActiveStage();if(s==graph.Stages.size())return result;
        for(size_t o=0;o<graph.Stages[s].Objectives.size();++o) {
            const auto& objective=graph.Stages[s].Objectives[o];
            if(!objective.Hidden && objective.Region && Count(s,o)<objective.Count)result.push_back(graph.Stages[s].Id+":"+objective.Id);
        }
        return result;
    }
    size_t Apply(const Credit& credit) const {
        if(credit.Amount<1 || credit.Amount>999)throw std::runtime_error("Invalid objective credit amount");
        const auto s=ActiveStage();if(s==graph.Stages.size())return 0;
        size_t changed=0;
        // Snapshot the active stage: one event cannot also advance its successor.
        for(size_t o=0;o<graph.Stages[s].Objectives.size();++o) {
            const auto& objective=graph.Stages[s].Objectives[o];
            if(objective.Type!=credit.Type)continue;
            if(objective.Type!=Kind::Reach && std::find(objective.Targets.begin(),objective.Targets.end(),credit.Target)==objective.Targets.end())continue;
            if(objective.Region && (!credit.Position || !objective.Region->Contains(*credit.Position)))continue;
            const auto value=Count(s,o);
            if(value==objective.Count)continue;
            Set(Key(graph.Stages[s],objective),value+std::min(credit.Amount,objective.Count-value));++changed;
        }
        return changed;
    }
};
}
