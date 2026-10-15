#include "Loader/ModOrderState.h"
#include "Core/ConfigFiles.h"
#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace State=DragonWilds::ModOrderState;
static unsigned Checks=0;
static void Require(bool value,const char* why){++Checks;if(!value){std::cerr<<"check "<<Checks<<": "<<why<<'\n';throw std::runtime_error(why);}}
static bool Enabled(const std::vector<State::Entry>& entries,const std::string& name) {
    const auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return State::Fold(e.Name)==State::Fold(name);});
    Require(it!=entries.end(),"saved choice disappeared");return it->Enabled;
}
static int Run(int argc,char** argv) {
    Require(argc==2,"fixture root required");
    const auto parsed=State::Parse("\xef\xbb\xbf" "DyeGoldQuests : 0\r\nIncreasedStorage : 1\nINCREASEDSTORAGE : 0\n# comment\nInvalid : yes\nEmpty :\nImplicit : 1 ; comment\n../escape : 1\n");
    Require(parsed.size()==6,"BOM, comments, or unsafe folder parsing");
    Require(!parsed[0].Enabled&&parsed[0].Name=="DyeGoldQuests","BOM must not hide first disabled row");
    Require(!parsed[3].Enabled&&!parsed[4].Enabled&&!parsed[5].Enabled,"non-1 values must never enable code");
    bool encodingRejected=false;try{State::Parse(std::string("Dye\0 : 0",9));}catch(...){encodingRejected=true;}
    Require(encodingRejected,"binary/UTF-16 list must not silently become enable-all");
    auto plan=State::Reconcile(parsed,{"DyeGoldQuests","IncreasedStorage","NewMod","AA_First","ZZ_Last"});
    Require(!Enabled(plan.Entries,"increasedstorage"),"duplicate 0 must win regardless of case");
    Require(plan.Active==std::vector<std::string>{"AA_First","NewMod","ZZ_Last"},"disabled mods entered active list");
    Require(Enabled(plan.Entries,"NewMod"),"ordinary new-mod policy changed");
    auto absent=State::Reconcile(plan.Entries,{"NewMod"});
    Require(!Enabled(absent.Entries,"DyeGoldQuests")&&!Enabled(absent.Entries,"IncreasedStorage"),"missing disabled choices lost");
    Require(absent.Active==std::vector<std::string>{"NewMod"},"absent folders must not become active owners");
    auto restored=State::Reconcile(State::Parse(State::Serialize(absent.Entries,{},false)),{"NewMod","DyeGoldQuests","IncreasedStorage"});
    Require(restored.Active==std::vector<std::string>{"NewMod"},"folder restore reactivated disabled mod");
    auto missingEnabled=State::Reconcile({{"MissingButEnabled",true}},{});
    Require(missingEnabled.Active.empty()&&Enabled(missingEnabled.Entries,"MissingButEnabled"),"absent enabled preference not retained safely");
    auto ordering=State::Reconcile({{"ZZ_Explicit",true},{"B",true},{"AA_Explicit",false},{"A",true}},{"A","B","AA_Explicit","ZZ_Explicit"});
    Require(ordering.Active==std::vector<std::string>{"ZZ_Explicit","B","A"},"explicit prefix order changed");
    Require(!ordering.Changed,"unchanged list must not be rewritten");
    const auto manual=State::Reconcile(State::Parse("DyeGoldQuests : 1\n"),{"DyeGoldQuests"});
    Require(manual.Active==std::vector<std::string>{"DyeGoldQuests"},"explicit manual re-enable must still work");
    const State::Snapshot comments{true,"\xef\xbb\xbf; Keep this note\r\n\nDyeGoldQuests : 0\r\n"};
    const auto text=State::Serialize({{"DyeGoldQuests",false}},comments,true);
    Require(text.starts_with("; Keep this note\r\n")&&!State::Parse(text)[0].Enabled,"comments or disabled BOM row changed during save");
    const auto root=std::filesystem::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=root/"nested"/"runeschema.txt";
    Require(!State::Read(path).Exists,"new fixture already exists");
    const State::Snapshot initial;
    Require(State::WriteExpected(path,initial,"DyeGoldQuests : 0\nIncreasedStorage : 0\n"),"atomic initial save failed");
    auto baseline=State::Read(path);
    Require(baseline.Exists,"initial file not created");
    Require(!State::WriteExpected(path,initial,"DyeGoldQuests : 1\n"),"stale initial snapshot overwrote disabled row");
    Require(State::Read(path)==baseline,"rejected write changed bytes");
    Require(State::WriteExpected(path,baseline,"DyeGoldQuests : 0\nIncreasedStorage : 1\n"),"explicit change failed");
    const auto latest=State::Read(path);
    Require(!State::WriteExpected(path,baseline,"DyeGoldQuests : 1\nIncreasedStorage : 0\n"),"stale editor snapshot overwrote newer decision");
    Require(State::Read(path)==latest,"stale editor changed current file");
    unsigned races=0;
    for(unsigned round=0;round<12;++round) {
        const auto expected=State::Read(path);
        std::barrier start(3);std::atomic<unsigned> written=0,errors=0;
        const auto writer=[&](const char* data){start.arrive_and_wait();try{if(State::WriteExpected(path,expected,data))++written;}catch(...){++errors;}};
        const auto first="DyeGoldQuests : 0\nIncreasedStorage : 0\n; pass "+std::to_string(round)+"a\n";
        const auto second="DyeGoldQuests : 0\nIncreasedStorage : 0\n; pass "+std::to_string(round)+"b\n";
        std::thread a(writer,first.c_str()),b(writer,second.c_str());start.arrive_and_wait();a.join();b.join();
        Require(errors==0&&written==1,"competing first-party writers both won or failed");
        const auto contents=State::Read(path).Contents;
        Require(contents==first||contents==second,"atomic replacement left partial contents");++races;
    }
    bool refused=false;try{(void)State::Read(path.parent_path());}catch(...){refused=true;}
    Require(refused,"unreadable configuration must not look like empty enable-all list");
    std::cout<<Checks<<" mod-order hardening checks; "<<races<<" writer races passed.\n";
    return 0;
}
int main(int argc,char** argv){try{return Run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
