// Explicit opt-in live metadata check, deliberately not a CTest test.
#include "Runtime/HelpyUpdates.h"
#include "Core/RuneSchemaUpdateInfo.h"
#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>
int main(int argc,char** argv){
    if(argc!=2||std::string_view(argv[1])!="--public-metadata")return 2;
    const std::string installed="0.7.8.0-alpha";
    if(!PS::HelpyUpdates::Start(installed))return 3;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(40);
    while(PS::HelpyUpdates::Busy()&&std::chrono::steady_clock::now()<deadline){
        PS::HelpyUpdates::Tick(false,false,installed);std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    const auto result=PS::HelpyUpdates::Current();PS::HelpyUpdates::Shutdown();
    std::printf("Main: %s (%s) error=%s\n",result.MainVersion.c_str(),PS::RuneSchemaUpdateInfo::Label(PS::RuneSchemaUpdateInfo::Compare(installed,result.MainVersion)),result.MainError.c_str());
    std::printf("Release: %s (%s) error=%s\n",result.ReleaseVersion.c_str(),PS::RuneSchemaUpdateInfo::Label(PS::RuneSchemaUpdateInfo::Compare(installed,result.ReleaseVersion)),result.ReleaseError.c_str());
    std::printf("Steam update: %s timestamp=%lld error=%s\n",result.GameUpdateTitle.c_str(),static_cast<long long>(result.GameUpdateDate),result.GameUpdateError.c_str());
    if(result.MainVersion.empty()||result.ReleaseVersion.empty()||result.GameUpdateDate<=0)return 4;
    if(PS::HelpyUpdates::Start(installed))return 5;
    std::puts("PASS live public endpoints, comparison, background worker, shutdown and manual rate limit.");return 0;
}
