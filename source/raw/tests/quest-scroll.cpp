#include "Loader/QuestScroll.h"
#include "Loader/QuestReceiptJournal.h"
#include <cassert>
using namespace DragonWilds;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    const nlohmann::json declaration={{"QuestID","rat_work"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"}};
    const auto scroll=QuestScrolls::Parse("JobBoard",declaration);
    assert(scroll.QuestKey=="JobBoard:rat_work"&&scroll.EntryId.empty());
    auto foreign=declaration;foreign["QuestID"]="Dye:rat_work";foreign["EntryID"]="standard";
    assert(QuestScrolls::Parse("JobBoard",foreign).QuestKey=="Dye:rat_work");
    auto executable=declaration;executable["QuestID"]="/Game/Arbitrary.Function";assert(Rejects([&]{QuestScrolls::Parse("JobBoard",executable);}));
    for(auto invalid:{nlohmann::json{{"QuestID","q"}},nlohmann::json{{"QuestID","q"},{"PersistenceID","bad"}},nlohmann::json{{"QuestID","q"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"},{"Function","GiveReward"}}})assert(Rejects([&]{QuestScrolls::Parse("JobBoard",invalid);}));
    QuestScrolls::Catalog catalog;const std::string item="/Game/Mods/JobBoard/Items/Scroll.Scroll";
    catalog.Bind(item,scroll);catalog.Bind(item,scroll);assert(catalog.Find(item)==scroll);
    assert(!catalog.Find("/Game/Other.Other"));
    assert(Rejects([&]{catalog.Bind("/Game/Other.Other",scroll);}));
    auto changed=scroll;changed.QuestKey="JobBoard:different";assert(Rejects([&]{catalog.Bind(item,changed);}));
    // Actual receipt machinery: acceptance pays one scroll only after confirmed
    // native state; duplicate active/cooldown/rejected/missing routes never pay.
    const Quests::Json doc={{"Id","work"},{"PersistenceID","wOZh7obI0EuSV8pQs7_RaQ"},{"Title","Work"},{"Description","Work"},
        {"Objective",{{"Id","collect"},{"Text","Collect"},{"Item","/Game/Log.Log"},{"Count",2}}},
        {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}},{"Repeatable",true},{"Repeat",{{"CooldownSeconds",100}}}};
    const auto quest=Quests::Parse("JobBoard",doc);std::map<std::string,int> vars;
    Quests::Catalog quests;quests.Add("JobBoard",doc);
    assert(Rejects([&]{quests.Find("JobBoard","missing_quest");}));
    bool init=false;std::string state="Ungiven";int scrolls=3,rewards=0,starts=0;int64_t now=1000;
    const auto path=std::filesystem::temp_directory_path()/("quest-scroll-no-write-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    Quests::ReceiptJournal receipt(quest,doc,path,"DA2312CD4B10577D552EE7975423CF88",[&](const auto& key){return vars[key];},[&](const auto& key,int v){vars[key]=v;},[&]{return init;},[&]{init=true;},[&]{return state;},[&]{return now;});
    using R=QuestProgress::Result;
    const auto use=[&](bool allowed=true){return receipt.Accept([&]{return allowed&&scrolls>0;},[&](bool){++starts;state="Given";return true;},[&]{--scrolls;return true;});};
    assert(use(false)==R::NotReady&&scrolls==3&&starts==0);
    assert(use()==R::Accepted&&scrolls==2&&starts==1&&receipt.Run()==1);
    assert(use()==R::AlreadyActive&&scrolls==2&&starts==1);
    const auto complete=[&]{return receipt.TurnIn([]{return true;},[]{return true;},[&]{++rewards;return true;},[&]{state="Complete";return true;});};
    assert(complete()==R::Completed&&rewards==1);assert(complete()==R::AlreadyComplete&&rewards==1);
    assert(use()==R::AlreadyComplete&&scrolls==2);now=1100;
    assert(use()==R::Accepted&&scrolls==1&&receipt.Run()==2);
    assert(complete()==R::Completed&&rewards==2);now=1200;
    assert(receipt.Accept([]{return true;},[](bool){return false;},[&]{--scrolls;return true;})==R::Uncertain);
    assert(scrolls==1&&use()==R::Uncertain&&rewards==2);
    assert(!std::filesystem::exists(path));
}
