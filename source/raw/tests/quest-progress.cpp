#include "Loader/QuestProgress.h"
#include <cassert>
#include <chrono>
using namespace DragonWilds::QuestProgress;
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    const auto folder=std::filesystem::temp_directory_path()/("runeschema-quest-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=folder/"one.json",other=folder/"two.json";
    const std::string character="11111111222222223333333344444444",second="22222222222222223333333344444444";
    Json definition={{"Id","supper"},{"PersistenceID","AAAAAAAAAAAAAAAAAAAAAA"},{"Title","Supper"},{"Description","Bring food."},
        {"Objective",{{"Id","bring"},{"Text","Bring food."},{"Item","/Game/Test/Cabbage.Cabbage"},{"Count",3}}},
        {"Reward",{{"Item","/Game/Test/Potato.Potato"},{"Count",1}}}};
    const auto quest=DragonWilds::Quests::Parse("Test",definition);
    int takes=0,gives=0,completes=0,accepts=0;
    const auto yes=[]{return true;};
    const auto accept=[&]{++accepts;assert(Read(path,character)["Quests"][quest.Key]["Operation"]=="accept");return true;};
    const auto take=[&]{++takes;assert(Read(path,character)["Quests"][quest.Key]["Operation"]=="turn_in");return true;};
    const auto give=[&]{++gives;return true;};
    const auto complete=[&]{++completes;return true;};
    assert(TurnIn(path,character,"Test",definition,yes,take,give,complete)==Result::NotReady);
    assert(!std::filesystem::exists(path) && takes==0);
    assert(Accept(path,character,"Test",definition,accept)==Result::Accepted);
    assert(Accept(path,character,"Test",definition,accept)==Result::AlreadyActive && accepts==1);
    assert(TurnIn(path,character,"Test",definition,[&]{
        assert(Rejects([&]{Accept(path,character,"Test",definition,yes);}));return false;
    },take,give,complete)==Result::NotReady);
    assert(TurnIn(path,character,"Test",definition,[]{return false;},take,give,complete)==Result::NotReady);
    assert(takes==0 && gives==0);
    assert(TurnIn(path,character,"Test",definition,yes,take,give,complete)==Result::Completed);
    assert(Status(Read(path,character),quest,definition)==State::Complete);
    assert(TurnIn(path,character,"Test",definition,yes,take,give,complete)==Result::AlreadyComplete);
    assert(Accept(path,character,"Test",definition,accept)==Result::AlreadyComplete);
    assert(takes==1 && gives==1 && completes==1);
    auto changed=definition;changed["Reward"]["Count"]=2;
    assert(Rejects([&]{Accept(path,character,"Test",changed,yes);}));
    assert(Rejects([&]{Read(path,second);}));
    assert(Rejects([&]{Read(path,"");}));
    assert(Accept(other,second,"Test",definition,yes)==Result::Accepted);
    assert(Rejects([&]{TurnIn(other,second,"Test",definition,yes,yes,[]()->bool{throw std::runtime_error("uncertain grant");},yes);}));
    assert(TurnIn(other,second,"Test",definition,yes,take,give,complete)==Result::Uncertain);
    assert(gives==1 && completes==1);
    for(const auto operation:{"accept","take","give","complete"}) {
        const auto failure=folder/(std::string(operation)+".json");
        if(std::string(operation)=="accept")assert(Accept(failure,character,"Test",definition,[]{return false;})==Result::Uncertain);
        else {
            assert(Accept(failure,character,"Test",definition,yes)==Result::Accepted);
            assert(TurnIn(failure,character,"Test",definition,yes,
                [&]{return std::string(operation)!="take";},[&]{return std::string(operation)!="give";},
                [&]{return std::string(operation)!="complete";})==Result::Uncertain);
        }
        assert(Accept(failure,character,"Test",definition,yes)==Result::Uncertain);
        std::filesystem::remove(failure);
    }
    PS::ConfigFiles::Write(other,"corrupt");
    assert(Rejects([&]{Accept(other,second,"Test",definition,yes);}));
    assert(PS::ConfigFiles::Read(other)=="corrupt");
    auto oversized=Json{{"Payload",std::string(512*1024,'x')}};
    assert(Rejects([&]{Write(other,oversized);}));
    assert(PS::ConfigFiles::Read(other)=="corrupt");
    std::filesystem::remove(path);std::filesystem::remove(other);std::filesystem::remove(folder);
}
