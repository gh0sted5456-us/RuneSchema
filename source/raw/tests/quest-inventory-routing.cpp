#include "Loader/QuestAcquisition.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("Completion, acquisition, action and dialogue sources required");
    const auto read=[](const char* path) {
        std::ifstream file(path);
        if(!file)throw std::runtime_error("Source unavailable");
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto completion=read(argv[1]),acquisition=read(argv[2]);
    const auto actions=read(argv[3]),dialogue=read(argv[4]);
    const auto guard=completion.substr(0,completion.find("bool enabled="));
    assert(guard.find("m_completingAutomaticQuests || m_observingAcquisition")!=guard.npos);
    // The same event retries completion after the observation guard has unwound.
    assert(acquisition.find("ObserveQuestInventory(controller,credit);ReconcileQuestLocations(controller);")!=acquisition.npos);
    assert(actions.find("const int carried=inventory.Count();")!=actions.npos);
    assert(actions.find("Quests::InventoryHandInStatus(carried,quest.Required.Count)")!=actions.npos);
    assert(actions.find("inventory.Take(quest.Required.Count,current)")!=actions.npos);
    assert(actions.find("if(!inArea)return false;")==actions.npos);
    assert(actions.find("visible=std::min(objective.Required.Count,DialogueInventory(controller")!=actions.npos);
    assert(dialogue.find("const auto plan=Quests::SelectHandIns(objectives")!=dialogue.npos);
    assert(dialogue.find("[](const Quests::Definition&){return true;},inventory")!=dialogue.npos);
    assert(dialogue.find("complete=DialogueInventory(controller,item).Count()>=quest.Required.Count")!=dialogue.npos);

    DragonWilds::Quests::AcquisitionBaseline baseline;
    assert(baseline.Observe("stone",0,false)==0);
    assert(baseline.Observe("stone",5,true)==5);
    // A subsequent hand-in updates the baseline before a reward adds the item.
    assert(baseline.Observe("stone",0,true)==0);
    assert(baseline.Observe("stone",2,true)==2);
    assert(baseline.Observe("stone",2,true)==0);
    assert(baseline.Observe("stone",3,true)==1);
    assert(DragonWilds::Quests::InventoryHandInStatus(2,5)==
        "Items carried: 2/5. Bring the requested items and leave room for your reward.");
}
