#include "Loader/DialogueDefinition.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
using namespace DragonWilds::Dialogue;
template<class F> bool Rejects(F call){try{call();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv) {
    if(argc==2)for(const auto& file:std::filesystem::directory_iterator(argv[1])) {
        if(file.path().extension()!=".json" && file.path().extension()!=".jsonc")continue;
        std::ifstream stream(file.path());
        assert(stream.good());
        Parse("RuneSchema2VendorTest",Json::parse(stream,nullptr,true,true));
    }
    const Json base={{"Id","guild_greeting"},{"Entry","hello"},{"Nodes",{
        {"hello",{{"Text","Welcome to the guild."},{"Choices",Json::array({
            {{"Id","trade"},{"Text","Show me your supplies."},{"VendorID","guild_supply"}},
            {{"Id","leave"},{"Text","Goodbye."},{"End",true}}
        })}}}
    }}};
    auto result=Parse("Test",base);
    auto gated=base;
    gated["Nodes"]["hello"]["Choices"][0]["WhenQuest"]={{"Id","debt"},{"States",Json::array({"Active"})}};
    assert(Parse("Test",gated).Quests.contains("Test:debt"));
    auto detailedGate=gated;
    detailedGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["Stage"]="collect";
    detailedGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["ObjectivesComplete"]=true;
    detailedGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["RepeatReady"]=false;
    Parse("Test",detailedGate);
    auto timeGate=base;
    timeGate["Nodes"]["hello"]["Choices"][0]["WhenTimeOfDay"]="Night";
    Parse("Test",timeGate);
    auto unlock=base;
    unlock["Nodes"]["hello"]["Choices"][0]["RequirementUnlock"]={{"Quest",{{"Id","debt"},{"States",Json::array({"Active"})},{"ObjectivesComplete",false}}},{"TimeOfDay","Night"}};
    assert(Parse("Test",unlock).Quests.contains("Test:debt"));
    auto mixedUnlock=unlock;mixedUnlock["Nodes"]["hello"]["Choices"][0]["WhenTimeOfDay"]="Night";
    assert(Rejects([&]{Parse("Test",mixedUnlock);}));
    auto emptyUnlock=base;emptyUnlock["Nodes"]["hello"]["Choices"][0]["RequirementUnlock"]=Json::object();
    assert(Rejects([&]{Parse("Test",emptyUnlock);}));
    auto invalidTime=timeGate;invalidTime["Nodes"]["hello"]["Choices"][0]["WhenTimeOfDay"]="Dusk";
    assert(Rejects([&]{Parse("Test",invalidTime);}));
    invalidTime=timeGate;invalidTime["Nodes"]["hello"]["Choices"][1]["WhenTimeOfDay"]="Day";
    assert(Rejects([&]{Parse("Test",invalidTime);}));
    for(const auto* field:{"ObjectivesComplete","RepeatReady"}) {
        auto badGate=detailedGate;badGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"][field]="true";
        assert(Rejects([&]{Parse("Test",badGate);}));
    }
    auto badStage=detailedGate;badStage["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["Stage"]="";
    assert(Rejects([&]{Parse("Test",badStage);}));
    auto invalidGate=gated;invalidGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["States"]=Json::array({"Given"});
    assert(Rejects([&]{Parse("Test",invalidGate);}));
    invalidGate=gated;invalidGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["States"]=Json::array({"Active","Active"});
    assert(Rejects([&]{Parse("Test",invalidGate);}));
    for(const auto& states:std::vector<Json>{Json::array(),Json("Active"),Json::array({3})}) {
        invalidGate=gated;invalidGate["Nodes"]["hello"]["Choices"][0]["WhenQuest"]["States"]=states;
        assert(Rejects([&]{Parse("Test",invalidGate);}));
    }
    invalidGate=gated;invalidGate["Nodes"]["hello"]["Choices"][1]["WhenQuest"]=gated["Nodes"]["hello"]["Choices"][0]["WhenQuest"];
    assert(Rejects([&]{Parse("Test",invalidGate);}));
    assert(result.Key=="Test:guild_greeting");
    assert(Parse("A Mod",base).Key=="A Mod:guild_greeting");
    assert(result.Stores==std::set<std::string>{"Test:guild_supply"});
    ValidateStores(result,{"Test:guild_supply"});
    assert(Rejects([&]{ValidateStores(result,{});}));
    auto bad=base;bad["Nodes"]["hello"]["Choices"][0]["Next"]="hello";
    assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Entry"]="missing";assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Nodes"]["unused"]=base["Nodes"]["hello"];assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Nodes"]["hello"]["Choices"][1]["Id"]="trade";assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Nodes"]["hello"]["Choices"][1]["End"]=false;assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Nodes"]["hello"]["Choices"][0]["VendorID"]="Other:supply";
    assert(Parse("Test",bad).Stores.contains("Other:supply"));
    bad["Nodes"]["hello"]["Choices"][0]["VendorID"]="Other:x:y";assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Nodes"]["hello"]["Choices"][1]={{"Id","again"},{"Text","Repeat."},{"Next","hello"}};
    Parse("Test",bad);
    bad=base;bad["Nodes"]["hello"]["Choices"][0]["Actions"]=Json::array();assert(Rejects([&]{Parse("Test",bad);}));
    bad=base;bad["Completion"]={{"Flag","heard"},{"Item","/Game/Item.Item"},{"Count",1}};
    assert(Rejects([&]{Parse("Test",bad);}));
    bad["Nodes"]["hello"]["Choices"][0]={{"Id","finish"},{"Text","Finish"},{"Next","hello"},{"Complete",true}};
    Parse("Test",bad);
    auto invalid=bad;invalid["Completion"]["Count"]=0;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=bad;invalid["Nodes"]["hello"]["Choices"][0]["Complete"]=false;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=bad;invalid["CompletedEntry"]="missing";assert(Rejects([&]{Parse("Test",invalid);}));
    std::cout<<"Dialogue definition tests passed\n";
    auto questDialogue=base;
    questDialogue["Nodes"]["hello"]["Choices"][0]={{"Id","accept"},{"Text","Accept"},{"Next","hello"},{"Quest",{{"Id","supper"},{"Action","Accept"}}}};
    assert(Parse("Test",questDialogue).Quests.contains("Test:supper"));
    questDialogue["Nodes"]["hello"]["Choices"][0]["Quest"]["Action"]="TurnIn";Parse("Test",questDialogue);
    questDialogue["Nodes"]["hello"]["Choices"][0]["Quest"]["Action"]="GiveReward";assert(Rejects([&]{Parse("Test",questDialogue);}));
    questDialogue["Nodes"]["hello"]["Choices"][0]["Quest"]["Action"]="Accept";
    questDialogue["Nodes"]["hello"]["Choices"][0]["Event"]={{"Id","supper_wave"},{"Action","Start"}};
    auto combined=Parse("Test",questDialogue);
    assert(combined.Quests.contains("Test:supper") && combined.Events.contains("Test:supper_wave"));
    auto invalidCombined=questDialogue;invalidCombined["Nodes"]["hello"]["Choices"][0]["Event"]["Action"]="Cancel";
    assert(Rejects([&]{Parse("Test",invalidCombined);}));
    invalidCombined=questDialogue;invalidCombined["Nodes"]["hello"]["Choices"][0]["Quest"]["Action"]="TurnIn";
    assert(Rejects([&]{Parse("Test",invalidCombined);}));
    questDialogue["Nodes"]["hello"]["Choices"][0].erase("Event");
    questDialogue["Nodes"]["hello"]["Choices"][0].erase("Next");questDialogue["Nodes"]["hello"]["Choices"][0]["End"]=true;
    Parse("Test",questDialogue);
    questDialogue["Nodes"]["hello"]["Choices"][0]["Quest"]["Action"]="Abandon";
    questDialogue["Nodes"]["hello"]["Choices"][0]["Event"]={{"Id","supper_wave"},{"Action","Cancel"}};
    Parse("Test",questDialogue);
    questDialogue["Nodes"]["hello"]["Choices"][0]["Event"]["Action"]="Start";
    assert(Rejects([&]{Parse("Test",questDialogue);}));
}
