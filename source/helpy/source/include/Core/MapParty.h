#pragma once
#include "Core/ItemPresentation.h"
#include "Core/MapProjection.h"
namespace PS::MapParty {
using Json=nlohmann::json;
inline const Json* Find(const Json& snapshot,const std::string& path){
    const auto rows=snapshot.find("Players");if(path.empty()||rows==snapshot.end()||!rows->is_array())return nullptr;
    const Json* found=nullptr;for(const auto& row:*rows)if(row.is_object()&&ItemPresentation::Text(row,"Path")==path){if(found)return nullptr;found=&row;}return found;
}
inline const Json* Local(const Json& snapshot){
    const auto rows=snapshot.find("Players");if(rows==snapshot.end()||!rows->is_array())return nullptr;
    const Json* found=nullptr;for(const auto& row:*rows)if(row.is_object()&&row.value("IsLocal",false)){if(found)return nullptr;found=&row;}return found;
}
inline Json Between(const Json& snapshot,const std::string& movingPath,const std::string& targetPath,uint64_t generation,uint64_t id){
    if(!snapshot.value("CanBuildAndTeleport",false)||snapshot.value("PartyTravelVersion",0)<2)throw std::runtime_error("This action requires the matching host/server party runtime.");
    const auto* moving=Find(snapshot,movingPath);const auto* target=Find(snapshot,targetPath);
    if(!moving||!target||moving==target)throw std::runtime_error("Choose two different connected party members.");
    const auto movingGuid=ItemPresentation::Text(*moving,"Guid"),targetGuid=ItemPresentation::Text(*target,"Guid");
    if(movingGuid.empty()||targetGuid.empty()||movingGuid==targetGuid||ItemPresentation::Text(snapshot,"World").empty()||!generation||!id)throw std::runtime_error("Party identities are unavailable; refresh the roster.");
    return {{"Action","MapTeleport"},{"_Source","HelpyMapParty"},{"_RequestId",id},{"Generation",generation},{"World",ItemPresentation::Text(snapshot,"World")},
        {"Player",movingPath},{"PlayerGuid",movingGuid},{"TargetPlayer",targetPath},{"TargetPlayerGuid",targetGuid}};
}
inline Json ToPoint(const Json& snapshot,const std::string& movingPath,const MapProjection::Projection& projection,MapProjection::Point point,uint64_t generation,uint64_t id){
    if(!snapshot.value("CanBuildAndTeleport",false)||snapshot.value("PartyTravelVersion",0)<2)throw std::runtime_error("This action requires the matching host/server party runtime.");
    const auto* moving=Find(snapshot,movingPath);if(!moving||!projection.Contains(point)||ItemPresentation::Text(*moving,"Guid").empty()||ItemPresentation::Text(snapshot,"World").empty()||!generation||!id)throw std::runtime_error("Choose a connected member and valid map destination.");
    return {{"Action","MapTeleport"},{"_Source","HelpyMapParty"},{"_RequestId",id},{"Generation",generation},{"World",ItemPresentation::Text(snapshot,"World")},
        {"Player",movingPath},{"PlayerGuid",ItemPresentation::Text(*moving,"Guid")},{"PartyPoint",true},{"NearMarker",false},{"TargetLocation",{{"X",point.X},{"Y",point.Y}}}};
}
inline Json Travel(const Json& snapshot,const std::string& other,bool recall,uint64_t generation,uint64_t id){
    if(!snapshot.value("CanBuildAndTeleport",false)||snapshot.value("PartyTravelVersion",0)<1)throw std::runtime_error("Party travel requires the matching host/server runtime.");
    const auto* local=Local(snapshot);const auto* member=Find(snapshot,other);
    if(!local||!member||local==member)throw std::runtime_error("Choose another connected party member; your own character must be available.");
    const auto& moving=recall?*member:*local;const auto& destination=recall?*local:*member;
    const auto movingGuid=ItemPresentation::Text(moving,"Guid"),targetGuid=ItemPresentation::Text(destination,"Guid");
    if(movingGuid.empty()||targetGuid.empty()||movingGuid==targetGuid||ItemPresentation::Text(snapshot,"World").empty()||!generation||!id)throw std::runtime_error("Party identity is unavailable; refresh the roster.");
    return {{"Action","MapTeleport"},{"_Source","HelpyMapParty"},{"_RequestId",id},{"Generation",generation},{"World",ItemPresentation::Text(snapshot,"World")},
        {"Player",ItemPresentation::Text(moving,"Path")},{"PlayerGuid",movingGuid},{"TargetPlayer",ItemPresentation::Text(destination,"Path")},{"TargetPlayerGuid",targetGuid}};
}
}
