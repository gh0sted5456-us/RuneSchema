#include "Generator/HelpyPerformance.h"
#include "Generator/HelpyCoordinates.h"
#include "Generator/HelpyStartupProgress.h"
#include "Core/PlayerCoordinates.h"
#include "../../../raw/include/Runtime/GameFrameRate.h"
#include <stdexcept>
#include <thread>
#include <atomic>

static void Check(bool value){if(!value)throw std::runtime_error("Frame-rate sampling regression");}
int main(){
    using namespace std::chrono_literals;
    using namespace PS;
    FrameRate counter;const auto start=FrameRate::Clock::time_point{};
    Check(!counter.Sample(start));
    for(int frame=1;frame<60;++frame)Check(!counter.Sample(start+std::chrono::nanoseconds(frame*1000000000LL/60)));
    Check(std::abs(*counter.Sample(start+1s)-60)<.01);
    Check(!counter.Sample(start+4s)); // A hidden window or suspended game starts a fresh sample.
    Check(!counter.Sample(start+4500ms));Check(std::abs(*counter.Sample(start+5s)-2)<.01);
    Check(!counter.Sample(start)); // Clock discontinuities cannot underflow the sample.
    GameFrameRate::UpdatedMs=0;Check(!GameFrameRate::Available());
    GameFrameRate::UpdatedMs=GameFrameRate::Stamp(FrameRate::Clock::now())-4000;Check(!GameFrameRate::Available());
    GameFrameRate::UpdatedMs=GameFrameRate::Stamp(FrameRate::Clock::now());Check(GameFrameRate::Available());
    unsigned reads=0;
    const auto read=[&](const nlohmann::json& request,nlohmann::json& reply){Check(request.at("Action")=="FrameRate");++reads;reply={{"Available",true},{"Fps",61.0}};return true;};
    HelpyPerformance::Poll(false,500,read);Check(reads==0);
    for(int i=0;i<50;++i)HelpyPerformance::Poll(true,500,read);
    Check(reads==1&&HelpyPerformance::GameFps==61.0);
    Check(HelpyPerformance::Label(false).find("Game 61")!=std::string::npos);
    Check(HelpyPerformance::Label(true).find("Server tick 61")!=std::string::npos);
    HelpyPerformance::Next={};HelpyPerformance::Poll(true,1000,{});Check(HelpyPerformance::GameFps==0);
    HelpyPerformance::Next={};HelpyPerformance::Poll(true,1000,[](const auto&,auto& reply){reply={{"Available",true},{"Fps","invalid"}};return true;});Check(HelpyPerformance::GameFps==0);
    HelpyPerformance::Poll(false,1000,read);Check(HelpyPerformance::UiFps==0&&reads==1);
    HelpyPerformance::PollGame(false,1000,read);
    HelpyPerformance::PollGame(true,500,read);Check(reads==2&&HelpyPerformance::GameLabel()=="Game 61 FPS");
    for(int i=0;i<50;++i){HelpyPerformance::Poll(false,500,read);HelpyPerformance::PollGame(true,500,read);}Check(reads==2);
    for(int i=0;i<50;++i)HelpyPerformance::PollGame(false,500,read);Check(reads==2&&HelpyPerformance::GameFps==0);
    PlayerCoordinates::Store positions;Check(!positions.Read(1).at("Available").get<bool>());Check(positions.TakeRequest()&&!positions.TakeRequest());
    positions.Publish(1,std::array<double,3>{123,-456,789});Check(positions.Read(1).at("Location")[1]==-456);
    Check(!positions.Read(1).contains("Rotation"));
    positions.Publish(1,std::array<double,3>{123,-456,789},std::array<double,3>{-12.5,90,7});Check(positions.Read(1).at("Rotation")==nlohmann::json::array({-12.5,90,7}));
    positions.Publish(1,std::array<double,3>{123,-456,789},std::array<double,3>{NAN,90,7});Check(positions.Read(1).at("Available").get<bool>()&&!positions.Read(1).contains("Rotation"));
    Check(!positions.Read(2).at("Available").get<bool>());
    positions.Publish(1,std::array<double,3>{NAN,1,2});Check(!positions.Read(1).at("Available").get<bool>());
    std::atomic<unsigned> coordinateReads=0;
    const auto coordinates=[&](const nlohmann::json& request,nlohmann::json& reply){Check(request.at("Action")=="LocalCoordinates");++coordinateReads;reply={{"Available",true},{"Location",{123,-456,789}}};return true;};
    HelpyCoordinates::Poll(false,123,coordinates);Check(coordinateReads==0&&HelpyCoordinates::Label.empty());
    const auto deadline=std::chrono::steady_clock::now()+1s;
    while(HelpyCoordinates::Label.empty()){HelpyCoordinates::Poll(true,123,coordinates);Check(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(1ms);}
    Check(HelpyCoordinates::Label=="X 123   Y -456   Z 789");for(int i=0;i<50;++i)HelpyCoordinates::Poll(true,123,coordinates);Check(coordinateReads==1);
    HelpyCoordinates::Poll(false,123,coordinates);Check(HelpyCoordinates::Label.empty());HelpyCoordinates::Shutdown();
    const auto sample=[&](nlohmann::json reply){
        HelpyCoordinates::Next={};const auto expires=std::chrono::steady_clock::now()+1s;
        const auto read=[reply](const auto&,auto& result){result=reply;return true;};
        HelpyCoordinates::Poll(true,123,read);
        while(HelpyCoordinates::Work.valid()){Check(std::chrono::steady_clock::now()<expires);std::this_thread::sleep_for(1ms);HelpyCoordinates::Poll(true,123,read);}
    };
    sample({{"Available",true},{"Location",{123,-456,789}},{"Rotation",{-12.5,90,7}}});
    Check(HelpyCoordinates::Label=="X 123   Y -456   Z 789   |   Pitch -12.5   Yaw 90.0");
    for(const auto& invalid:nlohmann::json::array({nlohmann::json::array({1,2}),nlohmann::json::array({"bad",90,0}),nlohmann::json::array({INFINITY,90,0})})){
        sample({{"Available",true},{"Location",{123,-456,789}},{"Rotation",invalid}});Check(HelpyCoordinates::Label=="X 123   Y -456   Z 789");
    }
    sample({{"Available",false}});Check(HelpyCoordinates::Label.empty());HelpyCoordinates::Shutdown();
    HelpyStartupProgress::Finished=false;
    Check(HelpyStartupProgress::Presentation(true,false,false,false,true));Check(!HelpyStartupProgress::Presentation(true,false,false,false,false));Check(!HelpyStartupProgress::Presentation(true,true,true,true,true));
    HelpyStartupProgress::SetTips(nlohmann::json::array({{{"Name","Example"},{"Enabled",true},{"Present",true},{"Metadata",{{"tips",{"First tip","Second tip"}}}}},{{"Name","Disabled"},{"Enabled",false},{"Present",true},{"Metadata",{{"tips",{"Hidden"}}}}}}));
    Check(HelpyStartupProgress::Tip(0)=="Example: First tip"&&HelpyStartupProgress::Tip(8)=="Example: Second tip"&&HelpyStartupProgress::Tip(16)=="Example: First tip");
    HelpyStartupProgress::Apply({{"Available",true},{"Complete",false},{"FrontendReady",true},{"Checkpoints",nlohmann::json::array({{{"Stage","data registrar begin"}}})}});
    Check(!HelpyStartupProgress::Finished&&HelpyStartupProgress::Stage=="Registering game content");
    HelpyStartupProgress::Apply({{"Complete",true},{"FrontendReady",false}});Check(!HelpyStartupProgress::Finished&&HelpyStartupProgress::Stage=="Waiting for the game menu");
    HelpyStartupProgress::Apply({{"Complete",true},{"FrontendReady",true}});Check(HelpyStartupProgress::Finished);
    std::atomic<unsigned> startupReads=0;
    const auto startup=[&](const nlohmann::json& request,nlohmann::json& reply){Check(request.at("Action")=="StartupStats");++startupReads;reply={{"Complete",true},{"FrontendReady",false}};return true;};
    const auto startupDeadline=std::chrono::steady_clock::now()+1s;
    HelpyStartupProgress::Poll(true,42,startup);while(HelpyStartupProgress::Work.valid()){HelpyStartupProgress::Poll(true,42,startup);Check(std::chrono::steady_clock::now()<startupDeadline);std::this_thread::sleep_for(1ms);}
    Check(HelpyStartupProgress::Visible()&&!HelpyStartupProgress::Finished);for(int i=0;i<50;++i)HelpyStartupProgress::Poll(true,42,startup);Check(startupReads==1);
    HelpyStartupProgress::Poll(false,42,startup);Check(!HelpyStartupProgress::Visible());HelpyStartupProgress::Shutdown();
    HelpyStartupProgress::SetTips(nlohmann::json::array());Check(HelpyStartupProgress::Tip(0).empty());
}
