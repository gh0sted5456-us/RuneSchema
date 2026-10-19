#include "Loader/QuestDefinition.h"
#include "Loader/QuestBoardCancellation.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using Json=nlohmann::json;
void Require(bool condition){if(!condition)throw std::runtime_error("Quest Board walkthrough tie-in mismatch");}
Json Read(const std::filesystem::path& path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing walkthrough starter");return Json::parse(file,nullptr,true,true);}
int main(int argc,char** argv)try{
    Require(argc==2);const std::filesystem::path root=argv[1];
    const auto quest=DragonWilds::Quests::Parse("MyQuests",Read(root/"quests/10-gather-ash.jsonc"));
    Require(quest.Key=="MyQuests:gather_ash"&&quest.Acquire&&quest.AutomaticReward);
    const auto assets=Read(root/"assets/20-ash-contract.jsonc");Require(assets.size()==1);
    const auto path=assets.begin().key();const auto& item=assets.begin().value();
    const auto scroll=DragonWilds::QuestScrolls::Parse("MyQuests",item.at("$QuestScroll"));
    Require(scroll.QuestKey==quest.Key&&scroll.PersistenceId==item.at("PersistenceID").get<std::string>()&&scroll.PersistenceId!=quest.PersistenceId);
    Require(item.at("$Clone")=="/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template.ITEM_QuestScroll_Template");
    const auto recipes=Read(root/"recipes/30-ash-contract.jsonc");Require(recipes.size()==1);const auto& recipe=recipes.begin().value();
    Require(recipe.at("ItemsCreated")[0].at("ItemData")==path&&recipe.at("ItemsCreated")[0].at("Count")==1);
    const auto placements=DragonWilds::QuestCancellations::BoardPlacements(recipe);Require(placements.size()==1);
    Require(DragonWilds::QuestCancellations::Build(scroll,path,quest.Title,placements).has_value());
    std::cout<<"Documented quest, acceptance identity, helper, output and cancellation board tie-ins validated.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
