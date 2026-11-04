#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

namespace PS {
struct HelpyAuthorizedPlayer {
    std::string characterGuid,characterName,accountName,platform;
    int playerId=-1; // Session alias for display, never an authorization key.
};
namespace HelpyPlayerAuthorization {
inline std::string Guid(std::string_view value){
    std::string result;for(unsigned char c:value){
        if(c=='-'||c=='{'||c=='}')continue;
        if(c>='a'&&c<='f')c-=32;
        if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return {};
        result+=char(c);
    }
    return result.size()==32&&result!=std::string(32,'0')?result:std::string{};
}
inline bool Text(std::string_view value,size_t maximum){return value.size()<=maximum&&std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;});}
inline bool Valid(const HelpyAuthorizedPlayer& player){return !Guid(player.characterGuid).empty()&&Text(player.characterName,1024)&&Text(player.accountName,128)&&Text(player.platform,64);}
inline bool Contains(const std::vector<HelpyAuthorizedPlayer>& players,std::string_view guid){
    const auto key=Guid(guid);return !key.empty()&&std::any_of(players.begin(),players.end(),[&](const auto& row){return Guid(row.characterGuid)==key;});
}
inline bool Remember(std::vector<HelpyAuthorizedPlayer>& players,HelpyAuthorizedPlayer player){
    if(!Valid(player))throw std::runtime_error("The server could not verify this player's persistent character identity.");
    player.characterGuid=Guid(player.characterGuid);
    auto found=std::find_if(players.begin(),players.end(),[&](const auto& row){return Guid(row.characterGuid)==player.characterGuid;});
    if(found==players.end()){players.push_back(std::move(player));return true;}
    if(player.accountName.empty())player.accountName=found->accountName;
    if(player.platform.empty()||player.platform=="Unknown")player.platform=found->platform;
    if(found->characterName==player.characterName&&found->accountName==player.accountName&&found->platform==player.platform&&found->playerId==player.playerId)return false;
    *found=std::move(player);return true;
}
}
}
