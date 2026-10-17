#include "Core/RecipeCatalog.h"
#include <stdexcept>
#include <cstdio>
void Check(bool value){if(!value)throw std::runtime_error("Recipe catalogue check failed");}
int main(){
    using namespace PS::RecipeCatalog;Index index;
    for(int i=0;i<70000;++i){char name[80];std::snprintf(name,sizeof(name),"/Game/Recipes/R_%06d.R_%06d",i,i);Add(index,{name,name,{},{},"Loaded recipe",true});}
    const auto first=Page(index,"",0),late=Page(index,"",65536);
    Check(first["Recipes"].size()==128&&first["Total"]==70000&&first["HasMore"]==true);
    Check(late["Recipes"].size()==128&&late["Offset"]==65536&&late["Total"]==70000);
    Add(index,{"/Mod/Recipes/Dye.Dye","Red Dye","DyeMod","DyeMod:red_dye","RuneSchema definition",true});
    Add(index,{"/Mod/Recipes/Dye.Dye","Other spelling",{},{},"Cooked candidate",true});
    Check(index.size()==70001);const auto match=Page(index,"dyemod:red_dye",0);Check(match["Total"]==1&&match["Recipes"][0]["Owner"]=="DyeMod");
    Add(index,{"/Mod/Recipes/Dye.Dye","Red Dye","DyeMod","DyeMod:red_dye","RuneSchema definition",false});
    Check(Page(index,"dyemod:red_dye",0)["Recipes"][0]["Available"]==false);
    Add(index,{{},"Broken definition","Mod","Mod:broken","RuneSchema definition",false});
    Check(Page(index,"mod:broken",0)["Recipes"][0]["Path"]=="");
    const auto end=Page(index,"R_069999",0);Check(end["Total"]==1&&!end["HasMore"].get<bool>());
    bool rejected=false;try{Page(index,"",MaxEntries+1);}catch(const std::exception&){rejected=true;}Check(rejected);
}
