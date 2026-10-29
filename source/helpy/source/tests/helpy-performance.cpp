#include "Generator/HelpyPerformance.h"
#include "../../../raw/include/Runtime/GameFrameRate.h"
#include <stdexcept>

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
}
