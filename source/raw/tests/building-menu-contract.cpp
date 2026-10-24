#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& path){std::ifstream input(path);if(!input)throw std::runtime_error("Building menu contract input missing");return {std::istreambuf_iterator<char>(input),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==2,"Expected source root");const std::filesystem::path source(argv[1]);
    const auto runtime=Read(source/"raw/src/Generator/HelpyMapRuntime.cpp");
    for(const auto* field:{"TEXT(\"Pages\")","TEXT(\"Collection\")","TEXT(\"Label\")","TEXT(\"DisplayIcon\")","\"Categories\"","\"CategoryId\"","\"Group\""})
        Check(runtime.find(field)!=std::string::npos,"Native menu label/icon/hierarchy contract regressed");
    Check(runtime.find("rows.size()>=32")!=std::string::npos&&runtime.find("seen.size()>4096")!=std::string::npos,"Menu metadata work must stay bounded");
    const auto tools=Read(source/"raw/src/Loader/SpawnTools.inl");
    Check(tools.find("VerifyToolBuildingCandidate(buildingObject,prototype,world,!permanent)")!=std::string::npos,"Menu UI must not bypass native building safety");
    Check(tools.find("No native non-transient instance")==std::string::npos,"First placement still requires a pre-existing world copy");
    for(const auto* proof:{"BuildableActor defaults are not initialized","BuildableActor has no native or Blueprint scene root","Constructed building has no native scene root","SimpleConstructionScript","ComponentTemplate","Selected building differs from the validated data-binding source","AdjustIfPossibleButDontSpawnIfColliding","VerifyToolBuildingInstance(actor,buildingObject,prototype,world,true)"})
        Check(tools.find(proof)!=std::string::npos,"First-placement replacement lost native default, collision or post-construction binding checks");
    Check(tools.find("LiveActor.Get()")!=std::string::npos&&tools.find("SavedActivationIncomplete")!=std::string::npos,"Saved placement must not imply a confirmed live building");
    Check(tools.find("PS::SpawnAuthoring::PositionMatches(")!=std::string::npos&&tools.find("Building actor did not retain the requested position")!=std::string::npos,"Data binding alone must not confirm a building moved to world origin");
    for(const auto* field:{"\"RequestedLocation\"","\"ObservedLocation\"","[HELPY][BUILDING][RESULT]","prototype.Location=position;"})Check(tools.find(field)!=std::string::npos,"Building position evidence or temporary placement target missing");
    const auto ui=Read(source/"helpy/source/include/Generator/MapPanel.h");
    const auto panel=Read(source/"helpy/source/include/Generator/SpawnToolsPanel.h");
    Check(panel.find("Place at world coordinates")!=std::string::npos&&panel.find("request[\"TargetLocation\"]={{\"X\",buildingX},{\"Y\",buildingY}}")!=std::string::npos,"Building coordinate controls must use the validated native map-placement path");
    Check(panel.find("std::isfinite(buildingX)")!=std::string::npos&&tools.find("if(mapPlacement)ground=ToolMapGround(world,*request,pawn)")!=std::string::npos,"Coordinate placement must retain finite-number, world-bound and loaded-ground checks");
    for(const auto* fragment:{"BeginPopupModal(\"Building menu\"","BuildingMenu::Matches(","BuildingMenu::BlockedReason(","ItemIconCache::Draw(","Retry ground","Building placement confirmed.","Do not place a duplicate."})
        Check(ui.find(fragment)!=std::string::npos,"Building selection or explicit outcome feedback is missing");
    Check(ui.find("Data.Operation==1){Data.Permanent=true;")!=std::string::npos,"Native buildings without save-exclusion must use explicit persistent placement, not an unsupported temporary default");
    Check(tools.find("[HELPY][BUILDING][FAILED]")!=std::string::npos&&ui.find("PlacementResult,\"Error\"")!=std::string::npos,"Building failures must retain the actionable runtime reason");
    const auto importer=Read(source/"raw/src/Loader/DragonWildsBuildingModLoader.cpp");
    Check(importer.find("if(plan){*plan=std::move(placements);return;}")!=std::string::npos&&importer.find("if(plan)throw;")!=std::string::npos,"Tool imports must plan without live registration and reject malformed pieces");
    Check(importer.find("missing or duplicate piece IDs")!=std::string::npos&&tools.find("checkpoint+plan.size()>65536")!=std::string::npos,"Prefab identity and definition limits missing");
    Check(tools.find("PlanBuildingImport(document)")<tools.find("file=std::make_unique<PS::Authoring::StagedFile>(\"spawns\",stem,plan)"),"Prefab must validate before saving");
    Check(tools.find("Prefab extends outside the native world bounds")!=std::string::npos&&tools.find("m_spawns.resize(checkpoint)")!=std::string::npos,"Prefab bounds and rollback checks missing");
    Check(tools.find("+ground.Z()-lowest")!=std::string::npos,"Ground placement must preserve relative height without burying half the prefab");
    Check(tools.find("VerifyToolBuildingCandidate(object,prototype,catalogWorld,false)")!=std::string::npos&&tools.find("TemporarySafetyReason")!=std::string::npos&&panel.find("\"Permanent\",buildingPermanent")!=std::string::npos,"Saved placements must not inherit temporary-preview restrictions");
    Check(ui.find("Import prefab...")!=std::string::npos&&ui.find("Place & save prefab")!=std::string::npos,"Prefab map placement controls missing");
    const auto players=Read(source/"raw/src/Loader/Spawn/PlayerRules.cpp");
    Check(players.find("PlayerCarryWeight::Verified(actual,requested)")!=std::string::npos&&players.find("CARRY-OVERRIDE")!=std::string::npos,"Capacity claims require native readback and conflict diagnostics");
    const auto accounts=Read(source/"raw/include/SDK/Helper/PlayerStatus.h"),steam=Read(source/"raw/include/Runtime/SteamBuild.h");
    Check(accounts.find("if(local){const auto steam=SteamBuild::PersonaName();")!=std::string::npos&&runtime.find("AccountName(state,isLocal)")!=std::string::npos,"A remote player must never inherit the local Steam account");
    Check(accounts.find("ControllerAccountName(UObject* controller)")!=std::string::npos&&tools.find("ControllerAccountName(target)")!=std::string::npos&&tools.find("ControllerAccountName(controller)")!=std::string::npos,"Map and player rosters must use the same local-aware account provider");
    for(const auto* field:{"SteamAPI_SteamFriends_v017","SteamAPI_ISteamFriends_GetPersonaName","GetModuleHandleW","strnlen_s(name,129)","Storefront::IsDedicatedServer()","EpicLauncher"})Check(steam.find(field)!=std::string::npos,"Steam persona lookup lost its runtime or bounds gate");
    for(const auto* unsafe:{"SteamAPI_Init","SteamAPI_Shutdown","LoadLibrary","GetProcAddress(module,\"SteamAPI_GetHSteamUser"})Check(steam.find(unsafe)==std::string::npos,"Persona lookup must reuse only initialized flat Steamworks exports");
}
