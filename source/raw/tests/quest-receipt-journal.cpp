#include "Loader/QuestReceiptJournal.h"
#include <cassert>
using namespace DragonWilds::Quests;
using Result=DragonWilds::QuestProgress::Result;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    Json document={{"Id","test"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"},{"Title","Test"},{"Description","Test"},
        {"Objective",{{"Id","collect"},{"Text","Collect"},{"Item","/Game/Cabbage.Cabbage"},{"Count",3}}},
        {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}},{"Repeatable",true},{"Repeat",{{"CooldownSeconds",100}}}};
    const auto quest=Parse("Test",document);
    const std::string character="DA2312CD4B10577D552EE7975423CF88";
    const auto legacy=std::filesystem::temp_directory_path()/("RuneSchema-journal-no-file-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    std::map<std::string,int> variables;bool initialized=false;std::string nativeState="Ungiven",failKey;
    int64_t now=1000;int payments=0,rewards=0;
    auto load=[&]{return ReceiptJournal(quest,document,legacy,character,
        [&](const auto& key){return variables[key];},[&](const auto& key,int value){if(key==failKey)throw std::runtime_error("Injected save write failure");variables[key]=value;},
        [&]{return initialized;},[&]{initialized=true;},[&]{return nativeState;},[&]{return now;});};
    auto journal=load();assert(journal.Phase()==0 && initialized);
    auto accept=[&](auto& receipt){return receipt.Accept([]{return true;},[&](bool repeat){assert(repeat==(nativeState=="Complete"));nativeState="Given";return true;},[&]{++payments;return true;});};
    auto complete=[&](auto& receipt){return receipt.TurnIn([]{return true;},[]{return true;},[&]{++rewards;return true;},[&]{nativeState="Complete";return true;});};
    assert(accept(journal)==Result::Accepted && payments==1 && journal.Run()==1);
    assert(accept(journal)==Result::AlreadyActive && payments==1);
    auto reloaded=load();assert(reloaded.Phase()==1);
    int stagePayments=0,stageCommits=0;
    assert(reloaded.StageExchange([]{return true;},[&]{++stagePayments;return true;},[&]{++stageCommits;return true;},[]{return true;})==Result::Accepted);
    assert(stagePayments==1 && stageCommits==1 && reloaded.Phase()==1);
    assert(complete(reloaded)==Result::Completed && rewards==1);
    assert(complete(reloaded)==Result::AlreadyComplete && rewards==1);
    assert(accept(reloaded)==Result::AlreadyComplete && payments==1);
    now=1100;assert(accept(reloaded)==Result::Accepted && payments==2 && reloaded.Run()==2);
    assert(complete(reloaded)==Result::Completed && rewards==2);
    now=1200;failKey="RuneSchema.Run";
    assert(Rejects([&]{accept(reloaded);}));assert(payments==3);
    failKey.clear();auto uncertain=load();assert(uncertain.Phase()==2);
    assert(accept(uncertain)==Result::Uncertain && payments==3);
    assert(complete(uncertain)==Result::Uncertain && rewards==2);
    assert(!std::filesystem::exists(legacy));
    auto changed=document;changed["Reward"]["Count"]=2;const auto different=Parse("Test",changed);
    assert(Rejects([&]{ReceiptJournal invalid(different,changed,legacy,character,[&](const auto& key){return variables[key];},[&](const auto& key,int value){variables[key]=value;},[]{return true;},[]{},[&]{return nativeState;});}));
    int changedResets=0;
    ReceiptJournal resetChanged(different,changed,legacy,character,[&](const auto& key){return variables[key];},
        [&](const auto& key,int value){variables[key]=value;},[]{return true;},[]{},[&]{return nativeState;},ReceiptJournal::EpochNow,
        [&]{++changedResets;variables.clear();nativeState="Ungiven";});
    assert(changedResets==1 && resetChanged.Phase()==0 && resetChanged.Run()==0 && variables["RuneSchema.Version"]==OwnershipVersion);
    variables.clear();nativeState="Complete";auto portable=load();assert(portable.Phase()==4);
    assert(complete(portable)==Result::AlreadyComplete && rewards==2);
    variables.clear();nativeState="Ungiven";auto stageFailure=load();assert(accept(stageFailure)==Result::Accepted);
    assert(stageFailure.StageExchange([]{return true;},[&]{++stagePayments;return false;},[]{return true;},[]{return true;})==Result::Uncertain);
    const int priorPayments=stagePayments;
    auto pendingStage=load();assert(pendingStage.Phase()==3);
    assert(pendingStage.StageExchange([]{return true;},[&]{++stagePayments;return true;},[]{return true;},[]{return true;})==Result::Uncertain);
    assert(stagePayments==priorPayments);
    // Native completion can succeed before the final receipt write fails.
    variables.clear();nativeState="Ungiven";now=1500;
    auto rewardRecovery=load();assert(accept(rewardRecovery)==Result::Accepted);
    const int rewardBefore=rewards;
    failKey="RuneSchema.CompletedLo";
    assert(Rejects([&]{complete(rewardRecovery);}));
    failKey.clear();assert(nativeState=="Complete" && rewards==rewardBefore+1);
    auto rewardReloaded=load();assert(rewardReloaded.Phase()==3);
    const auto pendingReward=variables;
    assert(!rewardReloaded.RecoverConfirmedExchange("Given") && variables==pendingReward);
    assert(rewardReloaded.RecoverConfirmedExchange("Complete"));
    assert(rewardReloaded.Phase()==4 && rewards==rewardBefore+1);
    assert(!rewardReloaded.RecoverConfirmedExchange("Complete"));
    assert(complete(rewardReloaded)==Result::AlreadyComplete && rewards==rewardBefore+1);
    now=1599;assert(accept(rewardReloaded)==Result::AlreadyComplete);
    now=1600;assert(accept(rewardReloaded)==Result::Accepted);
    // An uncertain give must never be upgraded to a confirmed reward.
    assert(rewardReloaded.TurnIn([]{return true;},[]{return true;},[]{return false;},[]{return true;})==Result::Uncertain);
    const auto unconfirmed=variables;
    assert(!rewardReloaded.RecoverConfirmedExchange("Complete") && variables==unconfirmed);
    // A stage's confirmed commit can recover without another item removal.
    variables.clear();nativeState="Ungiven";
    auto stageRecovery=load();assert(accept(stageRecovery)==Result::Accepted);
    assert(Rejects([&]{stageRecovery.StageExchange([]{return true;},[]{return true;},[&]{failKey="RuneSchema.Phase";return true;},[]{return true;});}));
    failKey.clear();auto stageReloaded=load();const auto pendingCommit=variables;
    assert(!stageReloaded.RecoverConfirmedExchange("Complete") && variables==pendingCommit);
    assert(stageReloaded.RecoverConfirmedExchange("Given") && stageReloaded.Phase()==1);
    assert(!stageReloaded.RecoverConfirmedExchange("Given"));
    // Old/unknown pending records remain untouched: no evidence, no replay.
    variables["RuneSchema.Phase"]=3;variables["RuneSchema.ExchangeStep"]=0;
    const auto unknownPending=variables;
    assert(!stageReloaded.RecoverConfirmedExchange("Given") && variables==unknownPending);
    for(bool explicitFalse:{false,true}) {
        auto single=document;single.erase("Repeat");single.erase("Repeatable");
        if(explicitFalse)single["Repeatable"]=false;
        auto repeat=single;repeat["Repeatable"]=true;
        auto singleDef=Parse("Test",single),repeatDef=Parse("Test",repeat);
        variables.clear();nativeState="Complete";
        auto construct=[&](const auto& def,const auto& doc){return ReceiptJournal(def,doc,legacy,character,
            [&](const auto& key){return variables[key];},[&](const auto& key,int value){variables[key]=value;},
            []{return true;},[]{},[&]{return nativeState;},[&]{return now;});};
        auto old=construct(singleDef,single);const auto before=variables;
        auto migrated=construct(repeatDef,repeat);
        assert(migrated.Phase()==4 && migrated.Run()==old.Run());
        for(const auto& [key,value]:before)if(key!="RuneSchema.DefLo" && key!="RuneSchema.DefHi")assert(variables[key]==value);
        variables=before;variables["RuneSchema.Phase"]=3;
        assert(Rejects([&]{construct(repeatDef,repeat);}));
        variables=before;auto changedReward=repeat;changedReward["Reward"]["Count"]=20;
        auto changedDef=Parse("Test",changedReward);
        assert(Rejects([&]{construct(changedDef,changedReward);}));
        variables.clear();nativeState="Given";
        auto active=construct(singleDef,single);
        variables["test_kill_counter"]=3;
        const auto activeBefore=variables;
        auto activeMigrated=construct(repeatDef,repeat);
        assert(activeMigrated.Phase()==1 && activeMigrated.Run()==active.Run());
        for(const auto& [key,value]:activeBefore)
            if(key!="RuneSchema.DefLo" && key!="RuneSchema.DefHi")assert(variables[key]==value);
        auto again=construct(repeatDef,repeat);
        assert(again.Phase()==1 && variables["test_kill_counter"]==3);
        for(int phase:{0,2,3}) {
            variables=activeBefore;variables["RuneSchema.Phase"]=phase;
            assert(Rejects([&]{construct(repeatDef,repeat);}));
        }
        variables=activeBefore;nativeState="Complete";
        assert(Rejects([&]{construct(repeatDef,repeat);}));
        variables=activeBefore;nativeState="Given";
        auto changedCount=repeat;changedCount["Objective"]["Count"]=12;
        auto countDef=Parse("Test",changedCount);
        assert(Rejects([&]{construct(countDef,changedCount);}));
        assert(Rejects([&]{construct(changedDef,changedReward);}));
    }
    {
        variables.clear();nativeState="Ungiven";now=2000;
        auto receipt=load();assert(accept(receipt)==Result::Accepted);
        assert(complete(receipt)==Result::Completed);
        variables["RuneSchema.Phase"]=2;
        const auto before=variables;
        assert(!receipt.RecoverUnchargedRepeat(false) && variables==before);
        assert(receipt.RecoverUnchargedRepeat(true) && receipt.Phase()==4);
        for(const auto& [key,value]:before)if(key!="RuneSchema.Phase")assert(variables[key]==value);
        const int savedPayments=payments,savedRewards=rewards;
        assert(accept(receipt)==Result::AlreadyComplete); // cooldown still applies
        assert(payments==savedPayments && rewards==savedRewards);
        assert(!receipt.RecoverUnchargedRepeat(true));
        for(int phase:{0,1,3,4}) {
            variables["RuneSchema.Phase"]=phase;
            assert(!receipt.RecoverUnchargedRepeat(true));
        }
        variables["RuneSchema.Phase"]=2;variables["RuneSchema.Run"]=0;
        assert(!receipt.RecoverUnchargedRepeat(true));
        variables["RuneSchema.Run"]=1;variables["RuneSchema.CompletedLo"]=0;
        assert(!receipt.RecoverUnchargedRepeat(true));
        for(bool tiered:{false,true}) {
            auto paid=document;
            if(tiered)paid["EntryOptions"]=Json::array({{{"Id","entry"},{"Cost",{{"Item","/Game/Coin.Coin"},{"Count",1}}}}});
            else paid["StartCost"]={{"Item","/Game/Coin.Coin"},{"Count",1}};
            auto paidQuest=Parse("Test",paid);
            variables.clear();nativeState="Complete";
            ReceiptJournal paidReceipt(paidQuest,paid,legacy,character,
                [&](const auto& key){return variables[key];},[&](const auto& key,int value){variables[key]=value;},
                []{return true;},[]{},[&]{return nativeState;},[&]{return now;});
            variables["RuneSchema.Phase"]=2;
            const auto paidBefore=variables;
            assert(!paidReceipt.RecoverUnchargedRepeat(true) && variables==paidBefore);
        }
    }
}
