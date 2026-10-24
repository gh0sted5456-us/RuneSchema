#include "Core/NativeInventoryMarkInput.h"
#include "Core/LegacyInventoryFavorites.h"
#include <cassert>
#include <thread>
int main(){
    using Input=PS::NativeInventoryMarkInput;using namespace std::chrono_literals;
    Input input;const auto now=Input::Clock::now();
    PS::NativeInventoryMarkPoll poll;
    assert(poll.Due(now,false,false,false));
    assert(!poll.Due(now+1ms,false,false,false));
    assert(poll.Due(now+2ms,true,false,false)); // Never throttle a press/release edge.
    assert(poll.Due(now+3ms,false,false,false));
    assert(poll.Due(now+4ms,false,true,false));
    assert(poll.Due(now+5ms,false,false,true));
    assert(!poll.Due(now+37ms,false,false,false));
    assert(poll.Due(now+38ms,false,false,false));
    input.Push(4,0,now);assert(input.Take(now).empty());
    input.Publish("character:item:revision",now);
    input.Push(4,0,now+1ms);input.Push(0x75,0,now+2ms);
    const auto clicks=input.Take(now+3ms);assert(clicks.size()==2&&clicks[0].Target=="character:item:revision"&&clicks[1].Key==0x75);
    assert(input.Take(now+4ms).empty());
    input.Push(4,0,now+5ms);input.Publish("different item",now+6ms);
    assert(input.Take(now+7ms).at(0).Target=="character:item:revision");
    input.Push(4,0,now+8ms);input.Reset();assert(input.Take(now+9ms).empty());
    input.Publish("",now);input.Push(0x75,3,now+1ms);assert(input.Take(now+2ms).at(0).Modifiers==3);
    input.Push(4,0,now+3ms);assert(input.Take(now+254ms).empty());
    input.Push(4,0,now+251ms);assert(input.Take(now+252ms).empty());
    input.Publish("valid",now);input.Push(1,0,now);input.Push(4,4,now);assert(input.Take(now).empty());
    for(int n=0;n<100;++n)input.Push(4,0,now);assert(input.Take(now).size()==16);
    input.Publish("threaded",Input::Clock::now());
    std::thread producer([&]{for(int n=0;n<100;++n)input.Push(4,0);});
    for(int n=0;n<100;++n)input.Publish("threaded");producer.join();assert(input.Take().size()<=16);
    assert(Input::Bindable(0x75)&&Input::Bindable(4)&&Input::Bindable(0x41)&&!Input::Bindable(0x10));
    assert(Input::Mouse(0)&&Input::Mouse(4)&&!Input::Mouse(0x75));
    using Controls=PS::NativeInventoryMarkControls;using Action=Controls::Action;
    Controls controls;assert(controls.Key==0x75&&controls.Mouse==4);
    const auto handle=[&](int key,int modifiers,auto at){return controls.Handle({key,modifiers,"item",at},at);};
    assert(handle(4,0,now)==Action::Cycle&&handle(0x75,0,now)==Action::Cycle);
    assert(handle(4,1,now)==Action::None);
    assert(handle(0x75,1,now)==Action::Changed&&controls.Mouse==2);
    assert(handle(0x75,1,now)==Action::Changed&&controls.Mouse==0&&handle(4,0,now)==Action::None);
    assert(handle(0x75,1,now)==Action::Changed&&controls.Mouse==4);
    assert(handle(0x75,3,now)==Action::Capture);
    assert(handle(4,0,now+10ms)==Action::None);
    assert(handle(0x41,0,now+400ms)==Action::Changed&&controls.Key==0x41);
    assert(handle(0x75,0,now+500ms)==Action::None&&handle(0x41,0,now+500ms)==Action::Cycle);
    assert(Controls::Parse(controls.Json()).Json()==controls.Json());
    assert(handle(0x75,3,now)==Action::Capture&&handle(0x1b,0,now+1ms)==Action::Cancel&&controls.Key==0x41);
    assert(handle(0x75,3,now)==Action::Capture&&handle(0x42,0,now+9s)==Action::None&&controls.Key==0x41);
    for(const auto& invalid:{nlohmann::json{{"Version",1},{"Key",1},{"Mouse",4}},nlohmann::json{{"Version",1},{"Key",0x75},{"Mouse",3}},nlohmann::json{{"Version",1},{"Key",UINT64_MAX},{"Mouse",4}}}){
        bool rejected=false;try{Controls::Parse(invalid);}catch(...){rejected=true;}assert(rejected);
    }
    namespace Legacy=PS::LegacyInventoryFavorites;
    const auto guid=Legacy::Key({1,2,3,0xabcdef});assert(guid=="00000001000000020000000300ABCDEF");
    assert(Legacy::Parse("  "+guid+"\r\n"+guid+"\n\t").size()==1);
    assert(Legacy::Parse("00000001000000020000000300abcdef").contains(guid)&&Legacy::Parse("").empty());
    for(const auto& bad:{std::string("broken"),std::string(32,'0'),std::string(32,'G'),std::string(1024*1024+1,'A')}){
        bool rejected=false;try{Legacy::Parse(bad);}catch(...){rejected=true;}assert(rejected);
    }
}
