#pragma once
// Presentation policy only. These are cooked texture references, not filesystem paths.
#include "Generator/HelpyNodes.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <string>
#include <string_view>

namespace PS::QuickDecorations {
inline constexpr const char* BadgeRoot="/RuneSchema/UI/Icons/Badges/";
// Shared custom-banner namespace. Storefronts without an explicit choice use
// the vanilla Death banner rather than depending on an optional content pak.
inline constexpr const char* BannerRoot="/Game/Mods/RuneSchema/UI/Icons/Banners/";
inline constexpr const char* DragonwildsBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Dragonwilds.T_Badge_Dragonwilds";
inline constexpr const char* RuneSchemaBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Runeschema.T_Badge_Runeschema";
inline constexpr const char* Ue4ssBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Ue4ss.T_Badge_Ue4ss";
inline constexpr const char* FavoriteBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Favorite.T_Badge_Favorite";
inline constexpr const char* ModdedBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Modded.T_Badge_Modded";
inline constexpr const char* VendorBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Vendor.T_Badge_Vendor";
// The cooked 0.7.0 UI container ships Dialogue as the shared conversation/
// quest navigation glyph; do not reference a non-existent Quest_Giver texture.
inline constexpr const char* QuestGiverBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Dialogue.T_Badge_Dialogue";
inline constexpr const char* NpcBadge="/RuneSchema/UI/Icons/Badges/T_Badge_NPC.T_Badge_NPC";
inline constexpr const char* LoreBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Dialogue.T_Badge_Dialogue";
inline constexpr const char* DialogueBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Dialogue.T_Badge_Dialogue";
inline constexpr const char* CloseBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Close.T_Badge_Close";
inline constexpr const char* RefreshBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Refresh.T_Badge_Refresh";
inline constexpr const char* FullScanBadge="/RuneSchema/UI/Icons/Badges/T_Badge_FullScan.T_Badge_FullScan";
inline constexpr const char* StopBadge=FullScanBadge;
inline constexpr const char* SettingsBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Settings.T_Badge_Settings";
inline constexpr const char* CoverageBadge="/RuneSchema/UI/Icons/Badges/T_Badge_Coverage.T_Badge_Coverage";
// Retained as a schema/compatibility path for cooked packs; Helpy renders power
// as gold "PL" plus a white numeral and no longer draws this texture.
inline constexpr const char* PowerLevelBadge="/Game/Art/UI/PowerLevel/T_PowerLevel_NeutralZone.T_PowerLevel_NeutralZone";
inline constexpr const char* LowercaseBadgeRoot="/RuneSchema/UI/Icons/badges/";
inline constexpr const char* LowercaseDragonwildsBadge="/RuneSchema/UI/Icons/badges/T_Badge_Dragonwilds.T_Badge_Dragonwilds";
inline constexpr const char* LowercaseRuneSchemaBadge="/RuneSchema/UI/Icons/badges/T_Badge_Runeschema.T_Badge_Runeschema";
inline constexpr const char* LowercaseFavoriteBadge="/RuneSchema/UI/Icons/badges/T_Badge_Favorite.T_Badge_Favorite";
inline constexpr const char* LowercaseModdedBadge="/RuneSchema/UI/Icons/badges/T_Badge_Modded.T_Badge_Modded";
inline constexpr const char* PreviousBadgeRoot="/Game/RuneSchema/Badges/Runtime/";
inline constexpr const char* PreviousDragonwildsBadge="/Game/RuneSchema/Badges/Runtime/T_Badge_Dragonwilds.T_Badge_Dragonwilds";
inline constexpr const char* PreviousRuneSchemaBadge="/Game/RuneSchema/Badges/Runtime/T_Badge_Runeschema.T_Badge_Runeschema";
inline constexpr const char* PreviousFavoriteBadge="/Game/RuneSchema/Badges/Runtime/T_Badge_Favorite.T_Badge_Favorite";
inline constexpr const char* PreviousModdedBadge="/Game/RuneSchema/Badges/Runtime/T_Badge_Modded.T_Badge_Modded";
inline constexpr const char* LegacyBadgeRoot="/Game/Mods/RuneSchema/Badges/Runtime/";
inline constexpr const char* LegacyDragonwildsBadge="/Game/Mods/RuneSchema/Badges/Runtime/T_Badge_Dragonwilds.T_Badge_Dragonwilds";
inline constexpr const char* LegacyRuneSchemaBadge="/Game/Mods/RuneSchema/Badges/Runtime/T_Badge_Runeschema.T_Badge_Runeschema";
inline constexpr const char* LegacyRuneSchemaCaseBadge="/Game/Mods/RuneSchema/Badges/Runtime/T_Badge_RuneSchema.T_Badge_RuneSchema";
inline constexpr const char* LegacyFavoriteBadge="/Game/Mods/RuneSchema/Badges/Runtime/T_Badge_Favorite.T_Badge_Favorite";
inline constexpr const char* LegacyModdedBadge="/Game/Mods/RuneSchema/Badges/Runtime/T_Badge_Modded.T_Badge_Modded";
inline std::array<const char*,4> FallbackTextures(std::string_view canonical) {
    if(canonical==DragonwildsBadge)return {LowercaseDragonwildsBadge,PreviousDragonwildsBadge,LegacyDragonwildsBadge,""};
    if(canonical==RuneSchemaBadge)return {LowercaseRuneSchemaBadge,PreviousRuneSchemaBadge,LegacyRuneSchemaBadge,LegacyRuneSchemaCaseBadge};
    if(canonical==FavoriteBadge)return {LowercaseFavoriteBadge,PreviousFavoriteBadge,LegacyFavoriteBadge,""};
    if(canonical==ModdedBadge)return {LowercaseModdedBadge,PreviousModdedBadge,LegacyModdedBadge,""};
    return {"","","",""};
}
inline constexpr float BadgeSize=24.f, BadgeGap=4.f, FavoriteHitSize=36.f;
inline constexpr std::size_t MaxFavorites=4096;
using Favorites=std::map<std::string,std::string>; // item object path OR typed node identity -> last display name

enum class Origin { Unknown, Dragonwilds, RuneSchema, Modded };
inline std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
    return value;
}
inline bool MissingDisplayName(std::string_view name) {
    const auto text=Fold(std::string(name));
    return text.empty()||text.find("missing string")!=std::string::npos
        ||text.find("string table entry")!=std::string::npos||text=="none";
}
inline std::string ReadableAssetName(std::string path) {
    const auto slash=path.find_last_of("/.");if(slash!=path.npos)path.erase(0,slash+1);
    for(const auto* prefix:{"ITEM_","BP_"})if(path.starts_with(prefix)){path.erase(0,std::char_traits<char>::length(prefix));break;}
    if(path.ends_with("_C"))path.resize(path.size()-2);
    std::replace(path.begin(),path.end(),'_',' ');return path;
}
inline bool ModPath(std::string_view path) {
    // Check a whole path segment, never names such as /Game/ModsBackup or item icons.
    return Fold(std::string(path)).starts_with("/game/mods/");
}
inline Origin ItemOrigin(std::string_view path,bool cooked,bool confirmedRuntimeClone) {
    if(confirmedRuntimeClone)return Origin::RuneSchema;
    if(ModPath(path))return Origin::Modded;
    if(cooked)return Origin::Dragonwilds;
    return Origin::Unknown;
}
inline const char* Texture(Origin origin) {
    switch(origin) {
    case Origin::Dragonwilds:return DragonwildsBadge;
    case Origin::RuneSchema:return RuneSchemaBadge;
    case Origin::Modded:return ModdedBadge;
    default:return "";
    }
}
inline const char* Label(Origin origin) {
    switch(origin) {
    case Origin::Dragonwilds:return "DW";
    case Origin::RuneSchema:return "RS";
    case Origin::Modded:return "MOD";
    default:return "?";
    }
}
inline bool ValidFavoritePath(std::string_view path) {
    if(path.size()<4||path.size()>2048||path.front()!='/')return false;
    const auto dot=path.find_last_of('.'),slash=path.find_last_of('/');
    if(dot==path.npos||dot<=slash||dot+1==path.size())return false;
    return std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127||c=='\\';});
}
inline bool ValidFavoriteKey(std::string_view key) {
    return ValidFavoritePath(key)||HelpyNodes::ValidFavoriteKey(key);
}
inline bool Token(std::string_view text,std::string_view token) {
    if(token.empty())return false;
    const auto lower=Fold(std::string(text));std::size_t at=0;
    const auto word=[](unsigned char c){return std::isalnum(c)!=0;};
    while((at=lower.find(token,at))!=std::string::npos) {
        const auto end=at+token.size();
        if((at==0||!word(static_cast<unsigned char>(lower[at-1])))
            &&(end==lower.size()||!word(static_cast<unsigned char>(lower[end]))))return true;
        ++at;
    }
    return false;
}
inline int LastPage(std::size_t count,int pageSize) {
    if(pageSize<=0)return 0;
    return count==0?0:static_cast<int>((count-1)/static_cast<std::size_t>(pageSize));
}
inline int ClampPage(int page,std::size_t count,int pageSize) {
    return std::clamp(page,0,LastPage(count,pageSize));
}
} // namespace PS::QuickDecorations
