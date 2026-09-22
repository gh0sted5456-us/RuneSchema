#include "Generator/TraceProfile.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using nlohmann::json;
template<class F> void rejects(F operation){bool caught=false;try{operation();}catch(const std::exception&){caught=true;}assert(caught);}
int main(int argc,char** argv){
    using PS::InspectionTools::ValidateTraceProfile;
    auto profile=ValidateTraceProfile({{"Version",1},{"Name","My trace"},{"Trace",json::object()}});
    assert(profile["Trace"]["Seconds"]==30 && profile["Trace"]["SuppressTicks"]==true);
    assert(profile["Export"]["Jsonc"]==false);
    assert(ValidateTraceProfile(json::parse(profile.dump()))==profile);
    auto multi=profile;multi["Trace"]["IncludeAny"]={"Inventory","Harvest"};multi["Trace"]["ExcludeAny"]={"GetAttributes"};
    assert(ValidateTraceProfile(multi)["Trace"]["IncludeAny"].size()==2);
    PS::PlayerTrace::NameFilters names(multi["Trace"]);
    assert(names.MatchesLower("/script/game:inventoryadded"));
    assert(names.MatchesLower("/script/game:harvestcomplete"));
    assert(!names.MatchesLower("/script/game:harvestgetattributes"));
    assert(!names.MatchesLower("/script/game:updatewetness"));
    const PS::PlayerTrace::NameFilters emptyFilters;
    assert(emptyFilters.MatchesLower("/script/game:anything"));
    const PS::PlayerTrace::NameFilters excludedOnly({{"ExcludeAny",{"GetAttributesComponent"}}});
    assert(!excludedOnly.MatchesLower("/script/dominion.gameplayattributeeffectinterface:getattributescomponent"));
    assert(excludedOnly.MatchesLower("/script/game:harvestcomplete"));
    const auto lines=PS::PlayerTrace::FilterLines(" Inventory\r\n\n Harvest ");
    assert(lines==json::array({"Inventory","Harvest"}));
    assert(PS::PlayerTrace::FilterLines(PS::PlayerTrace::FilterText(lines))==lines);
    auto invalidTerms=multi;invalidTerms["Trace"]["IncludeAny"]={""};rejects([&]{ValidateTraceProfile(invalidTerms);});
    invalidTerms=multi;invalidTerms["Trace"]["ExcludeAny"]={std::string("foo\0bar",7)};rejects([&]{ValidateTraceProfile(invalidTerms);});
    invalidTerms=multi;invalidTerms["Trace"]["IncludeAny"]=json::array();for(int i=0;i<17;++i)invalidTerms["Trace"]["IncludeAny"].push_back("term");rejects([&]{ValidateTraceProfile(invalidTerms);});
    auto precise=multi;precise["Trace"]["IncludeAny"]={"OnHarvestCompleted","OnInventoryAdded"};precise["Trace"]["CaptureParameters"]=true;
    ValidateTraceProfile(precise);
    precise["Trace"]["IncludeAny"].push_back("loot");rejects([&]{ValidateTraceProfile(precise);});
    profile["Trace"]["Filter"]="Multicast_SendPayloadForSpellCasting";
    profile["Trace"]["CaptureParameters"]=true;
    assert(ValidateTraceProfile(profile)["Trace"]["CaptureParameters"]==true);
    auto invalid=profile;invalid["Trace"]["Filter"]="";rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Name"]="../bad";rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["AutoStart"]=true;rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Trace"]["Seconds"]=61;rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Trace"]["MaxEvents"]=0;rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Export"]["Path"]="C:/outside";rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Version"]=2;rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Trace"]["CaptureParameters"]="yes";rejects([&]{ValidateTraceProfile(invalid);});
    invalid=profile;invalid["Trace"]["Filter"]=std::string("validname\0hidden",16);rejects([&]{ValidateTraceProfile(invalid);});
    assert(PS::InspectionTools::ParseTraceProfile("// comment\n"+profile.dump())==profile);
    auto targeted=profile;
    targeted["Target"]={{"Mode","ClassAndName"},{"ClassContains","Preview"},{"NameContains","Character"}};
    assert(ValidateTraceProfile(targeted)["Target"]["Mode"]=="ClassAndName");
    auto badTarget=targeted;badTarget["Target"]["Mode"]="Unknown";rejects([&]{ValidateTraceProfile(badTarget);});
    rejects([&]{PS::InspectionTools::ParseTraceProfile(std::string(40,'[')+"0"+std::string(40,']'));});
    rejects([&]{PS::InspectionTools::ParseTraceProfile(std::string(32769,' '));});
    assert(PS::InspectionTools::TraceTargetAfterProfileLoad(nlohmann::json::object(),"book","")=="book");
    assert(PS::InspectionTools::TraceTargetAfterProfileLoad(targeted,"book","widget")=="widget");
    assert(PS::InspectionTools::TraceTargetAfterProfileLoad(targeted,"book","").empty());
    if(argc==2) {
        size_t count=0;
        for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
            const auto extension=entry.path().extension();
            if(extension!=".json" && extension!=".jsonc")continue;
            std::ifstream input(entry.path());
            if(!input)throw std::runtime_error("Could not read trace profile");
            const std::string text((std::istreambuf_iterator<char>(input)),{});
            const auto filename=entry.path().filename().string();
            json actual;
            try { actual=ValidateTraceProfile(PS::InspectionTools::ParseTraceProfile(text)); }
            catch(const std::exception& error) {
                std::cerr<<filename<<": "<<error.what()<<'\n';
                return 2;
            }
            if(filename=="QuestHarvestCredit.jsonc" || filename=="QuestBuildPlacementCredit.jsonc" || filename=="QuestEnemyLootCredit.jsonc") {
                const PS::PlayerTrace::NameFilters filtered(actual["Trace"]);
                for(const auto* routine:{"getattributescomponent","getgameplayeffectscomponent","gettrackedstatuseffectscomponent","getsheltercomponent","islocalcontroller","isinlightningstorm","updatewetness"})
                    assert(!filtered.MatchesLower(std::string("/script/game:")+routine));
                const auto action=filename=="QuestHarvestCredit.jsonc"?"onresourceharvested":filename=="QuestBuildPlacementCredit.jsonc"?"onbuildingplaced":"oninventorychanged";
                assert(filtered.MatchesLower(std::string("/script/game:")+action));
            }
            ++count;
        }
        std::cout<<count<<" actual trace profiles validated\n";
    }
}
