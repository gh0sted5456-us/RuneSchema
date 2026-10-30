#pragma once
#if defined(HELPY_PREVIEW)
#include <imgui.h>
#include <array>
#include <map>
#include <string>
#include <optional>
#include <vector>
namespace PS::HelpyPreviewHooks {
inline std::optional<double> PageTransitionElapsed;
inline float PageTransitionAlpha=1.f;
inline unsigned ModifiedItemBadges=0;
inline ImVec4 ItemSearchButton{};
inline unsigned ItemEmptyNotices=0;
inline unsigned RuntimeBadgeDraws=0;
inline unsigned CartVerifiedBadges=0,CartRejectedBadges=0,CartUncertainBadges=0;
inline ImVec4 DeliveryBadge{};inline ImVec2 DeliveryBadgeBottomLeft{};
inline ImVec4 CacheDetach{},CacheAdvanced{};
inline ImVec4 CacheImport{},CacheBuild{},CacheExport{};
inline ImVec4 AuthorToolsDownload{};
inline size_t BayDetailsFacts=0;
inline std::map<std::string,ImVec4> ItemRects,CartRects;
inline std::map<std::string,ImVec4> DeliveryRects;
inline ImVec4 DeliveryHeading{},ReorderAction{};
inline ImVec4 CartImport{},CartExport{},CartSelectAll{},CartMoveUp{},CartMoveDown{},CartRemoveSelected{},ReorderSelected{};
inline bool ControlDown=false;
inline ImVec4 PresetDialog{};
inline std::map<std::string,ImVec4> CartChecks,CartQuantities,DeliveryChecks;
inline std::map<std::string,ImVec4> RecipientRemove;
inline ImVec4 RecipientsClear{};
inline std::map<std::string,ImVec4> AuthoringButtons;
inline std::map<std::string,ImVec4> WorkspaceTabs;
inline std::map<std::string,ImVec4> Navigation;
inline std::map<std::string,ImVec4> BugFields;
inline ImVec4 BugSubmit{};
inline ImVec4 Browser{},Cart{},Inputs{},Outputs{},Give{},AddCart{};
inline ImVec4 InventoryViewport{},SavedItemRect{};
inline ImVec4 LiveInventoryViewport{};
inline std::map<std::string,ImVec4> SpawnRows;
inline ImVec4 SpawnHide{};
inline std::string SpawnDetails;
inline ImGuiID InventorySearchId=0,InventoryRefreshId=0,BaySearchId=0,BayRefreshId=0;
inline ImVec4 Map{},Tooltip{},MapSelected{},MapLodestone{},MapChest{};
inline std::map<std::string,ImVec4> MapPartyRows;
inline ImVec4 MapPartyWindow{};
inline ImVec4 PartyDetach{};
inline std::map<std::string,ImVec4> PartyGear;
inline std::map<std::string,ImVec4> PartyPortraits,PartyPower,PartyPlatforms;
inline std::map<std::string,ImU32> MapPlayerTints;
inline std::map<std::string,ImVec4> PartyMeters;
inline unsigned PartyDurabilityMeters=0;
inline bool PartyDurabilityDetails=false;
inline std::string PartyDetailSections;
inline ImVec4 MapOperationRow{},MapSearchField{},MapOptionsBody{};
inline ImVec4 MapPartyContext{},MapPopupWindow{};
inline ImVec4 MapPartyTeleport{},MapPartyRecall{},MapPartyConfirm{},MapPartyCancel{};
inline ImVec4 MapFiltersButton{},MapMarkerSearch{};
inline ImVec4 MapFiltersClose{};
inline bool MapForceGroupsOpen=false;
inline std::map<std::string,ImVec4> MapCategoryControls;
inline size_t MapReferenceMatches=0,MapReferenceClusters=0;
inline ImVec4 MapSpawn{};
inline ImVec4 RuntimeBadge{},InformationBar{},PageHeading{},NavigationRail{};
inline ImVec4 RuntimeGameLogo{},SplashEnter{};
inline ImVec4 BackgroundPicture{};
inline size_t BackgroundDraws=0;
inline bool SplashVisible=false;
inline bool SplashAnimated=false;
inline float SplashBlend=0;
inline std::optional<double> SplashElapsed;
inline bool ForceArtworkHint=false;
inline ImVec4 ArtworkDownload{};
inline ImVec4 ArtworkHide{};
inline bool ArtworkHintVisible=false;
inline std::vector<ImVec4> SplashTipLines;
inline float SplashCenterX=0;
inline size_t SplashTipIndex=0;
inline bool SplashExiting=false;
inline float SplashExitProgress=0;
inline bool RuntimeDetailsVisible=false;
inline bool RuntimeHostOverlay=false;
inline ImVec4 RuntimeDetach{},RuntimeClose{},RuntimeMinimize{};
inline std::map<std::string,ImVec4> WindowControls;
inline ImVec4 BayExpand{};
inline ImVec4 BuildingBrowse{};
inline ImVec4 BuildingUse{};
inline ImVec4 BuildingClose{};
inline std::map<std::string,ImVec4> BuildingCards;
inline ImVec4 AuthoringSource{},AuthoringAppearance{},AuthoringCreate{},TransmogTab{},CreationTab{};
inline ImVec4 BayDock{};
inline ImVec4 CardScale{};
inline ImVec4 IconCache{};
inline ImVec4 IconCacheClose{};
inline ImVec4 SourceFilter{},ViewFamily{},ViewModFamily{},RefreshSource{};
inline std::map<std::string,ImVec4> ModTitles;
inline ImVec4 ModDetailsMenu{},ModDetailsWindow{};
inline std::map<std::string,ImVec4> SourceOptions;
inline size_t ModBadges=0;
inline size_t LoadingRings=0;
inline size_t LoadingDetailsShown=0;
inline std::string LoadingDetailsText;
inline ImVec4 LoadingRect{};
inline ImVec4 LoadingDetailsRect{};
inline std::map<std::string,ImVec4> ResizeHandles;
inline bool MouseEnabled=false,MouseDown=false,RightDown=false;
inline std::string TooltipPath;
inline size_t TooltipFacts=0;
inline float TooltipScroll=0,TooltipScrollMax=0;
inline ImVec2 Mouse{-100,-100};
inline ImVec4 LastRect(){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();return {a.x,a.y,b.x,b.y};}
inline ImVec4 WindowRect(){const auto a=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();return {a.x,a.y,a.x+s.x,a.y+s.y};}
}
#endif
