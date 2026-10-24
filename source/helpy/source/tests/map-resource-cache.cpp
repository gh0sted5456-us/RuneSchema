#include "Runtime/HelpyMapCache.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyEnvironment.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
using namespace PS;
static std::string Read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool ok){if(!ok)throw std::runtime_error("Map cache contract failed");}
static void Wait(){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(40);while(HelpyMapCache::Busy()){HelpyMapCache::Poll();if(std::chrono::steady_clock::now()>end)throw std::runtime_error("Map cache worker did not finish");std::this_thread::yield();}}
int main(int argc,char** argv){
    HelpyEnvironment::Current=HelpyEnvironment::Kind::Steam;HelpyEnvironment::GameVersion="++dominion+live-CL-251392";
    const auto folder=HostServices::CacheDirectory()/"map"/"steam";
    if(argc==2&&std::string(argv[1])=="--live"){
        HelpyMapCache::Initialize();Wait();if(!HelpyMapCache::Current().Data){Check(HelpyMapCache::StartInitial());Wait();}
        const auto& live=HelpyMapCache::Current();Check(live.Data&&live.Data->Markers.size()>10000&&!HelpyMapCache::RefreshEligible()&&!HelpyMapCache::Refresh());
        std::cout<<nlohmann::json{{"Markers",live.Data->Markers.size()},{"Categories",live.Data->Categories.size()},{"Built",live.Data->Built},{"Cache",(folder/"markers.json").string()},{"UnchangedBuildFetchBlocked",true}}.dump();HelpyMapCache::Shutdown();return 0;
    }
    const std::string data=R"({"source":"RuneScape: Dragonwilds Wiki","licence":"CC BY-NC-SA 3.0","built":"2026-10-16","groups":[{"id":"mining","label":"Ores and rocks","categories":[{"name":"Iron","points":[[200000,40000]]}]}]})";
    HelpyMapCache::Test(folder,data);const auto good=Read(folder/"markers.json");Check(!good.empty());
    bool rejected=false;try{HelpyMapCache::Test(folder,"{invalid}");}catch(...){rejected=true;}Check(rejected&&Read(folder/"markers.json")==good);
    HelpyMapCache::Initialize();Wait();Check(HelpyMapCache::Current().Data&&HelpyMapCache::Current().Data->Markers.size()==1);
    for(int i=0;i<100;++i){Check(!HelpyMapCache::RefreshEligible()&&!HelpyMapCache::Refresh());HelpyMapCache::Poll();}Check(!HelpyMapCache::Busy()&&Read(folder/"markers.json")==good);
    HelpyMapCache::Current().Filters.Hidden.insert("mining:Iron");HelpyMapCache::SaveFilters();HelpyMapCache::Shutdown();
    Check(Read(folder/"filters.json").find("mining:Iron")!=std::string::npos);
    HelpyMapCache::Current().Filters={};HelpyMapCache::Initialize();Wait();Check(HelpyMapCache::Current().Filters.Hidden.contains("mining:Iron"));
    HelpyEnvironment::GameVersion="Website says 1.0.2 is out";Check(!HelpyMapCache::RefreshEligible());
    HelpyEnvironment::GameVersion="++dominion+live-CL-251393";Check(HelpyMapCache::RefreshEligible());
    HelpyMapCache::Shutdown();
}
