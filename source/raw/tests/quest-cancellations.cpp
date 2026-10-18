#include "Loader/QuestBoardCancellation.h"
#include "Loader/QuestReceiptJournal.h"
#include "Loader/QuestStageProgress.h"
#include <fstream>
#include <iostream>
#include <set>
using namespace DragonWilds;
using Json=nlohmann::json;
void Check(bool v){if(!v)throw std::runtime_error("Quest cancellation regression failed");}
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
Json Read(const std::filesystem::path& file){std::ifstream in(file);if(!in)throw std::runtime_error("Missing quest fixture");return Json::parse(in);}
int main(int argc,char** argv) {
    const Json declaration={{"QuestID","Example:rat_work"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"}};
    const auto parent=QuestScrolls::Parse("Board",declaration);
    Check(parent.Action=="Accept");
    Check(QuestCancellations::CompletedRunsBeforeCancellation(1)==0);
    Check(QuestCancellations::CompletedRunsBeforeCancellation(2)==1);
    Check(Rejects([]{QuestCancellations::CompletedRunsBeforeCancellation(0);}));
    auto cancelDeclaration=declaration;cancelDeclaration["Action"]="Abandon";
    Check(QuestScrolls::Parse("Board",cancelDeclaration).Action=="Abandon");
    cancelDeclaration["EntryID"]="paid";Check(Rejects([&]{QuestScrolls::Parse("Board",cancelDeclaration);}));
    for(const auto action:{"Complete","TurnIn","Reset","RunFunction","abandon"}) {
        auto invalid=declaration;invalid["Action"]=action;Check(Rejects([&]{QuestScrolls::Parse("Board",invalid);}));
    }
    const Json board={{"AddTo",Json::array({{{"DataTable",QuestCancellations::Board},{"Row","QuestBoard"},{"Category","Bounties"}}})}};
    const auto places=QuestCancellations::BoardPlacements(board);Check(places.size()==1);
    const auto plan=QuestCancellations::Build(parent,"/Game/Scroll.Scroll","Rat Work",places);
    Check(bool(plan));Check(plan->Recipe.at("AddTo").at(0).at("Category")=="Quest Cancellations");
    Check(plan->Asset.at("Name")=="Cancel: Rat Work");
    const auto cancel=QuestScrolls::Parse(QuestCancellations::Owner,plan->Asset.at("$QuestScroll"));
    Check(cancel.QuestKey==parent.QuestKey && cancel.Action=="Abandon");
    Check(cancel.PersistenceId!=parent.PersistenceId && cancel.PersistenceId!=plan->Recipe.at("PersistenceID").get<std::string>());
    Check(!QuestCancellations::Build(cancel,plan->Path,"Cancel",places)); // No cancellation-of-cancellation.
    Check(QuestCancellations::Build(parent,"/Game/Renamed.Renamed","New title",places)->Path==plan->Path);
    auto other=parent;other.QuestKey="Other:rat_work";
    Check(QuestCancellations::Build(other,"/Game/Scroll.Scroll","Rat Work",places)->Path!=plan->Path);
    auto unrelated=board;unrelated["AddTo"][0]["DataTable"]="/Game/Smith.Smith";
    Check(QuestCancellations::BoardPlacements(unrelated).empty());
    unrelated=board;unrelated["AddTo"][0]["Category"]="Quest Cancellations";
    Check(QuestCancellations::BoardPlacements(unrelated).empty());
    auto duplicate=board;duplicate["AddTo"].push_back(duplicate["AddTo"][0]);
    Check(QuestCancellations::BoardPlacements(duplicate).size()==1);
    QuestScrolls::Catalog bindings;bindings.Bind("/Game/Scroll.Scroll",parent);bindings.Bind(plan->Path,cancel);bindings.Bind(plan->Path,cancel);
    Check(bindings.Find(plan->Path)->QuestKey==parent.QuestKey);
    int resets=0,charges=0,commits=0;bool active=true,ready=true,confirmed=true;
    const auto use=[&]{return QuestCancellations::Apply([&]{return active;},[&]{return ready;},[&]{++resets;active=false;},[&]{return confirmed;},[&]{++charges;return true;},[&]{++commits;});};
    ready=false;Check(!use() && resets==0 && charges==0);ready=true;
    Check(use() && resets==1 && charges==1 && commits==1);
    Check(!use() && charges==1 && resets==1); // Reuse/inactive/completed/pending cannot charge.
    active=true;confirmed=false;Check(Rejects(use) && charges==1 && commits==1);
    Check(!use() && charges==1); // Never replay an uncertain reset.
    Check(Rejects([&]{QuestCancellations::Apply([]{return true;},[]{return true;},[]{},[]{return true;},[]{return false;},[&]{++commits;});}) && commits==1);
    // Real receipt and stage stores: cancelling a repeat cannot revive its
    // paid first run, preserve staged credit, or issue another reward.
    const Json doc={{"Id","repeat_work"},{"PersistenceID","wOZh7obI0EuSV8pQs7_RaQ"},{"Title","Repeat Work"},{"Description","Work"},
        {"Objective",{{"Id","logs"},{"Text","Collect"},{"Item","/Game/Log.Log"},{"Count",2}}},
        {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}},{"Repeatable",true}};
    const auto q=Quests::Parse("Test",doc);std::map<std::string,int> vars;bool initialized=false;std::string state="Ungiven";
    const auto legacy=std::filesystem::temp_directory_path()/("quest-cancel-no-file-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    const auto read=[&](const std::string& key){return vars[key];};const auto write=[&](const std::string& key,int value){vars[key]=value;};
    const auto load=[&]{return Quests::ReceiptJournal(q,doc,legacy,"DA2312CD4B10577D552EE7975423CF88",read,write,[&]{return initialized;},[&]{initialized=true;},[&]{return state;},[]{return int64_t(1000);});};
    auto receipt=load();int rewards=0,cancellationPayments=0;
    const auto accept=[&](auto& journal){return journal.Accept([]{return true;},[&](bool){state="Given";return true;},[]{return true;});};
    Check(accept(receipt)==QuestProgress::Result::Accepted && receipt.Run()==1);
    Check(receipt.TurnIn([]{return true;},[]{return true;},[&]{++rewards;return true;},[&]{state="Complete";return true;})==QuestProgress::Result::Completed);
    Check(accept(receipt)==QuestProgress::Result::Accepted && receipt.Run()==2);
    namespace S=Quests::Stages;
    const S::Graph graph{{{"delivery",{{"logs",S::Kind::Gather,{"log"},2}}}}};
    S::Progress stages(graph,2,read,write);stages.BeginRun();stages.Apply({S::Kind::Gather,"log",1,std::nullopt});Check(stages.Count(0,0)==1);
    const auto prior=QuestCancellations::CompletedRunsBeforeCancellation(receipt.Run());
    Check(QuestCancellations::Apply([&]{return receipt.Phase()==1 && state=="Given";},[]{return true;},[&]{write("RuneSchema.Run",prior);write("RuneSchema.StageRun",prior);write("RuneSchema.Phase",0);state="Ungiven";},[&]{return state=="Ungiven";},[&]{++cancellationPayments;return true;},[]{}));
    auto reloaded=load();Check(reloaded.Run()==1 && rewards==1 && cancellationPayments==1 && vars["RuneSchema.CompletedLo"]==1000);
    Check(accept(reloaded)==QuestProgress::Result::Accepted && reloaded.Run()==2);
    S::Progress retried(graph,2,read,write);retried.BeginRun();Check(retried.Count(0,0)==0 && !retried.Complete());
    Check(rewards==1 && !std::filesystem::exists(legacy));
    if(argc>1) {
        const std::filesystem::path root=argv[1];
        const auto quests=Read(root/"quests/10-StarterQuests.json"),assets=Read(root/"assets/20-QuestContracts.json"),recipes=Read(root/"recipes/20-QuestContracts.json");
        QuestScrolls::Catalog catalog;for(const auto& [path,asset]:assets.items())catalog.Bind(path,QuestScrolls::Parse("JobBoardStarterQuests",asset.at("$QuestScroll")));
        std::map<std::string,QuestCancellations::Plan> generated;std::set<std::string> identities;
        for(const auto& [key,recipe]:recipes.items())for(const auto& output:recipe.at("ItemsCreated")) {
            const auto path=output.at("ItemData").get<std::string>();const auto bound=catalog.Find(path);Check(bool(bound));
            const auto q=std::find_if(quests.begin(),quests.end(),[&](const auto& q){return "JobBoardStarterQuests:"+q.at("Id").template get<std::string>()==bound->QuestKey;});Check(q!=quests.end());
            const auto generatedPlan=QuestCancellations::Build(*bound,path,q->at("Title").get<std::string>(),QuestCancellations::BoardPlacements(recipe));Check(bool(generatedPlan));
            generated.try_emplace(bound->QuestKey,*generatedPlan);
        }
        Check(generated.size()==20);Json outputAssets=Json::object(),outputRecipes=Json::object();
        for(const auto& [quest,p]:generated) {
            Check(identities.insert(p.Asset.at("PersistenceID").get<std::string>()).second);Check(identities.insert(p.Recipe.at("PersistenceID").get<std::string>()).second);
            outputAssets[p.Path]=p.Asset;outputRecipes[p.RecipeKey]=p.Recipe;
        }
        if(argc>2){std::filesystem::create_directories(argv[2]);std::ofstream(std::filesystem::path(argv[2])/"generated-assets.preview.json")<<outputAssets.dump(2);std::ofstream(std::filesystem::path(argv[2])/"generated-recipes.preview.json")<<outputRecipes.dump(2);}
        std::cout<<"PASS: 20 automatic cancellation scrolls/recipes, matching parents, 40 unique stable identities.\n";
    }
    std::cout<<"PASS: cancellation declaration, board filtering, stable identity, recursion prevention, authority-independent cancellation transaction.\n";
}
