#include "Core/MapFilters.h"
#include <limits>
#include <fstream>
#include <iostream>
using namespace PS::MapFilters;
static void Check(bool ok){if(!ok)throw std::runtime_error("Map filter contract failed");}
int main(int argc,char** argv){
    if(argc==2){std::ifstream file(argv[1],std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(file),{}};const auto real=Parse(bytes);std::cout<<Json{{"Categories",real.Categories.size()},{"Markers",real.Markers.size()},{"Built",real.Built},{"Credit",Credit}}.dump();return 0;}
    const Json input={{"source","RuneScape: Dragonwilds Wiki"},{"licence","CC BY-NC-SA 3.0"},{"built","2026-10-16"},{"groups",Json::array({
        {{"id","mining"},{"label","Ores and rocks"},{"categories",Json::array({{{"name","Iron"},{"points",{{200000,40000},{200000,40000},{201000,41000,"Iron node","Reference, not current availability"}}}},{{"name","Copper"},{"points",{{202000,42000}}}}})}},
        {{"id","trees"},{"label","Trees"},{"categories",Json::array({{{"name","Oak"},{"points",{{203000,43000}}}}})}}
    })}};
    const auto data=Parse(input.dump());Check(data.Categories.size()==3&&data.Markers.size()==4&&data.Categories[0].Count==2);
    Check(data.Markers[0].X==200000&&data.Markers[0].Y==40000&&data.Markers[0].Name=="Iron");
    Check(Matches(data.Markers[1].Search,"IRON ores")&&!Matches(data.Markers[1].Search,"iron tree"));
    Selection selection;Check(selection.Shows(data.Categories[0]));selection.Hidden.insert("mining:Iron");selection.Resources=true;selection.Players=false;
    const auto retained=Selection::From(selection.ToJson());Check(!retained.Shows(data.Categories[0])&&retained.Shows(data.Categories[1])&&retained.Resources&&!retained.Players);
    selection.Chests=true;Check(Selection::From(selection.ToJson()).Chests&&!Selection::From(Json::object()).Chests);
    selection.ObserveNearbyResources=false;selection.ResourceRadiusMetres=350;const auto nearby=Selection::From(selection.ToJson());Check(!nearby.ObserveNearbyResources&&nearby.ResourceRadiusMetres==350);
    Check(Selection::From({{"ResourceRadiusMetres",-1}}).ResourceRadiusMetres==25&&Selection::From({{"ResourceRadiusMetres",1e9}}).ResourceRadiusMetres==2000);
    Check(Selection::From({{"ResourceRadiusMetres",std::numeric_limits<double>::quiet_NaN()}}).ResourceRadiusMetres==200);
    const auto reject=[&](Json invalid){try{(void)Parse(invalid.dump());}catch(...){return true;}return false;};
    auto invalid=input;invalid["licence"]="MIT";Check(reject(invalid));
    invalid=input;invalid["groups"][0]["categories"][0]["points"][0][0]=1e99;Check(reject(invalid));
    invalid=input;invalid["groups"][0]["categories"][0]["points"][0][0]="200000";Check(reject(invalid));
    invalid=input;invalid["groups"].push_back(input["groups"][0]);Check(reject(invalid));
    invalid=input;invalid["groups"]=Json::array();Check(reject(invalid));
    Check(ConfirmedBuild("++dominion+live-CL-251392")=="CL-251392");
    for(const auto text:{"","1.0.1","News: CL-251393 available","RuneSchema 0.8.0.0","UE4SS 4.0","++dominion+live-CL-"})Check(ConfirmedBuild(text).empty());
    Check(!NeedsUpdate(false,"CL-1","CL-2",true)&&!NeedsUpdate(true,"","CL-2",true)&&!NeedsUpdate(true,"CL-1","",true));
    Check(!NeedsUpdate(true,"CL-1","CL-1",true)&&!NeedsUpdate(true,"CL-1","CL-2",false)&&NeedsUpdate(true,"CL-1","CL-2",true));
}
