#pragma once
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace PS {
// Input callbacks copy only the last game-thread target, never Unreal pointers.
class NativeInventoryMarkInput {
public:
    using Clock=std::chrono::steady_clock;
    struct Event {int Key=0,Modifiers=0;std::string Target;Clock::time_point At;};
    static bool Bindable(int key){return key==2||key==4||key==5||key==6||(key>=0x21&&key<=0x28)||key==0x2d||key==0x2e||(key>=0x41&&key<=0x5a)||(key>=0x60&&key<=0x69)||(key>=0x70&&key<=0x7b);}
    static bool Mouse(int key){return key==0||key==2||key==4||key==5||key==6;}
    void Publish(std::string target,Clock::time_point now=Clock::now()){
        std::lock_guard lock(Gate);Target=std::move(target);Published=now;Active=true;
    }
    void Reset(){std::lock_guard lock(Gate);Active=false;Target.clear();Pending.clear();}
    void Push(int key,int modifiers,Clock::time_point now=Clock::now()){
        std::lock_guard lock(Gate);
        if(!Active||now<Published||now-Published>std::chrono::milliseconds(250)||(!Bindable(key)&&key!=0x1b)||modifiers<0||modifiers>3)return;
        if(Pending.size()>=16)Pending.pop_front();
        Pending.push_back({key,modifiers,Target,now});
    }
    std::vector<Event> Take(Clock::time_point now=Clock::now()){
        std::lock_guard lock(Gate);std::vector<Event> output;
        for(auto& event:Pending)if(now>=event.At&&now-event.At<=std::chrono::milliseconds(250))output.push_back(std::move(event));
        Pending.clear();return output;
    }
private:
    std::mutex Gate;
    std::deque<Event> Pending;
    std::string Target;
    Clock::time_point Published{};
    bool Active=false;
};
inline NativeInventoryMarkInput InventoryMarkInput;
struct NativeInventoryMarkPoll {
    NativeInventoryMarkInput::Clock::time_point Next{};
    bool Down=true;
    bool Due(NativeInventoryMarkInput::Clock::time_point now,bool down,bool input,bool refresh){
        if(now<Next&&down==Down&&!input&&!refresh)return false;
        Down=down;Next=now+std::chrono::milliseconds(33);return true;
    }
};
struct NativeInventoryMarkControls {
    using Clock=NativeInventoryMarkInput::Clock;
    enum class Action {None,Cycle,Changed,Capture,Cancel};
    int Key=0x75,Mouse=4;
    Clock::time_point CaptureUntil{},IgnoreUntil{};
    bool Capturing(Clock::time_point now)const{return now<CaptureUntil;}
    nlohmann::json Json()const{return {{"Version",1},{"Key",Key},{"Mouse",Mouse}};}
    static NativeInventoryMarkControls Parse(const nlohmann::json& value){
        if(!value.is_object()||value.size()!=3||!value.at("Version").is_number_integer()||value.at("Version")!=1||!value.at("Key").is_number_integer()||!value.at("Mouse").is_number_integer())throw std::runtime_error("Invalid inventory marking controls");
        const auto key=value.at("Key").get<int64_t>(),mouse=value.at("Mouse").get<int64_t>();
        if(key<0||key>255||!NativeInventoryMarkInput::Bindable(static_cast<int>(key))||mouse<0||mouse>6||!NativeInventoryMarkInput::Mouse(static_cast<int>(mouse)))throw std::runtime_error("Unsupported inventory marking controls");
        NativeInventoryMarkControls result;result.Key=static_cast<int>(key);result.Mouse=static_cast<int>(mouse);return result;
    }
    Action Handle(const NativeInventoryMarkInput::Event& event,Clock::time_point now){
        if(Capturing(now)){
            if(event.Key==0x1b){CaptureUntil={};return Action::Cancel;}
            if(now<IgnoreUntil||event.Modifiers||!NativeInventoryMarkInput::Bindable(event.Key))return Action::None;
            Key=event.Key;CaptureUntil={};return Action::Changed;
        }
        if(event.Key==0x75&&event.Modifiers==3){IgnoreUntil=now+std::chrono::milliseconds(300);CaptureUntil=now+std::chrono::seconds(8);return Action::Capture;}
        if(event.Key==0x75&&event.Modifiers==1){Mouse=Mouse==4?2:Mouse==2?0:4;return Action::Changed;}
        return !event.Modifiers&&(event.Key==Key||(Mouse&&event.Key==Mouse))?Action::Cycle:Action::None;
    }
};
}
