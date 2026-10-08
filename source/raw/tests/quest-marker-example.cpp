#include "Loader/QuestDefinition.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/NpcCatalog.h"
#include <cassert>
#include <filesystem>
#include <fstream>
int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path root(argv[1]);
    const auto read=[&](const char* path){std::ifstream stream(root/path);assert(stream.good());return nlohmann::json::parse(stream);};
    const std::string mod="RuneSchema2VendorTest";
    const auto definition=read("quests/20-MarkerSupper.json");
    const auto quest=DragonWilds::Quests::Parse(mod,definition);
    assert(quest.Marker && quest.PersistenceId!="lzAEAZSs5ky6-Npv52bb-Q");
    DragonWilds::Quests::Catalog quests;quests.Add(mod,definition);
    const auto dialogue=DragonWilds::Dialogue::Parse(mod,read("dialogue/40-MarkerSupper.json"));
    assert(dialogue.Quests.size()==1);
    for(const auto& ref:dialogue.Quests)assert(quests.Find(mod,ref).Key==quest.Key);
    const auto npc=read("npc/40-MarkerSurveyor.json");
    DragonWilds::NpcCatalog catalog;catalog.AddNpc(mod,npc);
    assert(DragonWilds::Dialogue::Reference(mod,npc.at("DialogueID"))==dialogue.Key);
    for(size_t i=0;i<3;++i)assert(npc.at("Location")[i].get<double>()==quest.Marker->Position[i]);
}
