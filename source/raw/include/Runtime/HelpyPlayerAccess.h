#pragma once
#include "Utility/Config.h"
#include "SDK/Helper/PlayerStatus.h"
#include "Runtime/NetworkContext.h"
#include "Unreal/UnrealInitializer.hpp"
#include "Unreal/UObjectGlobals.hpp"

namespace PS::HelpyPlayerAccess {
inline HelpyAuthorizedPlayer Identity(RC::Unreal::UObject* controller){
    namespace AH=DragonWilds::ActorHelper;
    auto* state=controller?AH::GetObjectRef(controller,TEXT("PlayerState")):nullptr;
    HelpyAuthorizedPlayer player;
    player.characterGuid=PlayerStatusRuntime::CharacterGuid(state);
    player.accountName=PlayerStatusRuntime::ControllerAccountName(controller);
    player.platform=PlayerStatusRuntime::ControllerPlatform(controller);
    player.playerId=PlayerStatusRuntime::PlayerId(state);
    if(state){AH::FunctionCall name(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));name.Invoke();const auto native=name.Result<RC::Unreal::FString>();const auto& chars=native.GetCharArray();
        if(chars.Num()>1&&chars.Num()<=257&&chars.GetData())player.characterName=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));}
    return player;
}
inline bool Allowed(std::string_view guid){return HelpyPlayerAuthorization::Contains(PSConfig::Get()->LiveHelpyAuthority().authorizedPlayers,guid);}
inline void Authorize(RC::Unreal::UObject* controller){
    namespace AH=DragonWilds::ActorHelper;using namespace RC::Unreal;
    auto* type=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
    if(!IsInGameThread()||!controller||!UObjectGlobals::IsValidObjectForFindXOf(controller)||!type||!controller->IsA(type)
        ||!Network::OwnsGameplay(Network::Detect(controller).Mode))throw std::runtime_error("Helpy authorization requires the live authoritative player controller.");
    auto* config=PSConfig::Get();config->RefreshHelpyAuthority();const auto before=config->LiveHelpyAuthority();auto proposed=before;
    if(HelpyPlayerAuthorization::Remember(proposed.authorizedPlayers,Identity(controller))&&!config->SaveHelpyPlayerAccess(before,proposed))
        throw std::runtime_error("Helpy authorization could not be saved. Refresh before retrying.");
}
inline void Observe(RC::Unreal::UObject* controller){
    if(!controller||!Network::OwnsGameplay(Network::Detect(controller).Mode))return;
    auto* config=PSConfig::Get();const auto& before=config->LiveHelpyAuthority();
    const auto guid=PlayerStatusRuntime::CharacterGuid(DragonWilds::ActorHelper::GetObjectRef(controller,TEXT("PlayerState")));
    if(guid.empty()||(!before.autoAuthorizeConnectingPlayers&&!HelpyPlayerAuthorization::Contains(before.authorizedPlayers,guid)))return;
    auto proposed=before;
    if(HelpyPlayerAuthorization::Remember(proposed.authorizedPlayers,Identity(controller))&&!config->SaveHelpyPlayerAccess(before,proposed))
        throw std::runtime_error("Player authorization could not be saved. Refresh before retrying.");
}
}
