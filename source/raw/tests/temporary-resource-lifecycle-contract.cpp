#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    try {
    if(argc!=6)throw std::runtime_error("Expected spawn, NPC, event, wiring and schema sources");
    const auto spawn=Read(argv[1]),npc=Read(argv[2]),event=Read(argv[3]),wiring=Read(argv[4]),schema=Read(argv[5]);
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(std::string("Temporary resource regression: ")+message);
    };
    require(spawn.find("TemporaryResourceCondition(spawn,world)")!=std::string::npos
        && spawn.find("TemporaryResourceCondition(spawn,m_readyWorld)")!=std::string::npos,
        "spawn and cleanup do not use the same condition");
    require(spawn.find("std::optional<bool> DragonWildsSpawnLoader::TemporaryResourceCondition")!=std::string::npos
        && spawn.find("if(!condition)continue; // Unknown state must not retire a live resource.")!=std::string::npos,
        "unavailable quest/event state could retire a live resource");
    require(spawn.find("spawn.bTemporaryResource && spawn.bTemporaryResourceSpawned")!=std::string::npos,
        "harvested temporary nodes can respawn repeatedly within one event");
    require(spawn.find("spawn.bTemporaryResourceSpawned=false")!=std::string::npos,
        "temporary nodes cannot reset for a later event");
    require(spawn.find("spawn.Time!=TimeOfDay::Requirement::Any || questOwnedBuilding || spawn.bTemporaryResource")!=std::string::npos
        && spawn.find("Temporary resource save exclusion did not round-trip")!=std::string::npos,
        "temporary nodes can enter native world persistence");
    require(spawn.find("Temporary resource identity collides with an unverified world actor; left unchanged")!=std::string::npos,
        "cleanup can modify a persistent world actor");
    require(spawn.find("actorClass->IsChildOf(oreBase)")!=std::string::npos
        && spawn.find("actorClass->IsChildOf(treeBase)")!=std::string::npos
        && spawn.find("/Game/Gameplay/World/Mining/Ores/PARENT/BP_OreNode.BP_OreNode_C")!=std::string::npos
        && spawn.find("Temporary resource Class must be a cooked Mining or Trees actor")==std::string::npos,
        "mod-owned resource subclasses cannot be recognized outside vanilla folders");
    const auto reconcileStart=spawn.find("void DragonWildsSpawnLoader::ReconcileTimedBuildingProps(");
    const auto reconcileEnd=spawn.find("void DragonWildsSpawnLoader::ReconcileManagedActorScales(",reconcileStart);
    require(reconcileStart!=std::string::npos && reconcileEnd!=std::string::npos,
        "conditional spawn lifecycle is missing");
    const auto reconcile=spawn.substr(reconcileStart,reconcileEnd-reconcileStart);
    const auto inactive=reconcile.find("if(!allowed) {");
    const auto grounding=reconcile.find("ResolveGroundedLocation(m_readyWorld,spawn);");
    const auto creation=reconcile.find("ProcessActorEntry(m_readyWorld,spawn);");
    require(reconcile.find("!spawn.bCellActivated")!=std::string::npos
        && reconcile.find("TemporaryResourceCondition(spawn,m_readyWorld)")!=std::string::npos
        && inactive!=std::string::npos && grounding!=std::string::npos
        && creation!=std::string::npos && inactive<grounding && grounding<creation,
        "inactive-at-cell-load resource can be created before quest-time grounding");
    require(reconcile.find("Temporary resource '{}' from '{}' could not activate during {}:")!=std::string::npos
        && reconcile.find("if(!priorFailure || (!wasGrounded && spawn.bGroundingResolved))")!=std::string::npos,
        "quest-time grounding failures lack bounded owner/stage diagnostics");
    require(reconcile.find("matching identity did not prove an owned transient actor (tracked={}, class={}, transient={}, save-excluded={})")!=std::string::npos
        && reconcile.find("if(!spawn.bSpawnFailed)")!=std::string::npos,
        "inactive quest cleanup can silently remove an unverified actor or spam warnings");
    const auto groundStart=spawn.find("void DragonWildsSpawnLoader::ResolveGroundedLocation(");
    const auto groundEnd=spawn.find("void DragonWildsSpawnLoader::CreateSpawn(",groundStart);
    require(groundStart!=std::string::npos && groundEnd!=std::string::npos,
        "grounding implementation is missing");
    const auto ground=spawn.substr(groundStart,groundEnd-groundStart);
    const auto trace=ground.find("if (!UECustom::UKismetSystemLibrary::LineTraceGround(");
    const auto failure=ground.find("throw std::runtime_error(",trace);
    const auto placed=ground.find("spawn.Location = FVector(",failure);
    require(trace!=std::string::npos && failure!=std::string::npos
        && placed!=std::string::npos && trace<failure && failure<placed
        && ground.find("impact.Z() + spawn.GroundZOffset + 10.0")!=std::string::npos,
        "failed ground trace can place a temporary resource or lose its 10 cm clearance");
    require(npc.find("bool DragonWildsNpcLoader::IsQuestActive")!=std::string::npos
        && npc.find("if(state==\"Given\")return true")!=std::string::npos,
        "quest activation is not tied to the native Given state");
    require(event.find("bool Running(const std::string& key,UWorld* world)const")!=std::string::npos
        && wiring.find("m_spawnLoader->IsEventActive=")!=std::string::npos,
        "event condition is not tied to the active event in the same world");
    require(schema.find("[\"QuestActive\"]")!=std::string::npos
        && schema.find("[\"EventActive\"]")!=std::string::npos,
        "authoring schema does not describe temporary resource conditions");
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
