#include "Loader/NpcCatalog.h"
#include "Loader/NpcPlacement.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/ItemIdentity.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <iostream>
using Json=nlohmann::json;
int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path root=argv[1];
    const auto read=[&](const char* path) {std::ifstream file(root/path);assert(file.good());return Json::parse(file,nullptr,true,true);};
    const auto npc=read("npc/20-Granite.jsonc");
    const auto assets=read("assets/20-GranitesLastWord.jsonc");
    const auto dialogue=read("dialogue/20-GraniteStory.jsonc");
    const auto recipes=read("recipes/20-SwordForCabbage.jsonc");
    const auto parsed=DragonWilds::Dialogue::Parse("RuneSchema2VendorTest",dialogue);
    DragonWilds::Dialogue::ValidateStores(parsed,{"RuneSchema2VendorTest:granite_armory"});
    DragonWilds::NpcCatalog catalog;catalog.AddNpc("RuneSchema2VendorTest",npc);
    const auto store=read("vendors/20-GraniteArmory.jsonc");catalog.AddStore("RuneSchema2VendorTest",store);
    const auto resolved=catalog.Resolve();assert(resolved.size()==1);
    DragonWilds::Dialogue::ValidateNpcStore(parsed,resolved[0].StoreOwner);
    assert(resolved[0].Data.at("Stage")=="Merchant" && resolved[0].Data.at("InteractionProperties").at("InteractionPrompt")=="Talk");
    bool rejected=false;try {DragonWilds::Dialogue::ValidateNpcStore(parsed,"");}catch(const std::exception&){rejected=true;}assert(rejected);
    assert(npc.at("Type")=="Resource" && npc.at("DialogueID")==dialogue.at("Id"));
    assert(DragonWilds::NpcPlacement::Parse(npc.at("Location")).Ground);
    assert(assets.size()==1 && recipes.size()==1);
    const auto sword=assets.begin().key();const auto& clone=assets.begin().value();
    assert(DragonWilds::IsCanonicalPersistenceId(clone.at("PersistenceID").get<std::string>()));
    assert(!DragonWilds::IsCanonicalPersistenceId("rs_granites_last_word_01"));
    assert(!DragonWilds::IsCanonicalPersistenceId("SlKnVsqo80W2CNH9iXnVQ1"));
    assert(store.at("Items")[0].at("Item")==sword && store.at("Items")[0].at("Price")==100);
    assert(store.at("RequiresFlag")==dialogue.at("Completion").at("Flag"));
    assert(clone.at("$Clone")!=sword && clone.at("DamageMultiplier")==540);
    assert(clone.at("PersistenceID")!="vI0ix0WJSchv2dysqMjcaw");
    assert(dialogue.at("Completion").at("Item")==sword && dialogue.at("Completion").at("Count")==1);
    const auto offer=DragonWilds::NpcCatalog::ParseStoreOffer("RuneSchema2VendorTest",recipes.begin().key(),recipes.begin().value());
    assert(offer.Data.at("Currency")==sword && offer.Data.at("Price")==1 && offer.Data.at("Count")==1);
    assert(DragonWilds::NpcCatalog::StoreTargets("RuneSchema2VendorTest",offer.Data)==std::set<std::string>{"RuneSchema2VendorTest:guild_supply"});
    std::set<std::string> visited;
    std::string current="chapter_01";
    size_t stages=0,completions=0;
    for(const auto& node:dialogue.at("Nodes"))for(const auto& choice:node.at("Choices"))if(choice.value("Complete",false))++completions;
    assert(completions==1);
    for(;;) {
        assert(visited.insert(current).second);
        const auto& node=dialogue.at("Nodes").at(current);
        const auto& choices=node.at("Choices");
        ++stages;assert(stages<=40 && choices.size()==2 && choices[1].value("End",false));
        if(choices[0].value("Complete",false)) {assert(choices[0].at("Next")=="rewarded");break;}
        current=choices[0].at("Next");
    }
    assert(stages==40 && dialogue.at("CompletedEntry")=="after");
    std::cout<<"Stone NPC, 40-stage story, clone reward and shared-store barter validated\n";
}
