#pragma once
#include "Core/SpawnCatalogSearch.h"
namespace PS::SpawnPresentation {
inline bool Enemy(const nlohmann::json& row){
    if(SpawnCatalogSearch::Text(row,"Type")!="AI")return false;
    const auto path=SpawnCatalogSearch::Fold(SpawnCatalogSearch::Text(row,"Class"));
    if(path.starts_with("/script/"))return false;
    if(row.value("SpawnableCharacter",false)&&row.value("Available",false)&&!row.value("NeedsInspection",false))return true;
    if(!row.value("Mounted",false)||!row.value("NeedsInspection",false))return false;
    for(const auto* noise:{"/projectiles/","/effects/","/animations/","/abilities/","bp_projectile","bp_attackeffect","bp_aicontroller","bp_aispawner"})
        if(path.find(noise)!=path.npos)return false;
    return true;
}
inline bool ResourceNoise(const nlohmann::json& row){
    const auto text=SpawnCatalogSearch::Fold(SpawnCatalogSearch::Text(row,"Class")+" "+SpawnCatalogSearch::Text(row,"Name"));
    for(const auto* token:{"basebuilding","base building","decoration","recipedispenser","recipe dispenser","projectile","attackeffect","vfx_","buildingpiece"})if(text.find(token)!=text.npos)return true;return false;
}
// Curated presentation only: it does not grant spawn authority. Raw entries
// remain accessible in advanced authoring, not mixed with harvestable nodes.
inline int ResourceTab(const nlohmann::json& row){
    if(SpawnCatalogSearch::Text(row,"Type")!="Resource"||ResourceNoise(row))return -1;
    const auto family=SpawnCatalogSearch::Fold(SpawnCatalogSearch::Text(row,"ResourceFamily")),text=SpawnCatalogSearch::Fold(SpawnCatalogSearch::Text(row,"Class")+" "+SpawnCatalogSearch::Text(row,"Name"));
    if(family=="tree"||text.find("/trees/")!=text.npos||text.find("fellabletree")!=text.npos||text.find("sapling")!=text.npos||text.find("splittablelog")!=text.npos)return 0;
    if(family=="mineral"||text.find("/mining/")!=text.npos||text.find("ore node")!=text.npos||text.find("orenode")!=text.npos||text.find("stonenode")!=text.npos||text.find("rocknode")!=text.npos)return 1;
    for(const auto* token:{"forage","gatherable","harvestable","fishingpool","fishinghole","berrybush","berry bush","crop","farmplot"})if(text.find(token)!=text.npos)return 2;
    return row.value("HarvestableVerified",false)?2:-1;
}
}
