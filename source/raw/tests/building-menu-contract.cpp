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
    for(const auto* proof:{"BuildableActor defaults are not initialized","BuildableActor has no native scene root","Selected building differs from the validated data-binding source","AdjustIfPossibleButDontSpawnIfColliding","VerifyToolBuildingInstance(actor,buildingObject,prototype,world,true)"})
        Check(tools.find(proof)!=std::string::npos,"First-placement replacement lost native default, collision or post-construction binding checks");
    Check(tools.find("LiveActor.Get()")!=std::string::npos&&tools.find("SavedActivationIncomplete")!=std::string::npos,"Saved placement must not imply a confirmed live building");
    const auto ui=Read(source/"helpy/source/include/Generator/MapPanel.h");
    for(const auto* fragment:{"BeginPopupModal(\"Building menu\"","BuildingMenu::Matches(","BuildingMenu::BlockedReason(","ItemIconCache::Draw(","Retry ground","Building placement confirmed.","Do not place a duplicate."})
        Check(ui.find(fragment)!=std::string::npos,"Building selection or explicit outcome feedback is missing");
    Check(ui.find("Data.Operation==1){Data.Permanent=true;")!=std::string::npos,"Native buildings without save-exclusion must use explicit persistent placement, not an unsupported temporary default");
    Check(tools.find("[HELPY][BUILDING][FAILED]")!=std::string::npos&&ui.find("PlacementResult,\"Error\"")!=std::string::npos,"Building failures must retain the actionable runtime reason");
    const auto players=Read(source/"raw/src/Loader/Spawn/PlayerRules.cpp");
    Check(players.find("PlayerCarryWeight::Verified(actual,requested)")!=std::string::npos&&players.find("CARRY-OVERRIDE")!=std::string::npos,"Capacity claims require native readback and conflict diagnostics");
}
