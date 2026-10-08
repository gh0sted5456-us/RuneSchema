#include "Loader/EventDefinition.h"
#include "Loader/QuestDefinition.h"
#include <cassert>
#include <filesystem>
#include <fstream>
int main(int argc,char** argv) {
    assert(argc==2);const std::filesystem::path root(argv[1]);
    const auto read=[&](const char* path){std::ifstream file(root/path);assert(file.good());return nlohmann::json::parse(file);};
    const std::string mod="RuneSchema2VendorTest";
    std::map<std::string,DragonWilds::Events::SpawnTemplate> spawns;
    for(const auto& raw:read("EventTrial/spawns/50-TrainingEnemies.json")){auto value=DragonWilds::Events::ParseSpawn(mod,raw);spawns.emplace(value.Key,value);}
    const auto event=DragonWilds::Events::Parse(mod,read("EventTrial/events/50-WolfTrial.json"));
    assert(event.Waves.size()==2 && event.Waves[0].size()==2 && event.Waves[1].size()==1);
    for(const auto& wave:event.Waves)for(const auto& member:wave)assert(spawns.contains(member.Spawn));
    const auto dialogue=DragonWilds::Dialogue::Parse(mod,read("EventTrial/dialogue/40-MarkerSupper.json"));
    assert(dialogue.Events.size()==1 && dialogue.Events.contains(event.Key));
    DragonWilds::Quests::Catalog quests;
    quests.Add(mod,read("QuestMarker/quests/20-MarkerSupper.json"));quests.Add(mod,read("QuestArea/quests/30-CabbageArea.json"));
    for(const auto& key:dialogue.Quests)quests.Find(mod,key);
    assert(spawns.at(mod+":trial_named_wolf").Name=="Cabbage Bandit");
}
