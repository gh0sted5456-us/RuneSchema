#include "Generator/HelpyGrantBatch.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

static void Require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
static std::string Read(const std::filesystem::path& path){std::ifstream f(path);Require(bool(f),"contract input opens");return {std::istreambuf_iterator<char>(f),{}};}
template<class Fn>static void Reject(Fn fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid batch rejected before mutation");}
int main(int argc,char** argv){
    Require(argc==3,"raw and standalone source roots supplied");
    const std::filesystem::path raw=argv[1],helpy=argv[2];
    for(const auto* file:{"src/Generator/InGameQuickMenu.cpp","src/Generator/QuickMenuInputAdapter.inl",
        "include/Generator/InGameQuickMenu.h","include/Generator/QuickMenuInputSafety.h",
        "include/Generator/HelpyHotkeys.h","include/Generator/QuickMenuUI.h","include/Generator/QuickMenuSpawnRequest.h"})
        Require(!std::filesystem::exists(raw/file),"legacy canvas source must remain removed");
    Require(!std::filesystem::exists(helpy/"source/include/Generator/HelpyHotkeys.h"),"standalone has no legacy hotkey registry");
    const auto plugin=Read(helpy/"source/plugins/helpy/HelpyPlugin.cpp"),window=Read(helpy/"source/src/Generator/HelpyStandaloneWindow.cpp");
    Require(plugin.find("std::make_unique<PS::HelpyStandaloneWindow>")!=plugin.npos,"plugin starts the standalone window");
    Require(plugin.find("state->Window->Shutdown()")!=plugin.npos,"plugin shuts down the standalone window");
    Require(window.find("HelpySettings::Hotkey.load()")!=window.npos&&window.find("GetAsyncKeyState(key)")!=window.npos,"standalone polls the configured shortcut");
    Require(window.find("GetAsyncKeyState(VK_F2)")==window.npos&&window.find("GetAsyncKeyState(VK_F5)")==window.npos,"no hardcoded toggle or theme hotkey");
    for(const auto* forbidden:{"InGameQuickMenu","PostRenderThunk","SetIgnoreMoveInput","SetIgnoreLookInput","SetWindowsHookEx","VK_F7"}){
        Require(plugin.find(forbidden)==plugin.npos,"plugin must not install legacy viewport/input hooks");
        Require(window.find(forbidden)==window.npos,"desktop window must not install legacy viewport/input hooks");
    }
    for(const auto& file:{raw/"include/Runtime/HelpySettings.h",helpy/"source/include/Runtime/HelpySettings.h"}){
        const auto settings=Read(file);
        Require(settings.find("inline bool SetKey(")==settings.npos,"legacy key writer removed");
        Require(settings.find("HelpyHotkeys")==settings.npos,"settings cannot re-enable legacy hotkeys");
        Require(settings.find("Document.value(\"enabled\",true)")!=settings.npos,"existing plugin enable setting preserved");
    }
    using namespace PS::HelpyGrants;
    const std::vector<Grant> grants{{"/One",1},{"/Two",2},{"/Three",3}};int calls=0;
    auto result=ExecuteBatch(grants,[&](const Grant&){++calls;});
    Require(calls==3&&result.size()==3,"complete cart delivers once per item");
    calls=0;result=ExecuteBatch(grants,[&](const Grant&){if(++calls==2)throw std::runtime_error("capacity");});
    Require(calls==2&&result[0].state==BatchRow::State::Confirmed&&result[1].state==BatchRow::State::Unconfirmed
        &&result[2].state==BatchRow::State::NotAttempted,"partial cart stops without retry or false rollback");
    calls=0;result=ExecuteBatch(grants,[&](const Grant&){++calls;throw 42;});
    Require(calls==1&&result[0].state==BatchRow::State::Unconfirmed&&result[1].state==BatchRow::State::NotAttempted,"unknown errors stop the batch too");
    calls=0;result=ExecuteBatch(grants,[&](const Grant&){if(++calls==2)throw Rejected("Item no longer mounted");});
    Require(calls==3&&result[0].state==BatchRow::State::Confirmed&&result[1].state==BatchRow::State::Rejected&&result[2].state==BatchRow::State::Confirmed,
        "definite pre-write rejection does not block other cart items");
    Reject([]{ValidateGrants({});});Reject([]{ValidateGrants({{"bad",1}});});
    Reject([]{ValidateGrants({{"/One",1},{"/One",2}});});Reject([]{ValidateGrants({{"/One",0}});});
    Reject([]{ValidateGrants({{"/One",10001}});});Reject([]{ValidateGrants({{std::string(2049,'/'),1}});});
    std::vector<Grant> maximum;for(int i=0;i<MaxSelection;++i)maximum.push_back({"/Item"+std::to_string(i),10000});
    ValidateGrants(maximum);maximum.push_back({"/Extra",1});Reject([&]{ValidateGrants(maximum);});
    std::cout<<"Standalone-only lifecycle and shared cart contracts passed.\n";
}
