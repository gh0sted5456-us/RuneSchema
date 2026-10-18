#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("Missing station contract input");return {std::istreambuf_iterator<char>(f),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==3,"Expected station backend and recipe UI");const auto native=Read(argv[1]),ui=Read(argv[2]);
    const auto arrays=native.substr(native.find("std::vector<std::string> HelpyRecipeArrays"),native.find("nlohmann::json HelpyStationPresentation")-native.find("std::vector<std::string> HelpyRecipeArrays"));
    Check(arrays.contains("StationBuildingPieceData")&&arrays.contains("/Script/Dominion.BuildingPieceData")&&arrays.contains("if(!buildingBinding)return fields"),"Recipe arrays alone must not advertise item/reward/armor tables as stations");
    Check(native.contains("bIsVendor")&&native.contains("PhysicalStation"),"Vendor rows and unlinked menus must not masquerade as physical stations");
    Check(native.contains("HelpyResolve(path)")&&native.contains("loadsRemaining=64"),"Resolve the bound native station asset with bounded work, not the recipe output's icon");
    Check(native.contains("ContainsRecipeArray(values->Num(),recipePath,remaining")&&native.contains("remaining=262144"),"Native station membership needs an exact bounded reader independent of the generic 64-item inspector");
    const auto placements=native.substr(native.find("nlohmann::json HelpyRecipePlacements"),native.find("nlohmann::json HelpyItemRelations")-native.find("nlohmann::json HelpyRecipePlacements"));
    Check(!placements.contains("values.dump()")&&!placements.contains("MatchingPlacements"),"Do not infer membership from JSON substring matching or a truncated serialized recipe collection");
    Check(ui.contains("!station.value(\"PhysicalStation\",true)")&&ui.contains("ItemPresentation::StationIcon(station)"),"Picker must show station bindings and request native station icons");
}
