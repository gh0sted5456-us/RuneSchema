#include "Loader/QuestDefinition.h"
#include "Loader/EventDefinition.h"
#include "Loader/NpcCatalog.h"
#include <filesystem>
#include <fstream>
#include <cassert>
using Json=nlohmann::json;
int main(int argc,char** argv) {
    assert(argc==2);const std::filesystem::path root=argv[1];
    const auto read=[&](const char* path){std::ifstream file(root/path);assert(file.good());return Json::parse(file,nullptr,true,true);};
    const std::string mod="RuneSchema2VendorTest";
    const auto quest=DragonWilds::Quests::Parse(mod,read("quests/80-BarkleysBandits.json"));
    const auto event=DragonWilds::Events::Parse(mod,read("events/80-BarkleyPatrol.json"));
    const auto spawn=DragonWilds::Events::ParseSpawn(mod,read("spawns/80-BarkleyBandits.json").at(0));
    const auto dialogue=DragonWilds::Dialogue::Parse(mod,read("dialogue/80-Barkley.json"));
    const auto npc=read("npc/80-Barkley.jsonc");
    DragonWilds::NpcCatalog catalog;catalog.AddNpc(mod,npc);assert(catalog.Resolve().size()==1);
    assert(npc["DialogueID"]=="barkley_contract" && npc["Type"]=="Resource");
    assert(quest.Repeat.Enabled && quest.Repeat.CooldownSeconds==60);
    assert(quest.Required.Count==3 && quest.Reward.Count==3);
    assert(quest.Kill->EventKey==event.Key && quest.Kill->SpawnKey==spawn.Key);
    assert(quest.Kill->Classes==std::vector<std::string>{spawn.Class});
    assert(dialogue.Quests.contains(quest.Key) && dialogue.Events.contains(event.Key));
    size_t count=0;for(const auto& wave:event.Waves)for(const auto& member:wave){assert(member.Spawn==spawn.Key);++count;}
    assert(count==3 && event.Key!="RuneSchema2VendorTest:wolf_trial");
}
