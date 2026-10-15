#include "Core/RecipeWorkspace.h"
#include <cassert>
using namespace PS::RecipeWorkspace;
template<class F> bool Rejects(F action){try{action();}catch(const std::exception&){return true;}return false;}
int main(){
    Json input=Json::array(),output=Json::array();
    AddItem(input,{{"Path","/Game/Items/Wood.Wood"}});AddItem(input,{{"Path","/Game/Items/Wood.Wood"}});
    AddItem(output,{{"Path","/Game/Items/Plank.Plank"}});AddItem(output,{{"Path","/Game/Items/Bark.Bark"}});
    assert(input.size()==1&&input[0]["Count"]==2&&output.size()==2);
    Json options={{"Ingredients",input},{"Outputs",output},{"MinProcessingTime",2.5},{"MaxProcessingTime",4.0}};
    const auto props=Properties(options);assert(props["ItemsCreated"].size()==2&&props["MinProcessingTime"]==2.5);
    assert(!props.contains("PersistenceID")&&!props.contains("InternalName"));
    auto cooked=options;cooked["Ingredients"][0]["NativeExtra"]=7;
    assert(Properties(cooked)["ItemsConsumed"][0]["NativeExtra"]==7);
    auto bad=options;bad["MaxProcessingTime"]=1;assert(Rejects([&]{Properties(bad);}));
    bad=options;bad.erase("MinProcessingTime");assert(Rejects([&]{Properties(bad);}));
    for(const auto& value:{Json(true),Json(-1),Json(86401),Json("5")}){bad=options;bad["MaxProcessingTime"]=value;assert(Rejects([&]{Properties(bad);}));}
    for(const auto& count:{Json(0),Json(10001),Json(1.5),Json(true),Json("2")}){bad=options;bad["Outputs"][0]["Count"]=count;assert(Rejects([&]{Properties(bad);}));}
    bad=options;bad["Outputs"].push_back(bad["Outputs"][0]);assert(Rejects([&]{Properties(bad);}));
    assert(Rejects([&]{AddItem(input,{{"Path","not-an-asset"}});}));
    assert(Properties({{"Ingredients",input},{"Count",3}},"/Game/Items/Plank.Plank")["ItemsCreated"][0]["Count"]==3);
    assert(Properties({{"Ingredients",Json::array()},{"Outputs",output}})["ItemsConsumed"].empty());
    assert(Rejects([&]{Properties({{"Ingredients",input},{"Outputs",Json::array()}});}));
    AddItem(output,{{"Path","/Game/Items/Plank.Plank"},{"Count",5}});assert(output[0]["Count"]==6);
    assert(Rejects([&]{AddItem(output,{{"Path","/Game/Items/Plank.Plank"},{"Count",0}});}));
    ReplaceItem(output,0,"/Game/Items/Board.Board");assert(output[0]["Count"]==6);
    assert(Rejects([&]{ReplaceItem(output,0,"/Game/Items/Bark.Bark");}));
    assert(References(Json::array({{{"Collection",Json::array({"/Game/Recipes/A.A"})}}}),"/Game/Recipes/A.A"));
    assert(!References(Json::array({"/Game/Recipes/A.A_extra"}),"/Game/Recipes/A.A"));
    Json station={{"Id","dye"},{"Path","/Game/Mods/Dye/DT_Stations.DT_Stations"},{"Name","Dye Station"},{"Row","Dyer"}};
    assert(StationMatches(station,"DYE"));assert(StationMatches(station,"dyer","/Game/Mods/Dye"));
    assert(!StationMatches(station,"smith"));assert(!StationMatches(station,"","/Fishing"));
    assert(LoadedStation(Json::array({station}))==station);assert(LoadedStation(Json::array()).empty());
    const Json building={{"DisplayName","Native Furnace"},{"DisplayIcon","/Game/UI/Furnace.Furnace"}};
    assert(StationPresentation(Json::object(),building)["Icon"]==building["DisplayIcon"]);
    assert(StationPresentation({{"DisplayNameOverride","Custom Furnace"}},building)["Name"]=="Custom Furnace");
    assert(StationPresentation({{"DisplayIcon","/Dye/UI/Station.Station"}},building)["Icon"]=="/Dye/UI/Station.Station");
    assert(StationPresentation({{"DisplayIcon","None"}},building)["Icon"]==building["DisplayIcon"]);
    station["BuildingPiece"]="/Dye/Buildings/Station.Station";assert(StationSource(station)=="/Dye");
    const Json categories=Json::array({{{"Label","Weapons"},{"Collection",Json::array({"/Game/Recipes/A.A"})}},{{"Label","Armor"},{"Collection",Json::array({"/Game/Recipes/A.A"})}}});
    auto placements=MatchingPlacements(station,categories,"/Game/Recipes/A.A");assert(placements.size()==2&&placements[0]["Category"]=="Weapons");
    assert(MatchingPlacements(station,Json::array({{{"Label","/Game/Recipes/A.A"},{"Collection",Json::array()}}}),"/Game/Recipes/A.A").empty());
    station["Array"]="Recipes";assert(MatchingPlacements(station,Json::array({"/Game/Recipes/A.A"}),"/Game/Recipes/A.A").size()==1);
}
