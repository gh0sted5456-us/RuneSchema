#include "Loader/QuestReceiptJournal.h"
#include "Loader/QuestStageProgress.h"
#include "Loader/QuestHandIns.h"
#include <cassert>
#include <memory>
using namespace DragonWilds::Quests;
using Result=DragonWilds::QuestProgress::Result;
int main() {
    const Json document={{"Id","rat_work"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"},{"Title","Rat work"},{"Description","Choose either route"},
        {"Stages",Json::array({{{"Id","work"},{"Completion","Any"},{"Objectives",Json::array({
            {{"Id","rats"},{"Text","Kill rats"},{"Type","Kill"},{"AIClasses",{"/Test/Rat.Rat_C"}},{"Count",50},{"Location",{17355,171163,-185}},{"RadiusMeters",300}},
            {{"Id","meat"},{"Text","Bring meat"},{"Item","/Test/Meat.Meat"},{"Count",50}}})}}})},
        {"Reward",{{"Item","/Test/Reward.Reward"},{"Count",1}}},{"Repeatable",true},{"Repeat",{{"CooldownSeconds",300}}}};
    const auto quest=Parse("Test",document);
    const Stages::Graph graph{{{"work",{{"rats",Stages::Kind::Kill,{"/Test/Rat.Rat_C"},50,Stages::Area{{17355,171163,-185},300}},
        {"meat",Stages::Kind::TurnIn,{"/Test/Meat.Meat"},50,{}}},true}}};
    std::map<std::string,int> saved;int meat=50,rewards=0;int64_t now=1000;bool initialized=false;std::string state="Ungiven";
    const auto read=[&](const auto& key){return saved[key];};const auto write=[&](const auto& key,int value){saved[key]=value;};
    const auto legacy=std::filesystem::current_path()/"absent-alternative-legacy.json";
    const auto load=[&]{return std::make_unique<ReceiptJournal>(quest,document,legacy,"DA2312CD4B10577D552EE7975423CF88",read,write,
        [&]{return initialized;},[&]{initialized=true;},[&]{return state;},[&]{return now;});};
    auto receipt=load();
    const auto accept=[&]{return receipt->Accept([]{return true;},[&](bool){state="Given";Stages::Progress p(graph,receipt->Run()+1,read,write);p.BeginRun();return true;},[]{return true;});};
    assert(accept()==Result::Accepted);
    const auto turnIn=[&] {
        Stages::Progress progress(graph,receipt->Run(),read,write);
        if(!progress.Complete()) {
            const auto plan=SelectHandIns(quest.Stages[0].second,[&](size_t i){return progress.Count(0,i);},
                [](const auto&){return true;},[&](const auto&){return meat;},true);
            if(!plan.Objectives.empty()) {
                assert(plan.Objectives==std::vector<size_t>{1} && plan.Amounts.begin()->second==50);
                const auto exchanged=receipt->StageExchange([&]{return meat>=50;},[&]{meat-=50;return true;},[&]{write("RuneSchema.Objective:work:meat",50);return true;},[]{return true;});
                if(exchanged!=Result::Accepted)return exchanged;
            }
        }
        return receipt->TurnIn([&]{return progress.Complete();},[]{return true;},[&]{++rewards;return true;},[&]{state="Complete";return true;});
    };
    Stages::Progress killed(graph,receipt->Run(),read,write);
    assert(killed.Apply({Stages::Kind::Kill,"/Test/Rat.Rat_C",50,std::array<double,3>{47356,171163,0}})==0); // Outside radius.
    killed.Apply({Stages::Kind::Kill,"/Test/Rat.Rat_C",50,std::array<double,3>{17355,171163,-185}});
    receipt=load();assert(turnIn()==Result::Completed && rewards==1 && meat==50); // Both ready; kill wins.
    assert(turnIn()==Result::AlreadyComplete && rewards==1 && meat==50);
    assert(accept()==Result::AlreadyComplete);now+=300;assert(accept()==Result::Accepted);
    receipt=load();assert(turnIn()==Result::Completed && rewards==2 && meat==0); // Carried meat wins second run.
    assert(turnIn()==Result::AlreadyComplete && rewards==2 && meat==0);
    // Separate character state does not inherit this player's progress/reward.
    saved.clear();initialized=false;state="Ungiven";meat=49;now+=300;receipt=load();assert(accept()==Result::Accepted);
    assert(turnIn()==Result::NotReady && meat==49 && rewards==2);
}
