#include "Loader/QuestDefinition.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/NpcCatalog.h"
#include <cassert>
#include <fstream>
#include <filesystem>
int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path root(argv[1]);
    const auto read=[&](const char* path) {
        std::ifstream stream(root/path);assert(stream.good());
        return nlohmann::json::parse(stream,nullptr,true,true);
    };
    const std::string mod="RuneSchema2VendorTest";
    DragonWilds::Quests::Catalog quests;
    quests.Add(mod,read("quests/10-Supper.json"));
    const auto dialogue=DragonWilds::Dialogue::Parse(mod,read("dialogue/30-Supper.json"));
    assert(dialogue.Quests.size()==1);
    for(const auto& ref:dialogue.Quests)assert(quests.Find(mod,ref).Required.Count==3);
    const auto npc=read("npc/30-SupperQuartermaster.json");
    DragonWilds::NpcCatalog catalog;catalog.AddNpc(mod,npc);
    assert(DragonWilds::Dialogue::Reference(mod,npc.at("DialogueID"))==dialogue.Key);
}
