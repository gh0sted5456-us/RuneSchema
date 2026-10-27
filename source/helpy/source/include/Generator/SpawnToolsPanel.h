#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/RuneSchemaProfile.h"
#include "Generator/ToolRequest.h"
#include "Runtime/HelpySettings.h"
#include "Generator/ItemCloneRequest.h"
#include "Generator/HelpyImGuiTheme.h"
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <chrono>
#include "Generator/ItemGridPicker.h"
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyNodes.h"
#include "Generator/RecipeStudioPanel.h"
#include "Core/HelpyKitFiles.h"
#include "Core/CartDeliveryBatch.h"
#include "Generator/PlayerBadges.h"
#include "Generator/PartyMemberView.h"
#include "Generator/CatalogProgressView.h"
#include "Generator/ItemAuthoringPanel.h"
#include "Generator/SpawnStudioPanel.h"
#include <algorithm>
#include <cctype>
#include <random>
#include <vector>
#include <set>
#include <future>
#include <optional>
#include <imgui.h>
#include <imgui_internal.h>
namespace PS::SpawnToolsPanel {
inline bool ExtraLootBay=false;
enum class View : unsigned char { All, Items, Ai, Resources, ItemLab, Buildings, Cleanup, Recipes };
// The external shell owns the NPC/AI section selector. Keep one set of the
// validated spawn controls and requests underneath it.
inline int ForcedActorMode=-1;
inline int CatalogCategory(View view) {
    if(view==View::All)return -1;
    if(view==View::Items||view==View::ItemLab||view==View::Recipes)return 0;
    if(view==View::Ai)return 1;
    if(view==View::Resources)return 2;
    return -1;
}
inline bool SearchOnEnter(const char* label,const char* hint,char* buffer,size_t capacity,std::string& committed) {
    const auto& theme=HelpyImGuiTheme::Current;const float button=ImGui::GetFrameHeight(),spacing=ImGui::GetStyle().ItemSpacing.x;
    ImGui::PushID(label);ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(std::max(theme.X(140.0f),ImGui::GetContentRegionAvail().x-button-spacing));
    const bool enter=ImGui::InputTextWithHint("##query",hint,buffer,capacity,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();const bool click=HelpyImGuiTheme::SearchIconButton("search");ImGui::PopID();
    if(enter||click){committed=buffer;return true;}return false;
}
inline void SameLineIfRoom(float logicalWidth=150.0f) {if(ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(logicalWidth))ImGui::SameLine();}
struct MatchCache {uint64_t Revision=0;int Kind=-1;std::string Query;std::vector<size_t> Rows;};
struct ActorUiState {std::string Definition,Query,MetaKey,SelectedLabel="Select spawn";char ClassPath[1024]{},Name[257]{},Filter[257]{};MatchCache Matches;uint64_t MetaRevision=0;int Mode=0;bool Custom=false,Boss=false,Confirmed=false,GridView=false,ViewInitialized=false;float ResultPaneHeightLogical=-1.0f;};
inline std::string Lower(std::string value){std::ranges::transform(value,value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
inline void CacheDefinitions(const nlohmann::json& result,uint64_t revision,int kind,int mode,const std::string& query,MatchCache& cache) {
    const int cacheKind=kind*8+mode;if(cache.Revision==revision&&cache.Kind==cacheKind&&cache.Query==query)return;cache={revision,cacheKind,query,{}};
    if(!result.contains("Definitions")||!result["Definitions"].is_array())return;const auto filter=Lower(query);const auto& rows=result["Definitions"];
    cache.Rows.reserve(rows.size());for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto type=row.value("Type",std::string("AI"));
        if(kind==0&&((mode==0&&type!="NPC")||(mode==1&&type!="AI")))continue;
        if(kind==1){if(type!="Resource")continue;const auto rk=HelpyNodes::ResourceKind(row.value("Class",std::string{}),row.value("ResourceFamily",std::string{}));if((mode==0&&rk!=HelpyNodes::Kind::Tree)||(mode==1&&rk==HelpyNodes::Kind::Tree))continue;}
        const auto key=row.value("Key",std::string{}),title=row.value("Name",key),path=row.value("Class",std::string{}),modId=row.value("ModId",std::string{}),family=row.value("ResourceFamily",std::string{});
        if(filter.empty()||Lower(key+" "+type+" "+title+" "+path+" "+modId+" "+family).find(filter)!=std::string::npos)cache.Rows.push_back(i);}
}
inline void CacheBuildings(const nlohmann::json& result,uint64_t revision,const std::string& query,MatchCache& cache) {
    if(cache.Revision==revision&&cache.Query==query)return;cache={revision,-1,query,{}};
    if(!result.contains("Buildings")||!result["Buildings"].is_array())return;const auto filter=Lower(query);const auto& rows=result["Buildings"];
    cache.Rows.reserve(rows.size());for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto path=row.value("Path",std::string{}),title=row.value("Name",path);
        if(filter.empty()||Lower(path+" "+title).find(filter)!=std::string::npos)cache.Rows.push_back(i);}
}
inline nlohmann::json UiSnapshot={{"Status","Load a world before using runtime tools."}};
inline uint64_t UiRevision=0;inline int UiCategory=-99;
inline std::string SelectedPlayer;
inline uint64_t DefinitionSequence=0x5000000000000000ull,DefinitionPending=0,DefinitionGeneration=0;
inline std::map<std::string,nlohmann::json> InspectedDefinitions;
inline std::string DefinitionStatus;
inline uint64_t CartSequence=(uint64_t{1}<<62),CartGeneration=0;
inline uint64_t KitSequence=(uint64_t{1}<<61),KitPending=0;
inline uint64_t CartCheckPending=0;
inline std::chrono::steady_clock::time_point CartCheckNext{},CartCheckRequested{};
inline void InvalidateCartChecks(){CartCheckPending=0;CartCheckNext={};ItemGridPicker::SharedCart.ClearValidation();}
inline CartDeliveryBatch::Plan CartBatch;
inline std::vector<CartDeliveryBatch::Recipient> CartRecipients;
inline bool CartRecipientsChosen=false,CartNeedsReview=false,CartReviewAcknowledged=false;
inline std::chrono::steady_clock::time_point CartRequestedAt{};
inline const nlohmann::json* CurrentCartPlayer(const nlohmann::json& snapshot,const std::string& path){
    const auto players=snapshot.find("Players");if(path.empty()||players==snapshot.end()||!players->is_array())return nullptr;
    for(const auto& row:*players)if(row.is_object()&&ItemPresentation::Text(row,"Path")==path)return &row;
    return nullptr;
}
inline void SyncCartWorld(){
    const auto generation=SpawnToolRequests::CurrentGeneration();if(CartGeneration==generation)return;
    if(ItemGridPicker::SharedCart.Delivery()){
        ItemGridPicker::CartStatus="World changed during delivery. Cart retained; check recipient inventories before retrying.";
        CartNeedsReview=true;CartReviewAcknowledged=false;
    }
    ItemGridPicker::SharedCart.CancelDelivery();CartBatch=CartDeliveryBatch::Plan{};KitPending=0;CartGeneration=generation;InvalidateCartChecks();
    CartRecipients.clear();CartRecipientsChosen=false;SelectedPlayer.clear();
    UiSnapshot.erase("Players");UiSnapshot.erase("PlayerRoles");UiRevision=0;
}
inline void ReconcileCartRecipients(const nlohmann::json& snapshot){
    // Paths, not display names, identify recipients. Never retarget a removed
    // player to a new player who happens to have the same name.
    std::erase_if(CartRecipients,[&](const auto& recipient){return !CurrentCartPlayer(snapshot,recipient.Path);});
    for(auto& recipient:CartRecipients){
        const auto* row=CurrentCartPlayer(snapshot,recipient.Path);recipient.Name=RuneSchemaProfile::Name(*row);
        if(recipient.Name.empty())recipient.Name=recipient.Path;
    }
    if(!CurrentCartPlayer(snapshot,SelectedPlayer))SelectedPlayer.clear();
}
inline std::vector<CartDeliveryBatch::Recipient> EffectiveCartRecipients(){
    if(CartGeneration!=SpawnToolRequests::CurrentGeneration())return {};
    ReconcileCartRecipients(UiSnapshot);if(CartRecipientsChosen)return CartRecipients;
    const auto* player=CurrentCartPlayer(UiSnapshot,SelectedPlayer);if(!player)return {};
    auto name=ItemPresentation::Text(*player,"Name");return {{SelectedPlayer,name.empty()?SelectedPlayer:name}};
}
inline bool RemoveCartRecipient(const std::string& path){
    if(ItemGridPicker::SharedCart.Delivery())return false;
    auto selected=EffectiveCartRecipients();const auto removed=std::erase_if(selected,[&](const auto& player){return player.Path==path;});
    CartRecipients=std::move(selected);CartRecipientsChosen=true;
    if(SelectedPlayer==path)SelectedPlayer=CartRecipients.empty()?std::string{}:CartRecipients.front().Path;
    return removed!=0;
}
inline bool ClearCartRecipients(){
    if(ItemGridPicker::SharedCart.Delivery())return false;
    const bool changed=!EffectiveCartRecipients().empty();CartRecipients.clear();CartRecipientsChosen=true;SelectedPlayer.clear();return changed;
}
inline bool SubmitNextCartRecipient(uint64_t id){
    auto request=CartBatch.Request(id);
    if(CartGeneration!=SpawnToolRequests::CurrentGeneration()||!CurrentCartPlayer(UiSnapshot,request.value("Player",std::string{}))){CartBatch.Stop();return false;}
    if(!SpawnToolRequests::TrySubmit(std::move(request))){CartBatch.Stop();return false;}
    CartRequestedAt=std::chrono::steady_clock::now();
    ItemGridPicker::CartStatus="Giving the cart to player "+std::to_string(CartBatch.Completed()+1)+" of "+std::to_string(CartBatch.Total())+"...";return true;
}
inline void RefreshKitMetadata() {
    auto paths=nlohmann::json::array();for(const auto& entry:ItemGridPicker::SharedCart.Entries())paths.push_back(entry.Path);
    if(paths.empty()||KitPending)return;const auto id=++KitSequence;
    if(SpawnToolRequests::TrySubmit({{"Action","InspectKitItems"},{"_RequestId",id},{"Items",std::move(paths)},{"_Source","HelpyStandalone"}}))KitPending=id;
}
inline void RenderKitLibrary() {
    static std::vector<std::string> files;static std::set<std::string> selected;
    static std::string error;static nlohmann::json preview;static char exportName[81]{};
    static bool asJsonc=true,replace=false,append=false,selectedOnly=false,pendingAppend=false;
    static PS::ProcessExitFuture<std::vector<nlohmann::json>> reading;static std::optional<std::vector<nlohmann::json>> loaded;
    static std::filesystem::path pendingDirectory;
    auto& cart=ItemGridPicker::SharedCart;const auto& theme=HelpyImGuiTheme::Current;
    const auto directory=RuneSchemaProfile::Ready?RuneSchemaProfile::Data("loadouts"):HostServices::RuntimeDirectory()/"Helpy"/"kits";
    const auto refresh=[&]{try{files=HelpyKitFiles::List(directory);error.clear();}catch(const std::exception& e){files.clear();error=e.what();}};
    if(reading.valid()&&reading.wait_for(std::chrono::seconds(0))==std::future_status::ready)try{
        auto documents=reading.get();if(ImGui::IsPopupOpen("Import presets"))loaded=std::move(documents);
    }catch(const std::exception& e){error=e.what();}
    ImGui::BeginDisabled(cart.Delivery()!=0||reading.valid());
    if(HelpyImGuiTheme::SoftButton("Import...")){selected.clear();preview=nullptr;loaded.reset();refresh();ImGui::OpenPopup("Import presets");}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CartImport=HelpyPreviewHooks::LastRect();
#endif
    ImGui::SameLine();ImGui::BeginDisabled(cart.Entries().empty());
    if(HelpyImGuiTheme::SoftButton("Export...")){error.clear();replace=false;selectedOnly=!cart.Selection().empty();ImGui::OpenPopup("Export preset");}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CartExport=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cart.Entries().empty());
    if(HelpyImGuiTheme::SoftButton("Clear"))ItemGridPicker::ClearSharedCart();
    ImGui::EndDisabled();ImGui::EndDisabled();
    const auto area=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(theme.X(520),area.x-theme.X(24)),std::min(theme.Y(500),area.y-theme.Y(24))},ImGuiCond_Appearing);
    if(HelpyImGuiTheme::BeginCenteredModal("Import presets",nullptr,0)) {
#if defined(HELPY_PREVIEW)
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();HelpyPreviewHooks::PresetDialog={pos.x,pos.y,pos.x+size.x,pos.y+size.y};
#endif
        ImGui::TextWrapped("Select presets with checkboxes or Ctrl+click. Matching item quantities are combined.");
        ImGui::TextWrapped("Preset folder: %s",directory.string().c_str());
        ImGui::BeginDisabled(reading.valid());
        if(HelpyImGuiTheme::SoftButton("Refresh")){preview=nullptr;selected.clear();refresh();}
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Select all"))selected=std::set<std::string>(files.begin(),files.end());
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Select none"))selected.clear();
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Open folder"))try{
            HelpyKitFiles::NoRedirect(directory);std::filesystem::create_directories(directory);
            if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("The preset folder could not be opened");
        }catch(const std::exception& e){error=e.what();}
        if(ImGui::BeginChild("##kit-files",ImVec2(0,theme.Y(180)),true)) {
            if(files.empty())ImGui::TextWrapped("No presets found. Export one, or place shared .json/.jsonc presets in this folder.");
            for(const auto& file:files){
                ImGui::PushID(file.c_str());bool checked=selected.contains(file);
                if(ImGui::Checkbox("##pick",&checked)){if(checked)selected.insert(file);else selected.erase(file);}
                ImGui::SameLine();if(ImGui::Selectable(file.c_str(),selected.contains(file))){
                    if(!ImGui::GetIO().KeyCtrl)selected.clear();
                    if(ImGui::GetIO().KeyCtrl&&selected.contains(file))selected.erase(file);else selected.insert(file);
                    preview=nullptr;try{preview=HelpyKitFiles::Read(directory,file);error.clear();}catch(const std::exception& e){error=e.what();}
                }ImGui::PopID();
            }
        }ImGui::EndChild();
        ImGui::Text("%zu presets selected",selected.size());
        if(preview.is_object())ImGui::TextWrapped("Last preview: %s (%zu item types)",preview.value("Name",std::string{}).c_str(),preview.at("Items").size());
        ImGui::Checkbox("Add to current cart",&append);
        ImGui::TextWrapped(append?"Adds to staged items. Delivery still requires Give items.":"Replaces staged items. Delivery still requires Give items.");
        ImGui::BeginDisabled(selected.empty()||cart.Delivery()!=0);
        if(HelpyImGuiTheme::SoftButton("Load selected presets")){
            pendingDirectory=directory;pendingAppend=append;error.clear();
            reading=std::async(std::launch::async,[folder=directory,names=selected]{
                std::vector<nlohmann::json> result;for(const auto& file:names)result.push_back(HelpyKitFiles::Read(folder,file));return result;
            });
        }ImGui::EndDisabled();ImGui::EndDisabled();
        if(reading.valid())ImGui::TextDisabled("Reading selected presets...");
        if(loaded)try{
            auto documents=std::move(*loaded);loaded.reset();if(directory!=pendingDirectory)throw std::runtime_error("Profile changed; select the presets again");
            cart.ImportKits(documents,pendingAppend,[](const std::string& path){
                if(UiSnapshot.contains("Items")&&UiSnapshot["Items"].is_array())for(const auto& row:UiSnapshot["Items"])if(row.is_object()&&row.value("Path",std::string{})==path)return row;
                return nlohmann::json::object();
            });
            ItemGridPicker::CartStatus="Presets loaded. Review quantities and recipients, then choose Give items.";RefreshKitMetadata();error.clear();ImGui::CloseCurrentPopup();
        }catch(const std::exception& e){error=e.what();}
        if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
        ImGui::BeginDisabled(reading.valid());if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndDisabled();ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({std::min(theme.X(520),area.x-theme.X(24)),std::min(theme.Y(330),area.y-theme.Y(24))},ImGuiCond_Appearing);
    if(HelpyImGuiTheme::BeginCenteredModal("Export preset",nullptr,0)) {
#if defined(HELPY_PREVIEW)
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();HelpyPreviewHooks::PresetDialog={pos.x,pos.y,pos.x+size.x,pos.y+size.y};
#endif
        ImGui::TextWrapped("Save item paths and quantities. Player, delivery and equip actions are not stored.");
        ImGui::TextWrapped("Preset folder: %s",directory.string().c_str());
        ImGui::SetNextItemWidth(theme.X(350));if(ImGui::InputText("Preset name",exportName,sizeof(exportName)))replace=false;
        ImGui::BeginDisabled(cart.Selection().empty());ImGui::Checkbox("Selected cart items only",&selectedOnly);ImGui::EndDisabled();
        ImGui::Checkbox("JSONC (includes comments)",&asJsonc);ImGui::Checkbox("Replace existing preset (preserve backup)",&replace);
        if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
        ImGui::BeginDisabled(!HelpyKitFiles::FileName(exportName)||cart.Entries().empty()||cart.Delivery()!=0||(selectedOnly&&cart.Selection().empty()));
        if(HelpyImGuiTheme::SoftButton("Save preset"))try{
            const auto file=HelpyKitFiles::Write(directory,exportName,asJsonc,cart,replace,selectedOnly);
            ItemGridPicker::CartStatus="Preset exported: "+file.string();error.clear();ImGui::CloseCurrentPopup();
        }catch(const std::exception& e){error=e.what();}
        ImGui::EndDisabled();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
inline void PollCartDelivery() {
    SyncCartWorld();ReconcileCartRecipients(UiSnapshot);
    nlohmann::json receipt;const auto request=ItemGridPicker::SharedCart.Delivery();
    if(KitPending&&SpawnToolRequests::TakeCompleted(KitPending,receipt)){KitPending=0;if(receipt.contains("KitMetadata"))ItemGridPicker::SharedCart.RefreshMetadata(receipt["KitMetadata"]);}
    if(CartCheckPending&&SpawnToolRequests::TakeCompleted(CartCheckPending,receipt)){
        CartCheckPending=0;if(receipt.contains("CartValidation"))ItemGridPicker::SharedCart.ApplyValidation(receipt["CartValidation"]);
    }
    const auto checkNow=std::chrono::steady_clock::now();
    if(CartCheckPending&&checkNow-CartCheckRequested>std::chrono::seconds(15)&&!SpawnToolRequests::IsWaiting())CartCheckPending=0;
#if !defined(HELPY_PREVIEW)
    if(!request&&!CartCheckPending&&!KitPending&&!SpawnToolRequests::IsWaiting()&&checkNow>=CartCheckNext){
        CartCheckNext=checkNow+std::chrono::milliseconds(500);auto paths=nlohmann::json::array();
        for(const auto& entry:ItemGridPicker::SharedCart.Entries())if(!ItemGridPicker::SharedCart.Validation(entry.Path)){
            paths.push_back(entry.Path);if(paths.size()==4)break;
        }
        if(!paths.empty()){const auto id=++KitSequence;
            if(SpawnToolRequests::TrySubmit({{"Action","ValidateCartItems"},{"_RequestId",id},{"Items",std::move(paths)},{"_Source","HelpyStandalone"}})){CartCheckPending=id;CartCheckRequested=checkNow;}
        }
    }
#endif
    if(request){
        if(CartBatch.Pending()&&SpawnToolRequests::TakeCompleted(CartBatch.PendingId(),receipt))CartBatch.Accept(CartBatch.PendingId(),receipt);
        if(CartBatch.Pending()&&std::chrono::steady_clock::now()-CartRequestedAt>std::chrono::seconds(30)){
            // A busy/paused game thread can legitimately take longer than the
            // UI timeout. Keep the original one-shot request alive; requiring
            // another click while it is running risks granting the cart twice.
            if(SpawnToolRequests::IsWaiting())ItemGridPicker::CartStatus="Waiting for the game to finish this delivery. No second click is needed.";
            else if(CartDeliveryBatch::MissingReceiptExpired(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-CartRequestedAt).count(),false))CartBatch.Stop();
        }
        if(CartBatch.Finished()){
            ItemGridPicker::SharedCart.CompleteDelivery(request,CartBatch.Aggregate());
            CartNeedsReview=CartBatch.Stopped()&&!ItemGridPicker::SharedCart.Entries().empty();CartReviewAcknowledged=false;
            ItemGridPicker::CartStatus=CartNeedsReview?"Delivery stopped or unconfirmed. Remaining cart retained. Check Delivery results for the exact reason; inspect inventories before retrying.":
                "Delivery finished for "+std::to_string(CartBatch.Total())+" player(s). Confirmed and rejected entries cleared; see Delivery results.";
        }else if(!CartBatch.Pending()&&!SpawnToolRequests::IsWaiting()&&SpawnToolRequests::IsAvailable()){
            if(!SubmitNextCartRecipient(++CartSequence)){ItemGridPicker::SharedCart.CancelDelivery();CartNeedsReview=true;CartReviewAcknowledged=false;ItemGridPicker::CartStatus="Next recipient was not submitted. Check completed recipients before retrying.";}
        }
    }
}
inline bool RenderCartRecipient(const nlohmann::json& result) {
    SyncCartWorld();ReconcileCartRecipients(result);
    bool changed=false;const auto selected=EffectiveCartRecipients();auto label=selected.empty()?std::string("Select players"):selected.size()==1?selected[0].Name:std::to_string(selected.size())+" players selected";
    const auto* players=result.is_object()&&result.contains("Players")&&result["Players"].is_array()?&result["Players"]:nullptr;
    ImGui::BeginDisabled(ItemGridPicker::SharedCart.Delivery()!=0);
    const auto editableBefore=EffectiveCartRecipients();
    const float clearWidth=editableBefore.empty()?0.0f:ImGui::CalcTextSize("Clear").x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(60.0f,ImGui::GetContentRegionAvail().x-clearWidth));
    if(ImGui::BeginCombo("##item-recipient",label.c_str())){
        if(players)for(const auto& row:*players){const auto path=ItemPresentation::Text(row,"Path");if(path.empty())continue;
            bool checked=std::any_of(selected.begin(),selected.end(),[&](const auto& item){return item.Path==path;});
            ImGui::PushID(path.c_str());if(ImGui::Selectable(RuneSchemaProfile::Name(row).c_str(),checked,ImGuiSelectableFlags_DontClosePopups)){checked=!checked;
                if(!CartRecipientsChosen){CartRecipients=selected;CartRecipientsChosen=true;}
                if(checked){if(CartRecipients.size()<CartDeliveryBatch::Plan::MaxPlayers){CartRecipients.push_back({path,RuneSchemaProfile::Name(row)});SelectedPlayer=path;changed=true;}}
                else changed=RemoveCartRecipient(path)||changed;
            }ImGui::PopID();}
        if(ImGui::Selectable("Refresh players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});ImGui::EndCombo();
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RecipientRemove.clear();HelpyPreviewHooks::RecipientsClear={};
#endif
    if(!editableBefore.empty()){
        ImGui::SameLine();if(ImGui::Button("Clear##recipients"))changed=ClearCartRecipients()||changed;
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Clear recipients (keep cart items)");
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::RecipientsClear=HelpyPreviewHooks::LastRect();
#endif
    }
    // These controls remain available even when a player is no longer in the
    // dropdown. Removal only edits recipients; staged items stay untouched.
    std::string remove;
    const auto editable=EffectiveCartRecipients();
    if(!editable.empty()){
    if(ImGui::BeginChild("##selected-recipients",ImVec2(0,ImGui::GetTextLineHeightWithSpacing()*float(std::min<size_t>(editable.size(),3))),false))for(const auto& player:editable){
        ImGui::PushID(player.Path.c_str());if(ImGui::SmallButton("X##remove-recipient"))remove=player.Path;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::RecipientRemove[player.Path]=HelpyPreviewHooks::LastRect();
#endif
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Remove recipient");
        ImGui::SameLine();ImGui::TextWrapped("%s",player.Name.c_str());ImGui::PopID();
    }ImGui::EndChild();}
    if(!remove.empty())changed=RemoveCartRecipient(remove)||changed;
    if(EffectiveCartRecipients().size()>1)ImGui::TextDisabled("Same quantities for each player.");
    ImGui::EndDisabled();return changed;
}
inline bool DrawCartCheck(const ImVec2& bottomLeft,const std::string& state,const std::string& message){
#if defined(HELPY_PREVIEW)
    if(state=="Confirmed")++HelpyPreviewHooks::CartVerifiedBadges;else if(state=="Rejected")++HelpyPreviewHooks::CartRejectedBadges;else ++HelpyPreviewHooks::CartUncertainBadges;
#endif
    const auto& theme=HelpyImGuiTheme::Current;const float r=theme.Y(8);const ImVec2 c{bottomLeft.x+theme.X(12),bottomLeft.y-theme.Y(12)};
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::DeliveryBadge={c.x-r,c.y-r,c.x+r,c.y+r};HelpyPreviewHooks::DeliveryBadgeBottomLeft=bottomLeft;
#endif
    auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(state=="Confirmed"?theme.success:state=="Rejected"?theme.error:theme.warning);
    draw->AddCircleFilled(c,r,ImGui::GetColorU32(theme.panelAlt));
    if(state=="Confirmed"){
        draw->AddLine({c.x-r*.55f,c.y},{c.x-r*.12f,c.y+r*.45f},color,theme.X(2));
        draw->AddLine({c.x-r*.12f,c.y+r*.45f},{c.x+r*.6f,c.y-r*.5f},color,theme.X(2));
    }else if(state=="Rejected"){
        draw->AddLine({c.x-r*.45f,c.y-r*.45f},{c.x+r*.45f,c.y+r*.45f},color,theme.X(2));
        draw->AddLine({c.x-r*.45f,c.y+r*.45f},{c.x+r*.45f,c.y-r*.45f},color,theme.X(2));
    }else{draw->AddLine({c.x,c.y-r*.5f},{c.x,c.y+r*.1f},color,theme.X(2));draw->AddCircleFilled({c.x,c.y+r*.5f},theme.X(1.5f),color);}
    const bool hover=ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect({c.x-r,c.y-r},{c.x+r,c.y+r});
    if(hover)HelpyImGuiTheme::TooltipAt({c.x-r,c.y-r},{c.x+r,c.y+r},message.c_str());return hover;
}
inline void RenderCartDock(ImVec2 size,const std::function<void()>& recipient) {
    static bool equipOnReceipt=false;
    const auto& theme=HelpyImGuiTheme::Current;
    auto& cart=ItemGridPicker::SharedCart;
    const bool empty=cart.Entries().empty(),delivering=cart.Delivery()!=0;
    const auto deliver=[&](int mode,bool selectedOnly=false){
        const auto recipients=EffectiveCartRecipients();if(recipients.empty()||!SpawnToolRequests::IsAvailable()||delivering||empty||CartNeedsReview)return;
        auto items=cart.Payload(mode,selectedOnly);if(items.empty())return;
        const auto request=++CartSequence;
        try{CartBatch.Begin(items,recipients,equipOnReceipt);if(!cart.BeginDelivery(request,mode,selectedOnly))return;
            if(SpawnToolRequests::IsWaiting())ItemGridPicker::CartStatus="Delivery queued. Waiting for the current operation; no second click is needed.";
            else if(!SubmitNextCartRecipient(request)){cart.CancelDelivery();ItemGridPicker::CartStatus="Delivery was not submitted; cart retained.";}
        }catch(const std::exception& error){cart.CancelDelivery();ItemGridPicker::CartStatus=error.what();}
    };
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);
    if(ImGui::BeginChild("##item-cart-dock",size,true,ImGuiWindowFlags_AlwaysUseWindowPadding)){
        const auto pos=ImGui::GetWindowPos(),bounds=ImGui::GetWindowSize();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::Cart=HelpyPreviewHooks::WindowRect();
#endif
        if(!delivering&&ImGui::BeginDragDropTargetCustom(ImRect(pos,ImVec2(pos.x+bounds.x,pos.y+bounds.y)),ImGui::GetID("cart-transfer"))){
            if(const auto* payload=ImGui::AcceptDragDropPayload("RS_ITEM_ROW")){
                try{
                    const auto item=ItemTransfer::Decode(payload->Data,payload->DataSize);
                    const auto* row=ItemGridPicker::Find(UiSnapshot,item.at("Path").get_ref<const std::string&>().c_str());
                    ItemGridPicker::AddToCart(row?*row:item);
                }
                catch(const std::exception& error){ItemGridPicker::CartStatus=error.what();}
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::AlignTextToFramePadding();ImGui::Text("Cart (%zu)",cart.Entries().size());
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-theme.X(58)));
        if(ImGui::SmallButton("More"))ImGui::OpenPopup("##cart-more");
        if(ImGui::BeginPopup("##cart-more")){
            if(ImGui::MenuItem("Clear cart",nullptr,false,!empty&&!delivering))ItemGridPicker::ClearSharedCart();
            ImGui::Separator();
            const bool ready=!EffectiveCartRecipients().empty()&&SpawnToolRequests::IsAvailable()&&!delivering&&!empty&&!CartNeedsReview;
            if(ImGui::MenuItem("Give one of each",nullptr,false,ready))deliver(1);
            if(ImGui::MenuItem("Give half stacks",nullptr,false,ready))deliver(2);
            if(ImGui::MenuItem("Give full stacks",nullptr,false,ready))deliver(3);
            ImGui::EndPopup();
        }
        RenderKitLibrary();
        ImGui::BeginDisabled(delivering||cart.Entries().empty());
        bool all=!cart.Entries().empty()&&cart.Selection().size()==cart.Entries().size();
        const bool mixed=!all&&!cart.Selection().empty();
        if(mixed)ImGui::PushItemFlag(ImGuiItemFlags_MixedValue,true);
        if(ImGui::Checkbox("All##cart-select-all",&all))cart.SelectAll(all);
        if(mixed)ImGui::PopItemFlag();
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Select or clear all cart items. Ctrl+click cards to change individual selections.");
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::CartSelectAll=HelpyPreviewHooks::LastRect();
#endif
        ImGui::SameLine();ImGui::TextDisabled("%zu selected",cart.Selection().size());
        ImGui::EndDisabled();
        if(!cart.Selection().empty()){
            ImGui::BeginDisabled(delivering);
            if(ImGui::SmallButton("Up"))cart.MoveSelected(-1);
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::CartMoveUp=HelpyPreviewHooks::LastRect();
#endif
            ImGui::SameLine();if(ImGui::SmallButton("Down"))cart.MoveSelected(1);
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::CartMoveDown=HelpyPreviewHooks::LastRect();
#endif
            ImGui::SameLine();if(ImGui::SmallButton("Remove selected"))cart.RemoveSelected();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::CartRemoveSelected=HelpyPreviewHooks::LastRect();
#endif
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        std::string removePath;
        const auto recipientCount=EffectiveCartRecipients().size();
        const float recipientHeight=recipientCount?ImGui::GetTextLineHeightWithSpacing()*float(std::min<size_t>(recipientCount,3)+(recipientCount>1?1:0)):0.0f;
        const float footer=theme.Y((ItemGridPicker::CartStatus.empty()?146.0f:190.0f)+(CartNeedsReview?90.0f:0.0f))+recipientHeight+(cart.Selection().empty()?0.f:ImGui::GetFrameHeightWithSpacing());
        const float listHeight=std::max(theme.Y(12.0f),ImGui::GetContentRegionAvail().y-footer);
        if(ImGui::BeginChild("##cart-items",ImVec2(0,listHeight),false)){
            if(empty&&cart.Outcomes().empty()){ImGui::Spacing();ImGui::TextWrapped("Drag items here");ImGui::TextDisabled("or right-click an item.");}
            const auto entries=cart.Entries();
            const float gap=ImGui::GetStyle().ItemSpacing.x;
            const float available=std::max(1.f,ImGui::GetContentRegionAvail().x);
            const int columns=ItemBayLayout::Columns(available,std::max(theme.X(130),theme.X(theme.gridMinCardWidth)),gap,ItemBayLayout::DefaultColumns);
            const float cardWidth=std::max(1.f,(available-gap*(columns-1))/columns);
            const float cardHeight=theme.Y(std::max(96.f,theme.gridCardHeight));
            const float buttonWidth=std::max(theme.X(22),ImGui::GetFrameHeight());
            const float quantityMin=ImGui::CalcTextSize("10000").x+ImGui::GetStyle().FramePadding.x*2;
            const float fullSelectWidth=ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+ImGui::CalcTextSize("Select").x;
            const bool compactSelect=cardWidth<fullSelectWidth+quantityMin+gap;
            const float selectWidth=compactSelect?ImGui::GetFrameHeight():fullSelectWidth;
            const bool wrapControls=cardWidth<selectWidth+quantityMin+buttonWidth*3+gap*4;
            const float quantityWidth=std::max(1.f,cardWidth-selectWidth-gap-(wrapControls?0.f:buttonWidth*3+gap*3));
            const float actionWidth=wrapControls?std::max(1.f,(cardWidth-gap*2)/3):buttonWidth;
            const float controlsHeight=ImGui::GetFrameHeight()*(wrapControls?2.f:1.f)+ImGui::GetStyle().ItemSpacing.y*(wrapControls?2.f:1.f);
            const float rowHeight=cardHeight+controlsHeight+ImGui::GetStyle().ItemSpacing.y;
            ImGui::BeginDisabled(delivering);
            ImGuiListClipper cartClip;cartClip.Begin(static_cast<int>((entries.size()+columns-1)/columns),rowHeight);
            while(cartClip.Step())for(int line=cartClip.DisplayStart;line<cartClip.DisplayEnd;++line)for(int column=0;column<columns;++column){
                const auto index=static_cast<size_t>(line)*columns+column;if(index>=entries.size())break;
                const auto& entry=entries[index];const auto name=ItemPresentation::Name(entry.Row);
                if(column)ImGui::SameLine();ImGui::BeginGroup();ImGui::PushID(entry.Path.c_str());
                const auto cardMin=ImGui::GetCursorScreenPos();const ImVec2 cardMax(cardMin.x+cardWidth,cardMin.y+cardHeight);
                if(ImGui::InvisibleButton("##cart-slot",{cardWidth,cardHeight}))cart.Select(entry.Path,ImGui::GetIO().KeyCtrl);
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::CartRects[entry.Path]=HelpyPreviewHooks::LastRect();
#endif
                ItemGridPicker::DrawPlacard(entry.Row,cardMin,cardMax,cart.Selection().contains(entry.Path),ImGui::IsItemHovered(),theme.X(20));
                const auto* check=cart.Validation(entry.Path);
                const bool checkHovered=check&&DrawCartCheck({cardMin.x,cardMax.y},check->Valid?"Confirmed":"Rejected",check->Message);
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID|ImGuiDragDropFlags_SourceNoPreviewTooltip)){
                    ImGui::SetDragDropPayload("RS_CART_PATH",entry.Path.c_str(),entry.Path.size()+1);ItemGridPicker::HeldItem(entry.Row,"Cart item");ImGui::EndDragDropSource();
                }
                if(ImGui::BeginPopupContextItem("##cart-actions")){if(ImGui::MenuItem("Remove from cart"))removePath=entry.Path;ImGui::EndPopup();}
                if(!checkHovered&&HelpyImGuiTheme::TooltipReady("cart:"+entry.Path,ImGui::IsItemHovered()))ItemGridPicker::VerboseTooltip(entry.Row,name,entry.Path,false,true);
                bool selected=cart.Selection().contains(entry.Path);
                if(ImGui::Checkbox(compactSelect?"###cart-item":"Select###cart-item",&selected))cart.Select(entry.Path,true);
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::CartChecks[entry.Path]=HelpyPreviewHooks::LastRect();
#endif
                if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Select this item for bulk actions. Ctrl+click its card to add or remove it from the selection.");
                ImGui::SameLine();int quantity=entry.Quantity;ImGui::SetNextItemWidth(quantityWidth);
                if(ImGui::InputInt("##quantity",&quantity,0,0))cart.SetQuantity(entry.Path,quantity);
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::CartQuantities[entry.Path]=HelpyPreviewHooks::LastRect();
#endif
                if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Quantity per selected player");
                if(!wrapControls)ImGui::SameLine();
                ImGui::BeginDisabled(quantity<=1);
                if(ImGui::Button("-",{actionWidth,0}))cart.SetQuantity(entry.Path,quantity-1);
                ImGui::EndDisabled();if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip(quantity<=1?"Minimum quantity: 1":"Decrease quantity");
                ImGui::SameLine();ImGui::BeginDisabled(quantity>=10000);
                if(ImGui::Button("+",{actionWidth,0}))cart.SetQuantity(entry.Path,quantity+1);
                ImGui::EndDisabled();if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip(quantity>=10000?"Maximum quantity: 10000":"Increase quantity");
                ImGui::SameLine();if(ImGui::Button("X",{actionWidth,0}))removePath=entry.Path;
                if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Remove from cart");
                ImGui::PopID();ImGui::EndGroup();
            }
            ImGui::EndDisabled();
            if(!cart.Outcomes().empty()){
                ImGui::SeparatorText("Last delivery");
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::DeliveryHeading=HelpyPreviewHooks::LastRect();
#endif
                const auto reorder=[&](const std::string& path={}){
                    if(cart.Reorder(path))ItemGridPicker::CartStatus="Reordered into cart. Review quantities and players, then choose Give items.";
                    else ItemGridPicker::CartStatus="Reorder could not fit in the cart. Keep at most 64 items and 10000 of each.";
                };
                if(ImGui::BeginPopupContextItem("##last-delivery-actions")){
                    const bool confirmed=std::any_of(cart.Outcomes().begin(),cart.Outcomes().end(),[](const auto& outcome){return outcome.State=="Confirmed";});
                    if(ImGui::MenuItem("Reorder confirmed items",nullptr,false,confirmed&&!delivering&&!CartNeedsReview))reorder();
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::ReorderAction=HelpyPreviewHooks::LastRect();
#endif
                    ImGui::EndPopup();
                }
                ImGui::BeginDisabled(delivering||CartNeedsReview);
                bool allOutcomes=!cart.OutcomeSelection().empty()&&cart.OutcomeSelection().size()==static_cast<size_t>(std::count_if(cart.Outcomes().begin(),cart.Outcomes().end(),[](const auto& o){return o.State=="Confirmed";}));
                if(ImGui::Checkbox("All confirmed",&allOutcomes))cart.SelectAllOutcomes(allOutcomes);
                ImGui::BeginDisabled(cart.OutcomeSelection().empty());
                if(HelpyImGuiTheme::SoftButton("Reorder selected")){
                    ItemGridPicker::CartStatus=cart.ReorderSelected()?"Selected delivery items added to cart. Review quantities and recipients before giving.":"Selected items could not fit in the cart; no quantities changed.";
                }
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::ReorderSelected=HelpyPreviewHooks::LastRect();
#endif
                ImGui::EndDisabled();ImGui::EndDisabled();
                size_t outcomeIndex=0;for(const auto& outcome:cart.Outcomes()){
                    if(outcomeIndex++%columns)ImGui::SameLine();ImGui::BeginGroup();
                    ImGui::PushID(outcome.Item.Path.c_str());
                    ImGui::BeginDisabled(delivering||CartNeedsReview||outcome.State!="Confirmed");
                    bool selected=cart.OutcomeSelection().contains(outcome.Item.Path);
                    if(ImGui::Checkbox("Select##delivery-item",&selected))cart.SelectOutcome(outcome.Item.Path,true);
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::DeliveryChecks[outcome.Item.Path]=HelpyPreviewHooks::LastRect();
#endif
                    const auto start=ImGui::GetCursorScreenPos();
                    if(ImGui::InvisibleButton("##last-delivery-item",{cardWidth,cardHeight}))cart.SelectOutcome(outcome.Item.Path,ImGui::GetIO().KeyCtrl);
                    ItemGridPicker::DrawPlacard(outcome.Item.Row,start,{start.x+cardWidth,start.y+cardHeight},cart.OutcomeSelection().contains(outcome.Item.Path),ImGui::IsItemHovered(),theme.X(20));
                    ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::DeliveryRects[outcome.Item.Path]=HelpyPreviewHooks::LastRect();
#endif
                    if(ImGui::BeginPopupContextItem("##reorder-item")){
                        if(ImGui::MenuItem("Reorder",nullptr,false,outcome.State=="Confirmed"&&!delivering&&!CartNeedsReview))reorder(outcome.Item.Path);
#if defined(HELPY_PREVIEW)
                        HelpyPreviewHooks::ReorderAction=HelpyPreviewHooks::LastRect();
#endif
                        ImGui::EndPopup();
                    }
                    DrawCartCheck({start.x,start.y+cardHeight},outcome.State,outcome.Message);
                    ImGui::PopID();
                    ImGui::EndGroup();
                }
            }
        }ImGui::EndChild();
        if(!removePath.empty())ItemGridPicker::RemoveFromCart(removePath);
        ImGui::Separator();ImGui::TextDisabled("Give to");recipient();
        ImGui::BeginDisabled(delivering);ImGui::Checkbox("Equip on receipt",&equipOnReceipt);ImGui::EndDisabled();
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Opt-in native equipment transfer. One new equipment instance per item type; empty slots only. Non-equipment stays in inventory. Equip errors never repeat delivery.");
        ImGui::BeginDisabled(EffectiveCartRecipients().empty()||!SpawnToolRequests::IsAvailable()||delivering||empty||CartNeedsReview);
        if(!cart.Selection().empty()&&HelpyImGuiTheme::SoftButton("Give selected",ImVec2(-1,0)))deliver(0,true);
        ImGui::PushStyleColor(ImGuiCol_Button,theme.accent);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,theme.accentHover);ImGui::PushStyleColor(ImGuiCol_ButtonActive,theme.accentActive);ImGui::PushStyleColor(ImGuiCol_Text,theme.onAccent);
        if(HelpyImGuiTheme::SoftButton(delivering?"Giving items...":cart.Selection().empty()?"Give items":"Give all items",ImVec2(-1,0)))deliver(0);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::Give=HelpyPreviewHooks::LastRect();
#endif
        ImGui::PopStyleColor(4);ImGui::EndDisabled();
        if(!ItemGridPicker::CartStatus.empty())ImGui::TextWrapped("%s",ItemGridPicker::CartStatus.c_str());
        if(!CartBatch.Receipts().empty()&&ImGui::TreeNode("Delivery results")){
            for(const auto& result:CartBatch.Receipts()){
                ImGui::TextWrapped("%s: %s",result.value("Name",std::string{}).c_str(),result["Receipt"].value("Status",std::string("Native confirmation received")).c_str());
                const auto rows=result["Receipt"].find("GrantResults");if(rows!=result["Receipt"].end()&&rows->is_array())for(const auto& row:*rows)if(row.is_object()){
                    ImGui::TextWrapped("%s: %s — %s",row.value("Item",std::string{}).c_str(),row.value("State",std::string{}).c_str(),row.value("Message",std::string{}).c_str());
                    if(row.contains("Equip")&&row["Equip"].is_object())ImGui::TextWrapped("Equip: %s — %s",row["Equip"].value("State",std::string{}).c_str(),row["Equip"].value("Message",std::string{}).c_str());}
            }ImGui::TreePop();
        }
        if(CartNeedsReview){ImGui::Checkbox("I checked affected inventories",&CartReviewAcknowledged);ImGui::BeginDisabled(!CartReviewAcknowledged);if(ImGui::SmallButton("Allow a new delivery")){CartNeedsReview=false;CartReviewAcknowledged=false;ItemGridPicker::CartStatus="Cart unlocked after inventory review. Quantities are per selected player.";}ImGui::EndDisabled();}
    }ImGui::EndChild();ImGui::PopStyleColor();
}
inline void RefreshSnapshot(int category,bool force=false) {
    SyncCartWorld();
    if(category!=UiCategory){UiCategory=category;UiRevision=0;force=true;}
    const auto prior=UiRevision;SpawnToolRequests::ReadQuickIfChanged(UiRevision,category,UiSnapshot);
    // A read can discover a missed generation push. Discard the old selection
    // before reading the new generation's roster, even if the UI was hidden.
    if(CartGeneration!=SpawnToolRequests::CurrentGeneration()){SyncCartWorld();SpawnToolRequests::ReadQuickIfChanged(UiRevision,category,UiSnapshot);force=true;}
    ReconcileCartRecipients(UiSnapshot);
    if(force||UiRevision!=prior){LazyCatalog::Apply(UiSnapshot,UiRevision);LazyCatalog::ReleaseFailedWindow(UiRevision,UiSnapshot);LazyCatalog::Overlay(UiSnapshot,category);}
    else LazyCatalog::Overlay(UiSnapshot,category);
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(generation!=DefinitionGeneration){DefinitionGeneration=generation;DefinitionPending=0;InspectedDefinitions.clear();DefinitionStatus.clear();}
    PlayerBadges::Update(UiSnapshot);
    nlohmann::json receipt;
    if(DefinitionPending&&SpawnToolRequests::TakeCompleted(DefinitionPending,receipt)) {
        DefinitionPending=0;DefinitionStatus=receipt.value("Status",std::string{});
        if(receipt.contains("SpawnDefinition")){
            auto row=receipt["SpawnDefinition"];
            if(row.is_object()&&row.contains("Key")&&row["Key"].is_string()){
                const auto key=row["Key"].get<std::string>();InspectedDefinitions[key]=std::move(row);
            }else DefinitionStatus="The runtime returned an incomplete spawn definition. Inspect it again.";
        }
    }
    if(UiSnapshot.contains("Definitions"))for(auto& row:UiSnapshot["Definitions"]) {
        const auto found=InspectedDefinitions.find(row.value("Key",std::string{}));if(found!=InspectedDefinitions.end())row=found->second;
    }
}
inline void Render(View view) {
    auto& player=SelectedPlayer;
    static char itemFilter[257]{},dropItemFilter[257]{},buildingFilter[257]{},selectedBuilding[1024]{};
    static std::string buildingQuery;static MatchCache buildingMatches;static ActorUiState actors[2];
    static int count=1;static float distance=5,scale=1,yaw=0,height=0,radius=500,gridX=0,gridY=0,gridSize=100;static bool buildingConfirmed=false,buildingPermanent=true;
    static bool grid=false,overrideGhost=false,ghost=false,overrideDrops=false;
    static int dropMin=1,dropMax=1,npcDuration=HelpyNodes::DefaultDuration;static float chance=100;
    static char item[1024]{};
    static int buildingTime=0;static bool rebuildCatalogs=false,allowDeconstruction=false;
    static bool buildingAtCoordinates=false;static double buildingX=0,buildingY=0;
    static ItemGridPicker::State itemPickerState{};
    static ItemGridPicker::State cloneItemPickerState{};
    static ItemGridPicker::State dropItemPickerState{};
    static nlohmann::json drops=nlohmann::json::array();
    RefreshSnapshot(CatalogCategory(view));const auto& result=UiSnapshot;const auto resultRevision=UiRevision;
    const auto& ui=HelpyImGuiTheme::Current;
    if(view==View::Recipes){RecipeStudioPanel::Render(result,resultRevision);return;}
    static std::string lastPlayer;
    if(lastPlayer!=player){lastPlayer=player;buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;}
    const bool itemPage=view==View::Items;
    const int coverageCategory=CatalogCategory(view);
    const auto maintenance=[&]{
    CatalogProgressView::Render(result);
    if(coverageCategory>=0&&!LazyCatalog::For(coverageCategory).Coverage.empty()&&ui.advancedMode)ImGui::TextWrapped("%s",LazyCatalog::For(coverageCategory).Coverage.c_str());
    if(HelpyImGuiTheme::SoftButton("Refresh current list")){buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;
        const int category=CatalogCategory(view);if(category>=0){auto& window=LazyCatalog::For(category);const char* mode=(category==0&&window.Query.empty())?"Random":"Search";LazyCatalog::Retry(category);}}
    SameLineIfRoom(145.0f);if(HelpyImGuiTheme::SoftButton("Refresh players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});
    if(ui.advancedMode){ImGui::Checkbox("I understand Full Scan may load additional candidate assets",&rebuildCatalogs);
    ImGui::BeginDisabled(!rebuildCatalogs);
    if(HelpyImGuiTheme::SoftButton("Full Scan / rebuild live RSMK catalog")) {
        buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;rebuildCatalogs=false;SpawnToolRequests::Submit({{"Action","IndexQuickCatalog"}});
    }
    ImGui::EndDisabled();}
    };
    if(!itemPage&&view!=View::ItemLab)maintenance();
    const auto recipient=[&]{
        if(itemPage){if(RenderCartRecipient(result)){buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;}return;}
        auto label=std::string("Select player");
        if(result.contains("Players"))for(const auto& row:result["Players"])if(row["Path"]==player)label=row["Name"];
        if(itemPage)ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo(itemPage?"##item-recipient":"Player##spawn",label.c_str())) {
            if(result.contains("Players"))for(const auto& row:result["Players"]) {
                const auto path=row["Path"].get<std::string>();ImGui::PushID(path.c_str());
                if(ImGui::Selectable(row["Name"].get_ref<const std::string&>().c_str(),player==path)){player=path;buildingConfirmed=false;actors[0].Confirmed=false;actors[1].Confirmed=false;}
                PlayerBadges::Inline(row);ImGui::PopID();
            }
            if(ImGui::Selectable("Refresh players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});
            ImGui::EndCombo();
        }
    };
    if(!itemPage)recipient();
    if(view==View::Ai||view==View::Resources){SpawnStudioPanel::Render(result,resultRevision,view==View::Resources,ForcedActorMode<0?1:ForcedActorMode,player,[](const nlohmann::json& row){
        if(DefinitionPending)return;const auto id=++DefinitionSequence;
        if(SpawnToolRequests::TrySubmit({{"Action","InspectSpawnDefinition"},{"_Source","HelpyStandalone"},{"_RequestId",id},{"Class",row.at("Class")},{"Key",row.at("Key")},{"ExpectedType",row.at("Type")}}))DefinitionPending=id;
    });
    if(!DefinitionStatus.empty())ImGui::TextWrapped("%s",DefinitionStatus.c_str());
        if(!ui.advancedMode||!ImGui::CollapsingHeader("Legacy advanced spawn authoring"))return;
    }
    const auto invalidateSharedPlacement=[&]{actors[0].Confirmed=false;actors[1].Confirmed=false;buildingConfirmed=false;};
    const auto renderActorSection=[&](int kind,const char* title,ImGuiTreeNodeFlags flags=0) {
        if(!HelpyImGuiTheme::SectionHeader(title,kind==0?HelpyImGuiTheme::Icon::Ai:HelpyImGuiTheme::Icon::Resource,flags))return;
        ImGui::PushID(kind);auto& state=actors[kind];auto& definition=state.Definition;auto& definitionQuery=state.Query;auto& definitionMatches=state.Matches;
        auto& ai=state.ClassPath;auto& name=state.Name;auto& definitionFilter=state.Filter;auto& custom=state.Custom;auto& boss=state.Boss;auto& confirmed=state.Confirmed;
        const bool resource=kind==1;
    if(kind==0&&ForcedActorMode>=0) {
        if(state.Mode!=ForcedActorMode){state.Mode=ForcedActorMode;definition.clear();confirmed=false;}
    } else if(kind==0) {
        if(ImGui::RadioButton("NPCs",state.Mode==0)){state.Mode=0;definition.clear();confirmed=false;}
        SameLineIfRoom(125.0f);if(ImGui::RadioButton("AI / Enemies",state.Mode==1)){state.Mode=1;definition.clear();confirmed=false;}
    } else {
        if(ImGui::RadioButton("Trees",state.Mode==0)){state.Mode=0;definition.clear();confirmed=false;}
        SameLineIfRoom(155.0f);if(ImGui::RadioButton("Stone / Ore / Other",state.Mode==1)){state.Mode=1;definition.clear();confirmed=false;}
    }
    const bool npc=kind==0&&state.Mode==0;
    bool permanent=Authoring::PermanentSpawn.load();
    if(ImGui::RadioButton("Temporary",!permanent)){permanent=false;Authoring::PermanentSpawn=false;invalidateSharedPlacement();}ImGui::SameLine();
    if(ImGui::RadioButton("Permanent",permanent)){permanent=true;Authoring::PermanentSpawn=true;invalidateSharedPlacement();}
    ImGui::TextDisabled("%s",permanent?"Saved as RuneSchema placements and restored when the mod loads.":"Uses the native temporary-spawn and save-exclusion rules.");
    if(kind==0&&!npc) {
        bool overridePower=Authoring::EnemyPower.load()!=-1;
        if(ImGui::Checkbox("Override enemy power level",&overridePower)){Authoring::EnemyPower=overridePower?1:-1;confirmed=false;}
        if(overridePower) {
            int power=Authoring::EnemyPower.load();
            if(ImGui::InputInt("Enemy power level (1..100)",&power)) {Authoring::EnemyPower=std::clamp(power,1,100);confirmed=false;}
            ImGui::TextWrapped("Permanent uses the native AI spawn-point power. Temporary requires a reflected PowerLevel on that AI and refuses unsupported classes; it does not invent stat multipliers.");
        }else ImGui::TextDisabled("Power level: native/default");
    }
    if(!npc&&ImGui::Checkbox(kind==0?"Ad-hoc AI class":"Ad-hoc resource class",&custom))confirmed=false;
    if(npc)custom=false;
    if(custom) {
        if(ImGui::InputText("Blueprint class path",ai,sizeof(ai)))confirmed=false;
        if(ImGui::InputText("Spawn name (optional)",name,sizeof(name)))confirmed=false;
        if(!resource && ImGui::Checkbox("Use custom name for boss title too",&boss))confirmed=false;
    }else {
        if(SearchOnEnter("Search loaded AI/resources","name, class, type or mod id",definitionFilter,sizeof(definitionFilter),definitionQuery))
            LazyCatalog::Search(kind==0?1:2,definitionQuery);
        if(kind==0){state.GridView=HelpyImGuiTheme::Current.aiGridView;state.ViewInitialized=true;if(HelpyImGuiTheme::SoftButton(state.GridView?"List view":"Grid view")){state.GridView=!state.GridView;HelpyImGuiTheme::Current.aiGridView=state.GridView;HelpyImGuiTheme::SaveCurrent();}}
        const int lazyCategory=kind==0?1:2;auto& lazyState=LazyCatalog::For(lazyCategory);
        if(!lazyState.Started&&!lazyState.Pending)LazyCatalog::Search(lazyCategory,definitionQuery);
        CacheDefinitions(result,resultRevision,kind,state.Mode,definitionQuery,definitionMatches);
        if(definitionMatches.Rows.size()<24&&lazyState.HasMore&&!lazyState.Pending)LazyCatalog::More(lazyCategory);
        if(state.MetaRevision!=resultRevision||state.MetaKey!=definition) {
            state.MetaRevision=resultRevision;state.MetaKey=definition;state.SelectedLabel=definition.empty()?std::string("Select spawn"):definition;
            if(result.contains("Definitions"))for(const auto& row:result["Definitions"])if(row.value("Key",std::string{})==definition){state.SelectedLabel="["+row.value("Type",std::string("AI"))+"] "+row.value("Name",row.value("Key",std::string{}));break;}
        }
        if(kind==0&&result.contains("Definitions")&&result["Definitions"].is_array()) {
            const auto& definitionRows=result["Definitions"];const auto& theme=HelpyImGuiTheme::Current;if(state.ResultPaneHeightLogical<0.0f)state.ResultPaneHeightLogical=theme.aiResultHeight;
            const float availableLogical=ImGui::GetContentRegionAvail().y/std::max(0.01f,theme.Scale()*theme.density);const float maxPane=std::max(180.0f,availableLogical-theme.Y(120.0f)/std::max(0.01f,theme.Scale()*theme.density));state.ResultPaneHeightLogical=std::clamp(state.ResultPaneHeightLogical,180.0f,std::max(180.0f,maxPane));
            HelpyImGuiTheme::BeginScroll("AIResults",ImVec2(0,theme.Y(state.ResultPaneHeightLogical)),true);const int total=static_cast<int>(definitionMatches.Rows.size());
            const auto tooltip=[&](const nlohmann::json& row,const std::string& title,const std::string& path){HelpyImGuiTheme::BeginDetailsTooltip();PS::HelpyImGuiTheme::DetailsTextUnformatted(title.c_str());if(!path.empty()){ImGui::Separator();PS::HelpyImGuiTheme::DetailsTextUnformatted(path.c_str());}if(theme.verboseTooltips){const auto field=[&](const char* label,const char* key){const auto v=row.value(key,std::string{});if(!v.empty())PS::HelpyImGuiTheme::DetailsText("%s: %s",label,v.c_str());};field("Key","Key");field("Type","Type");field("Class","Class");field("Mod ID","ModId");field("Resource family","ResourceFamily");if(row.contains("Available"))PS::HelpyImGuiTheme::DetailsText("Available: %s",row.value("Available",true)?"true":"false");const auto reason=row.value("Reason",std::string{});if(!reason.empty())PS::HelpyImGuiTheme::DetailsText("Reason: %s",reason.c_str());}HelpyImGuiTheme::EndDetailsTooltip();};
            if(!state.GridView){const float iconSize=theme.Y(theme.listIconSize),itemHeight=std::max(ImGui::GetTextLineHeight()+theme.Y(4.0f),iconSize),rowHeight=itemHeight+ImGui::GetStyle().ItemSpacing.y;ImGuiListClipper clipper;clipper.Begin(total,rowHeight);while(clipper.Step()){if(clipper.DisplayEnd>=total-4)LazyCatalog::More(1);for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible){const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),type=row.value("Type",std::string("AI")),title=row.value("Name",key),path=row.value("Class",std::string{});const auto label="["+type+"] "+title;const float rowWidth=ImGui::GetContentRegionAvail().x;ImGui::PushID(visible);ImGui::PushID(key.c_str());const auto pos=ImGui::GetCursorScreenPos();if(ImGui::Selectable("##airow",key==definition,0,ImVec2(rowWidth,itemHeight))){definition=key;confirmed=false;}const auto c=ImVec2(pos.x+theme.X(8.0f)+iconSize*.5f,pos.y+itemHeight*.5f);HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Ai,c,iconSize*.72f,key==definition?theme.accentHover:theme.icon);ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x+theme.X(16.0f)+iconSize,pos.y+(itemHeight-ImGui::GetTextLineHeight())*.5f),ImGui::GetColorU32(theme.text),label.c_str());if(HelpyImGuiTheme::TooltipReady(std::string("ai:")+key+":"+std::to_string(visible),ImGui::IsItemHovered()))tooltip(row,title,path);ImGui::PopID();ImGui::PopID();}}}
            else {const float spacing=ImGui::GetStyle().ItemSpacing.x,minCard=theme.X(theme.gridMinCardWidth),avail=std::max(minCard,ImGui::GetContentRegionAvail().x-theme.X(2.0f));const int cols=std::max(1,static_cast<int>((avail+spacing)/(minCard+spacing)));const float cardW=(avail-spacing*(cols-1))/cols,cardH=theme.Y(theme.gridCardHeight),icon=theme.Y(theme.gridIconSize),gridRowH=cardH+ImGui::GetStyle().ItemSpacing.y;const int rows=(total+cols-1)/cols;ImGuiListClipper clipper;clipper.Begin(rows,gridRowH);while(clipper.Step()){if(clipper.DisplayEnd*cols>=total-4)LazyCatalog::More(1);for(int gr=clipper.DisplayStart;gr<clipper.DisplayEnd;++gr){for(int col=0;col<cols;++col){const int visible=gr*cols+col;if(visible>=total)break;const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),title=row.value("Name",key),path=row.value("Class",std::string{});ImGui::PushID(visible);ImGui::PushID(key.c_str());const auto a=ImGui::GetCursorScreenPos();const bool clicked=ImGui::InvisibleButton("##aicard",ImVec2(cardW,cardH));const auto b=ImVec2(a.x+cardW,a.y+cardH);auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,b,ImGui::GetColorU32(key==definition?theme.selection:theme.panelAlt),theme.X(theme.childRounding));draw->AddRect(a,b,ImGui::GetColorU32(key==definition?theme.accentHover:theme.border),theme.X(theme.childRounding),0,key==definition?theme.X(2.0f):theme.X(1.0f));HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Ai,ImVec2((a.x+b.x)*.5f,a.y+theme.Y(12.0f)+icon*.5f),icon,key==definition?theme.accentHover:theme.icon);const auto text=title.size()>40?title.substr(0,37)+"...":title;const auto ts=ImGui::CalcTextSize(text.c_str());draw->AddText(ImVec2(a.x+(cardW-ts.x)*.5f,b.y-theme.Y(29.0f)),ImGui::GetColorU32(theme.text),text.c_str());if(clicked){definition=key;confirmed=false;}if(HelpyImGuiTheme::TooltipReady(std::string("ai:")+key+":"+std::to_string(visible),ImGui::IsItemHovered()))tooltip(row,title,path);ImGui::PopID();ImGui::PopID();if(col<cols-1&&visible+1<total)ImGui::SameLine();}}}}
            HelpyImGuiTheme::EndScroll(theme);ImGui::PushID("ai-results-splitter");const float sh=std::max(theme.Y(theme.splitterWidth),theme.Y(5.0f));ImGui::InvisibleButton("##split",ImVec2(ImGui::GetContentRegionAvail().x,sh));const bool active=ImGui::IsItemActive(),hover=ImGui::IsItemHovered();if(active||hover)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);if(active&&ImGui::GetIO().MouseDelta.y!=0.0f){state.ResultPaneHeightLogical+=ImGui::GetIO().MouseDelta.y/std::max(0.01f,theme.Scale()*theme.density);state.ResultPaneHeightLogical=std::clamp(state.ResultPaneHeightLogical,180.0f,std::max(180.0f,maxPane));HelpyImGuiTheme::Current.aiResultHeight=state.ResultPaneHeightLogical;}static bool aiPaneDirty=false;if(active)aiPaneDirty=true;if(aiPaneDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();aiPaneDirty=false;}ImGui::PopID();
            if(definitionMatches.Rows.empty()){const auto& lazy=LazyCatalog::For(1);if(lazy.Pending)ImGui::TextDisabled("Loading runtime definitions...");else if(!lazy.LastError.empty())ImGui::TextWrapped("Definition query failed: %s",lazy.LastError.c_str());else ImGui::TextDisabled("No loaded AI matches this search.");}
            if(!definition.empty())ImGui::TextDisabled("Selected: %s",state.SelectedLabel.c_str());
        } else {
            if(ImGui::BeginCombo("Loaded spawn",state.SelectedLabel.c_str())) {if(result.contains("Definitions")&&result["Definitions"].is_array()){const auto& definitionRows=result["Definitions"];ImGuiListClipper clipper;clipper.Begin(static_cast<int>(definitionMatches.Rows.size()));while(clipper.Step()){if(clipper.DisplayEnd>=static_cast<int>(definitionMatches.Rows.size())-4)LazyCatalog::More(kind==0?1:2);for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible){const auto index=definitionMatches.Rows[static_cast<size_t>(visible)];if(index>=definitionRows.size())continue;const auto& row=definitionRows[index];const auto key=row.value("Key",std::string{}),type=row.value("Type",std::string("AI")),title=row.value("Name",key),path=row.value("Class",std::string{});const auto display="["+type+"] "+title+" — "+key;ImGui::PushID(key.c_str());if(ImGui::Selectable(display.c_str(),key==definition)){definition=key;confirmed=false;}if(HelpyImGuiTheme::TooltipReady(std::string("spawn:")+key,ImGui::IsItemHovered()))PS::HelpyImGuiTheme::Tooltip("%s",path.c_str());ImGui::PopID();}}}if(definitionMatches.Rows.empty()){const auto& lazy=LazyCatalog::For(kind==0?1:2);if(lazy.Pending)ImGui::TextDisabled("Loading runtime definitions...");else if(!lazy.LastError.empty())ImGui::TextWrapped("Definition query failed: %s",lazy.LastError.c_str());else ImGui::TextDisabled("No loaded AI or resource matches this search.");}ImGui::EndCombo();}
        }
        if(ImGui::InputText("Spawn name override (optional)",name,sizeof(name)))confirmed=false;
        if(kind==0&&!npc && ImGui::Checkbox("Use name for boss title too",&boss))confirmed=false;
    }
    std::string selectedClass;bool selectedAvailable=custom,selectedModeAllowed=custom;std::string selectedReason;
    if(!custom&&result.contains("Definitions"))for(const auto& row:result["Definitions"])if(row.value("Key",std::string{})==definition) {
        if(row.value("NeedsInspection",false)) {
            ImGui::TextWrapped("Mounted content: load and inspect this class to verify its native type. This does not spawn it.");
            ImGui::BeginDisabled(DefinitionPending!=0||SpawnToolRequests::IsWaiting());
            if(HelpyImGuiTheme::SoftButton("Inspect selected definition")) {
                const auto id=++DefinitionSequence;
                if(SpawnToolRequests::TrySubmit({{"Action","InspectSpawnDefinition"},{"_Source","HelpyStandalone"},{"_RequestId",id},{"Class",row.at("Class")},{"Key",definition},{"ExpectedType",row.at("Type")}}))DefinitionPending=id;
            }ImGui::EndDisabled();
        }
        selectedClass=row.value("Class",std::string{});selectedAvailable=row.value("Available",true);
        selectedModeAllowed=npc?(permanent?row.value("PermanentAllowed",false):row.value("TemporaryAllowed",false)):selectedAvailable;
        selectedReason=npc?(permanent?row.value("PermanentReason",std::string{}):row.value("TemporaryReason",std::string{})):row.value("Reason",std::string{});break;
    }
    if(!DefinitionStatus.empty())ImGui::TextWrapped("%s",DefinitionStatus.c_str());
    if(!selectedModeAllowed&&!definition.empty()&&!selectedReason.empty())ImGui::TextWrapped("Unavailable: %s",selectedReason.c_str());
    if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx"))invalidateSharedPlacement();
    if(ImGui::Checkbox("Grid placement",&grid))invalidateSharedPlacement();
    if(grid) {
        if(ImGui::InputFloat("Grid X offset (cells)",&gridX))invalidateSharedPlacement();
        if(ImGui::InputFloat("Grid Y offset (cells)",&gridY))invalidateSharedPlacement();
        if(ImGui::InputFloat("Grid cell size (cm)",&gridSize))invalidateSharedPlacement();
    }else if(ImGui::InputFloat("Spawn radius (cm)",&radius))invalidateSharedPlacement();
    if(!npc&&ImGui::SliderInt("Count",&count,1,20))invalidateSharedPlacement();
    if(npc&&!permanent){if(ImGui::InputInt("NPC lifetime (seconds)",&npcDuration))confirmed=false;npcDuration=std::clamp(npcDuration,HelpyNodes::MinDuration,HelpyNodes::MaxDuration);}
    if(!npc&&ImGui::Checkbox("Override ghost mesh setting",&overrideGhost))invalidateSharedPlacement();
    if(!npc&&overrideGhost && ImGui::Checkbox("Ghost mesh overlay",&ghost))invalidateSharedPlacement();
    if(!npc&&ImGui::Checkbox("Append additional drops (AI / resources)",&overrideDrops))invalidateSharedPlacement();
    ExtraLootBay=!npc&&overrideDrops;
    if(overrideDrops) {
        ImGui::SeparatorText("Additional loot");
        const auto dropPicker=ItemGridPicker::RenderShared(result,resultRevision);
        std::snprintf(item,sizeof(item),"%s",ItemGridPicker::Bay.Selected);
        if(dropPicker.Changed)invalidateSharedPlacement();
        if(item[0]=='/') {
            ImGui::Text("Selected: %s",dropPicker.SelectedTitle.c_str());
            ImGui::TextDisabled("%s",item);
        }
        if(ui.advancedMode&&ImGui::CollapsingHeader("Advanced item path override")) {
            ImGui::InputText("Item asset path##additional-drop",item,sizeof(item));
            ImGui::TextDisabled("Use this only for a valid loaded item path that is not available in the current catalog.");
        }
        ImGui::InputInt("Minimum quantity",&dropMin);ImGui::InputInt("Maximum quantity",&dropMax);
        ImGui::SliderFloat("Drop chance (%)",&chance,0,100);
        const bool haveDropSelection=!dropPicker.SelectedPaths.empty()||item[0]=='/';
        ImGui::BeginDisabled(drops.size()>=16 || !haveDropSelection || dropMin<1 || dropMax<dropMin || dropMax>10000);
        if(HelpyImGuiTheme::SoftButton(dropPicker.SelectedCount>1?"Add selected items to additional loot":"Add selected item to additional loot")) {
            std::vector<std::string> paths=dropPicker.SelectedPaths;if(paths.empty()&&item[0]=='/')paths.push_back(item);
            for(const auto& path:paths){if(drops.size()>=16)break;drops.push_back({{"Item",path},{"Min",dropMin},{"Max",dropMax},{"ChancePercent",chance}});}
            invalidateSharedPlacement();
        }
        ImGui::EndDisabled();
        SameLineIfRoom(105.0f);ImGui::TextDisabled("%zu / 16 entries",drops.size());

        static uint64_t dropTitlesRevision=0;static std::vector<std::string> dropPaths,dropTitles;
        std::vector<std::string> currentDropPaths;currentDropPaths.reserve(drops.size());for(const auto& drop:drops)currentDropPaths.push_back(drop.value("Item",std::string{}));
        if(dropTitlesRevision!=resultRevision||dropPaths!=currentDropPaths) {dropTitlesRevision=resultRevision;dropPaths=std::move(currentDropPaths);dropTitles.clear();dropTitles.reserve(dropPaths.size());for(const auto& path:dropPaths)dropTitles.push_back(ItemGridPicker::Title(result,path));}
        if(!drops.empty() && ImGui::BeginTable("AdditionalDropList",4,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Item",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Quantity",ImGuiTableColumnFlags_WidthFixed,ui.X(90.0f));
            ImGui::TableSetupColumn("Chance",ImGuiTableColumnFlags_WidthFixed,ui.X(80.0f));
            ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,ui.X(70.0f));
            ImGui::TableHeadersRow();
            for(size_t i=0;i<drops.size();++i) {
                const auto path=drops[i].value("Item",std::string{});
                const auto& title=i<dropTitles.size()?dropTitles[i]:path;
                ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(title.c_str());
                if(HelpyImGuiTheme::TooltipReady(std::string("drop:")+path+":"+std::to_string(i),ImGui::IsItemHovered()))PS::HelpyImGuiTheme::Tooltip("%s",path.c_str());
                ImGui::TableSetColumnIndex(1);ImGui::Text("%d-%d",drops[i].value("Min",1),drops[i].value("Max",1));
                ImGui::TableSetColumnIndex(2);ImGui::Text("%.1f%%",drops[i].value("ChancePercent",100.0f));
                ImGui::TableSetColumnIndex(3);
                if(ImGui::SmallButton("Remove")){drops.erase(i);invalidateSharedPlacement();ImGui::PopID();break;}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if(custom&&ImGui::SliderFloat("Distance from player (metres)",&distance,3,50))invalidateSharedPlacement();
    if(!npc&&ImGui::InputFloat("Placement yaw (degrees)",&yaw))invalidateSharedPlacement();
    if(!npc&&ImGui::InputFloat("Height offset above ground (cm)",&height))invalidateSharedPlacement();
    ImGui::Checkbox(permanent?"I want to spawn and install these placements":"I want to create these functioning temporary actors",&confirmed);
    ImGui::BeginDisabled(!confirmed || player.empty() || (!custom && (definition.empty()||!selectedAvailable||!selectedModeAllowed)));
    if(HelpyImGuiTheme::SoftButton("Spawn near selected player")) {
        nlohmann::json request={{"Action","Spawn"},{"_Source","HelpyStandalone"},{"Player",player},{"Definition",custom?"":definition},
            {"Class",custom?std::string(ai):selectedClass},{"Name",name},{"Scale",scale},{"Permanent",permanent}};
        if(custom) {request["Boss"]=boss;request["Resource"]=resource;request["Yaw"]=yaw;request["Height"]=height;request["Count"]=count;request["Distance"]=distance;}
        else {
            request["NPC"]=npc;request["Resource"]=resource;request["Count"]=npc?1:count;request["Distance"]=5.0;
            request["PlacementMode"]=grid?"Grid":"Radius";
            if(grid){request["GridX"]=gridX;request["GridY"]=gridY;request["GridSize"]=gridSize;}else request["PlacementRadius"]=radius;
            if(npc)request["DurationSeconds"]=permanent?0:npcDuration;
            else {request["Yaw"]=yaw;request["Height"]=height;request["Boss"]=boss;if(kind==0&&Authoring::EnemyPower.load()!=-1)request["PowerLevel"]=Authoring::EnemyPower.load();
                if(overrideGhost)request["GhostMesh"]=ghost;if(overrideDrops){request["AppendAdditionalDrops"]=true;request["AdditionalDrops"]=drops;}}
        }
        SpawnToolRequests::Submit(std::move(request));confirmed=false;
    }
    ImGui::EndDisabled();
    if(HelpyImGuiTheme::SoftButton("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
    ImGui::TextWrapped("Exports use original placement, not wandering AI positions. Export/clear do not install a mod or change live actors. Resource classes must expose verified save exclusion; unsupported classes are refused.");
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    ImGui::PopID();
    };
    if(view==View::All||view==View::Ai)renderActorSection(0,ForcedActorMode==0?"NPCs":"AI / Enemies",ImGuiTreeNodeFlags_DefaultOpen);
    if(view==View::All||view==View::Resources)renderActorSection(1,"Resources",ImGuiTreeNodeFlags_DefaultOpen);
    if(view==View::All||view==View::Items||view==View::ItemLab) {
        if(view==View::All||view==View::Items) {
            const float width=ImGui::GetContentRegionAvail().x;
            // One placard column plus the narrow cart fits small desktop
            // windows; stacking both fixed-height panes hides delivery.
            const bool sideBySide=width>=ui.X(480.0f);
            const float height=std::max(ui.Y(350.0f),ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y);
            auto& cartWidthLogical=HelpyImGuiTheme::Current.itemCartWidth;
            const float cartWidth=std::clamp(ui.X(cartWidthLogical),ui.X(210),std::max(ui.X(210),width*.5f)),gap=ui.Y(8);
            const float cartHeight=ui.Y(210.0f);
            // On short/narrow windows, let the workspace scroll instead of
            // shrinking the browser below one usable placard row.
            const float browserHeight=sideBySide?height:std::max(ui.Y(320.0f),height-cartHeight-ImGui::GetStyle().ItemSpacing.y);
            ItemGridPicker::Result inventoryPicker;
            if(ImGui::BeginChild("##item-browser-pane",ImVec2(sideBySide?width-cartWidth-gap:0,browserHeight),false)){
                inventoryPicker=ItemGridPicker::RenderShared(result,resultRevision,true);
            }ImGui::EndChild();
            if(sideBySide){ImGui::SameLine(0,0);ItemGridPicker::ResizeDivider("item-cart-width",false,cartWidthLogical,210,500,-1.0f/std::max(.01f,ui.Scale()));ImGui::SameLine(0,0);}
            RenderCartDock(ImVec2(sideBySide?cartWidth:0,sideBySide?height:cartHeight),recipient);
        }
        if(view==View::All||view==View::ItemLab)ItemAuthoringPanel::Render(result,resultRevision,player);
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    }
    if((view==View::All||view==View::Buildings) && HelpyImGuiTheme::SectionHeader("Buildings",HelpyImGuiTheme::Icon::Building,ImGuiTreeNodeFlags_DefaultOpen)) {
        if(HelpyImGuiTheme::SoftButton("Load saved building catalog"))SpawnToolRequests::Submit({{"Action","Catalog"}});
        SameLineIfRoom(230.0f);if(HelpyImGuiTheme::SoftButton("Rebuild building safety catalog"))SpawnToolRequests::Submit({{"Action","RebuildCatalogs"}});
        SearchOnEnter("Search loaded buildings","name or BuildingPieceData path",buildingFilter,sizeof(buildingFilter),buildingQuery);
        CacheBuildings(result,resultRevision,buildingQuery,buildingMatches);
        static uint64_t buildingMetaRevision=0,buildingCountsRevision=0;static std::string buildingMetaPath,buildingLabel,selectedBuildingReason,selectedTemporaryReason;static bool selectedBuildingSafe=false,selectedTemporarySafe=false;static size_t verifiedBuildings=0,blockedBuildings=0;
        if(buildingCountsRevision!=resultRevision) {buildingCountsRevision=resultRevision;verifiedBuildings=blockedBuildings=0;if(result.contains("Buildings"))for(const auto& row:result["Buildings"])row.value("Safe",false)?++verifiedBuildings:++blockedBuildings;}
        if(buildingMetaRevision!=resultRevision||buildingMetaPath!=selectedBuilding) {
            buildingMetaRevision=resultRevision;buildingMetaPath=selectedBuilding;buildingLabel=selectedBuilding;selectedBuildingSafe=selectedTemporarySafe=false;selectedBuildingReason.clear();selectedTemporaryReason.clear();
            if(result.contains("Buildings"))for(const auto& row:result["Buildings"])if(row.value("Path",std::string{})==selectedBuilding){buildingLabel=row.value("Name",buildingLabel);selectedBuildingSafe=row.value("Safe",false);selectedBuildingReason=row.value("SafetyReason",std::string{});selectedTemporarySafe=row.value("TemporarySafe",false);selectedTemporaryReason=row.value("TemporarySafetyReason",std::string("Rebuild the building catalogue to check temporary previews."));break;}
        }
        auto label=buildingLabel;if(verifiedBuildings+blockedBuildings)ImGui::TextDisabled("Safety scope: %zu verified / %zu blocked",verifiedBuildings,blockedBuildings);
        if(ImGui::BeginCombo("Loaded building",label.empty()?"Select building":label.c_str())) {
            if(result.contains("Buildings")&&result["Buildings"].is_array()) {
                const auto& buildingRows=result["Buildings"];ImGuiListClipper clipper;clipper.Begin(static_cast<int>(buildingMatches.Rows.size()));
                while(clipper.Step())for(int visible=clipper.DisplayStart;visible<clipper.DisplayEnd;++visible) {
                    const auto index=buildingMatches.Rows[static_cast<size_t>(visible)];if(index>=buildingRows.size())continue;const auto& row=buildingRows[index];
                    const auto path=row.value("Path",std::string{}),title=row.value("Name",path);const bool safe=row.value("Safe",false);ImGui::PushID(path.c_str());ImGui::BeginDisabled(!safe);
                    if(ImGui::Selectable(title.c_str(),path==selectedBuilding)){std::snprintf(selectedBuilding,sizeof(selectedBuilding),"%s",path.c_str());buildingConfirmed=false;}
                    ImGui::EndDisabled();if(HelpyImGuiTheme::TooltipReady(std::string("building:")+path,ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))PS::HelpyImGuiTheme::Tooltip("%s\nSafety: %s",path.c_str(),row.value("SafetyReason",safe?std::string("Verified"):std::string("Blocked")).c_str());ImGui::PopID();
                }
            }
            if(buildingMatches.Rows.empty())ImGui::TextDisabled("No loaded BuildingPieceData matches this search.");
            ImGui::EndCombo();
        }
        if(ImGui::SliderFloat("Instance scale",&scale,0.1f,10.0f,"%.2fx")){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::Checkbox("Save placement",&buildingPermanent))buildingConfirmed=false;
        if(ImGui::Checkbox("Place at world coordinates",&buildingAtCoordinates))buildingConfirmed=false;
        if(buildingAtCoordinates){
            if(ImGui::InputDouble("World X (cm)",&buildingX,0,0,"%.1f"))buildingConfirmed=false;
            if(ImGui::InputDouble("World Y (cm)",&buildingY,0,0,"%.1f"))buildingConfirmed=false;
            ImGui::TextDisabled("Ground-snapped at X/Y; use Height offset for vertical placement. Area must be loaded.");
        }else if(ImGui::SliderFloat("Distance from player (metres)",&distance,3,50)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::InputFloat("Placement yaw (degrees)",&yaw)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::InputFloat("Height offset above ground (cm)",&height)){buildingConfirmed=false;actors[0].Confirmed=actors[1].Confirmed=false;}
        if(ImGui::Checkbox("Allow deconstruction",&allowDeconstruction))buildingConfirmed=false;
        ImGui::Text("Availability");ImGui::SameLine();
        if(ImGui::RadioButton("Any##building-time",&buildingTime,0))buildingConfirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Day##building-time",&buildingTime,1))buildingConfirmed=false;ImGui::SameLine();
        if(ImGui::RadioButton("Night##building-time",&buildingTime,2))buildingConfirmed=false;
        const bool placementSafe=selectedBuildingSafe&&(buildingPermanent||selectedTemporarySafe);
        if(!buildingPermanent)ImGui::TextDisabled("Temporary previews require native save exclusion.");
        if(selectedBuilding[0]=='/'&&!placementSafe)ImGui::TextWrapped("%s",!selectedBuildingSafe?selectedBuildingReason.c_str():selectedTemporaryReason.c_str());
        ImGui::Checkbox(buildingPermanent?"Confirm saved building placement":"Confirm temporary preview",&buildingConfirmed);
        ImGui::BeginDisabled(!buildingConfirmed || player.empty() || selectedBuilding[0]!='/' || !placementSafe || (buildingAtCoordinates&&(!std::isfinite(buildingX)||!std::isfinite(buildingY))));
        if(HelpyImGuiTheme::SoftButton(buildingPermanent?"Place & save building":buildingAtCoordinates?"Preview at coordinates":"Preview near player")) {
            nlohmann::json request={{"Action","SpawnBuilding"},{"Player",player},{"Building",selectedBuilding},
                {"Yaw",yaw},{"Height",height},{"Scale",scale},{"Count",1},{"Distance",distance},
                {"AllowDeconstruction",allowDeconstruction},{"Permanent",buildingPermanent},{"TimeOfDay",buildingTime==1?"Day":buildingTime==2?"Night":"Any"}};
            if(buildingAtCoordinates)request["TargetLocation"]={{"X",buildingX},{"Y",buildingY}};
            SpawnToolRequests::Submit(std::move(request));buildingConfirmed=false;
        }
        ImGui::EndDisabled();
        if(HelpyImGuiTheme::SoftButton("Export authored placements as /spawns JSON"))SpawnToolRequests::Submit({{"Action","Export"}});
        ImGui::TextWrapped("The catalog keeps loaded BuildingPieceData for inspection. Placement rechecks native data binding and actor defaults in the current authoritative world. Use World map placement and Permanent for pieces that cannot be excluded from native saves.");
        ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
    }

}

inline void Render(){Render(View::All);}

inline void RenderSessionCleanup() {
    static std::string player;RefreshSnapshot(-1);const auto& result=UiSnapshot;
    ImGui::TextWrapped("Clean only actors and staged placements created by the current Mod Authoring session. Installed /spawns content and saves are not changed.");
    if(HelpyImGuiTheme::SoftButton("Refresh authoritative players"))SpawnToolRequests::Submit({{"Action","QuickPlayers"}});
    auto label=std::string("Select player");
    if(result.contains("Players"))for(const auto& row:result["Players"])if(row.value("Path",std::string{})==player)label=RuneSchemaProfile::Name(row);
    if(ImGui::BeginCombo("Player##cleanup",label.c_str())) {
        if(result.contains("Players"))for(const auto& row:result["Players"]) {
            const auto path=row.value("Path",std::string{});ImGui::PushID(path.c_str());
            if(ImGui::Selectable(RuneSchemaProfile::Name(row).c_str(),player==path))player=path;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(player.empty());
    if(HelpyImGuiTheme::SoftButton("Dismiss RSMK temporary NPCs"))SpawnToolRequests::Submit({{"Action","DismissHelpyNpcs"},{"_Source","HelpyStandalone"},{"Player",player}});
    ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Clean up live tool-created actors"))SpawnToolRequests::Submit({{"Action","Cleanup"},{"Player",player}});
    ImGui::EndDisabled();
    if(HelpyImGuiTheme::SoftButton("Clear staged export placements"))SpawnToolRequests::Submit({{"Action","ClearPlacements"}});
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
}
}
