#pragma once
#include "Utility/Config.h"
#include "SDK/Helper/PlayerStatus.h"

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
inline bool Allowed(std::string_view guid){return HelpyPlayerAuthorization::Contains(PSConfig::Get()->GetSettings().helpyAuthority.authorizedPlayers,guid);}
inline void Observe(RC::Unreal::UObject* controller){
    if(!controller||!Network::OwnsGameplay(Network::Detect(controller).Mode))return;
    auto* config=PSConfig::Get();auto& policy=config->GetMutableSettings().helpyAuthority;
    const auto guid=PlayerStatusRuntime::CharacterGuid(DragonWilds::ActorHelper::GetObjectRef(controller,TEXT("PlayerState")));
    if(guid.empty()||(!policy.autoAuthorizeConnectingPlayers&&!Allowed(guid)))return;
    const auto before=policy.authorizedPlayers;
    if(HelpyPlayerAuthorization::Remember(policy.authorizedPlayers,Identity(controller))&&!config->Save()){
        policy.authorizedPlayers=before;throw std::runtime_error("Player authorization could not be saved. Existing permissions retained.");
    }
}
}
