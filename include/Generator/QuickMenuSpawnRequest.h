#pragma once
#include "Generator/QuickMenuUI.h"

namespace PS::QuickUI {
// This encoder feeds the existing SpawnTools action. It owns no Unreal objects
// and never spawns during Canvas drawing. Tests can record the exact fields.
template<class Set, class AddDrop>
void EncodeSpawn(const Command& c, Set&& set, AddDrop&& addDrop) {
    if(c.kind!=Command::Kind::Spawn||c.player.empty()||c.definition.empty())
        throw std::runtime_error("Select a current player and spawn definition first.");
    if(c.classPath.empty()||c.classPath.front()!='/'||c.classPath.find('.')==std::string::npos
        ||c.classPath.size()>2048||c.classPath.find_first_of("\r\n\t")!=std::string::npos)
        throw std::runtime_error("The selected class is not a complete game asset path. Reindex and select it again.");
    if(c.count<1||c.count>20||!std::isfinite(c.scale)||c.scale<=0.0)
        throw std::runtime_error("Count must be 1..20 and scale 0.1..10.");
    if(c.npc) {
        if(c.resource||c.count!=1||c.powerLevel!=-1||!c.loot.empty()||c.effect!=Effect::Inherit)
            throw std::runtime_error("NPC placement does not accept AI power, effects, loot or multiple copies per request.");
        HelpyNodes::ValidateDuration(c.permanent,c.durationSeconds);
        set("Action",std::string("Spawn"));set("Player",c.player);set("Definition",c.definition);set("Class",c.classPath);
        set("NPC",true);set("Resource",false);set("Permanent",c.permanent);set("DurationSeconds",c.durationSeconds);
        set("Name",c.name);set("Scale",c.scale);set("Count",1);set("Distance",5.0);set("PlacementMode",c.placementMode);
        if(c.placementMode=="Radius")set("PlacementRadius",c.placementRadius);
        else if(c.placementMode=="Grid"){set("GridX",c.gridX);set("GridY",c.gridY);set("GridSize",c.gridSize);}
        else throw std::runtime_error("NPC placement mode must be Radius or Grid.");
        return;
    }
    Authoring::ValidatePower(c.powerLevel);
    if(c.resource && c.powerLevel!=-1)throw std::runtime_error("Resources do not support enemy power level.");
    set("Permanent",c.permanent);
    if(c.powerLevel!=-1)set("PowerLevel",c.powerLevel);
    set("Action",std::string("Spawn"));set("Player",c.player);set("Definition",c.definition);
    set("Class",c.classPath);set("Resource",c.resource);set("Name",c.name);
    set("Scale",c.scale);set("Count",c.count);set("Distance",5.0);set("Yaw",0.0);set("Height",0.0);set("PlacementMode",c.placementMode);
    if(c.placementMode=="Radius")set("PlacementRadius",c.placementRadius);
    else if(c.placementMode=="Grid"){set("GridX",c.gridX);set("GridY",c.gridY);set("GridSize",c.gridSize);}
    else throw std::runtime_error("Spawn placement mode must be Radius or Grid.");
    if(c.effect!=Effect::Inherit)set("GhostMesh",c.effect==Effect::Ghost);
    set("AppendAdditionalDrops",true);
    if(c.loot.size()>MaxDrops)throw std::runtime_error("At most 16 additional loot entries are supported.");
    for(const auto& d:c.loot){
        if(d.item.empty()||d.item.front()!='/'||d.item.find('.')==std::string::npos||d.min<1||d.max<d.min
            ||d.max>10000||!std::isfinite(d.chance)||d.chance<0||d.chance>100)
            throw std::runtime_error("An additional loot entry has an invalid path, quantity or chance.");
        addDrop(d);
    }
}
} // namespace PS::QuickUI
