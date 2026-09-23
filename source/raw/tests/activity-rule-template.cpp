#include "Generator/ActivityRuleTemplate.h"
#include <cassert>
using nlohmann::json;
int main() {
    using namespace PS::InspectionTools;
    const json event={{"function","/Script/Dominion.PlayerMagicComponent:Multicast_SendPayloadForSpellCasting"},
        {"parameters",{{"SpellNetId",{{"NetId",27}}},{"Data",{{"omitted","unsupported"}}}}}};
    const auto suggestions=ActivityParameterSuggestions(event["parameters"]);
    assert(suggestions.size()==1);
    const auto rule=BuildActivityRuleTemplate(event,suggestions,"Fishing","/Fishing/Icons/Test.Test",5.0,"TestPlayer");
    const auto& nameplate=rule[0]["Nameplate"];
    assert(nameplate["Mode"]=="Hidden" && nameplate["Server"]=="No");
    assert(DragonWilds::MatchActivityEvent(nameplate["Events"],event["function"],event["parameters"])=="Fishing");
    for(int i=0;i<7;++i) {
        auto sample=event;
        if(i==0)sample.erase("parameters");
        if(i==1)sample["function"]="/Game/Unsafe:Function";
        bool rejected=false;
        try {BuildActivityRuleTemplate(sample,suggestions,i==2?"Dead":"Fishing",
            i==3?"bad icon":"/Fishing/Icons/Test.Test",i==4?0.0:i==5?3601.0:5.0,i==6?"":"Player");}
        catch(...){rejected=true;}
        assert(rejected);
    }
    assert(ActivityParameterSuggestions({{"omitted","unsupported"}}).empty());
    assert(ActivityParameterSuggestions({{"$truncated",true}}).empty());
    auto changed=suggestions;changed[0]["Equals"]=30;
    const auto other=BuildActivityRuleTemplate(event,changed,"Magic","/Game/Icons/Icon.Icon",5,"Player");
    assert(DragonWilds::MatchActivityEvent(other[0]["Nameplate"]["Events"],event["function"],event["parameters"]).empty());
}
