#include "Loader/PlayerArchetype.h"
#include <cassert>
int main() {
    using nlohmann::json;
    const json base={{"PlayerName","Test"},{"Archetype",{{"Name","Custom Mage"},{"Icon","/Game/Icons/Mage.Mage"},{"Scale",0.9}}},
        {"Nameplate",{{"ActivityTimeoutSeconds",5},{"States",{{"Attack",{{"Icon","/Game/Icons/Attack.Attack"}}}}}}}};
    auto normalized=DragonWilds::NormalizePlayerArchetype(base);
    assert(normalized["Nameplate"]["Mode"]=="Icon");
    assert(normalized["Nameplate"]["Icon"]==base["Archetype"]["Icon"]);
    assert(normalized["Nameplate"]["States"]==base["Nameplate"]["States"]);
    assert(normalized["Nameplate"]["ActivityTimeoutSeconds"]==5);
    assert(DragonWilds::NormalizePlayerArchetype(json{{"PlayerName","Test"}})==json({{"PlayerName","Test"}}));
    for(int i=0;i<7;++i) {
        auto invalid=base;
        if(i==0)invalid["Archetype"]["Name"]="";
        if(i==1)invalid["Archetype"]["Icon"]="C:\\image.png";
        if(i==2)invalid["Archetype"]["Scale"]=0;
        if(i==3)invalid["Nameplate"]["Mode"]="Hidden";
        if(i==4)invalid["Archetype"]["Unknown"]=true;
        if(i==5)invalid["Archetype"]["Name"]="Bad\nName";
        if(i==6)invalid["Nameplate"]["Scale"]=2;
        bool rejected=false;try{DragonWilds::NormalizePlayerArchetype(invalid);}catch(...){rejected=true;}
        assert(rejected);
    }
}
