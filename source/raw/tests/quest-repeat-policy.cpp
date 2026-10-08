#include "Loader/QuestRepeatPolicy.h"
#include <cassert>
using namespace DragonWilds::Quests;
using nlohmann::json;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    assert(!RepeatAvailableAt(ParseRepeat(json::object()),10));
    const auto immediate=ParseRepeat({{"Repeatable",true}});
    assert(RepeatReady(immediate,100,100,100));
    const auto cooldown=ParseRepeat({{"Repeatable",true},{"Repeat",{{"CooldownSeconds",3600}}}});
    assert(!RepeatReady(cooldown,100,3699,100));assert(RepeatReady(cooldown,100,3700,100));
    assert(Rejects([&]{RepeatReady(cooldown,100,100,200);}));
    const auto daily=ParseRepeat({{"Repeatable",true},{"Repeat",{{"Reset",{{"Frequency","Daily"},{"HourUTC",6}}}}}});
    assert(*RepeatAvailableAt(daily,21599)==21600);
    assert(*RepeatAvailableAt(daily,21600)==108000);
    const auto weekly=ParseRepeat({{"Repeatable",true},{"Repeat",{{"Reset",{{"Frequency","Weekly"},{"WeekdayUTC",0}}}}}});
    assert(*RepeatAvailableAt(weekly,0)==345600); // Unix epoch Thursday; next Monday.
    assert(*RepeatAvailableAt(weekly,345600)==950400);
    const auto both=ParseRepeat({{"Repeatable",true},{"Repeat",{{"CooldownSeconds",90000},{"Reset",{{"Frequency","Daily"}}}}}});
    assert(*RepeatAvailableAt(both,100)==90100);
    assert(!RepeatReady(both,100,86400,100));assert(RepeatReady(both,100,90100,100));
    for(const auto& bad:{json{{"Repeatable","true"}},json{{"Repeat",json::object()}},json{{"Repeatable",true},{"Repeat",{{"CooldownSeconds",-1}}}},json{{"Repeatable",true},{"Repeat",{{"Reset",{{"Frequency","Weekly"}}}}}}})
        assert(Rejects([&]{ParseRepeat(bad);}));
    assert(Rejects([&]{RepeatAvailableAt(cooldown,std::numeric_limits<int64_t>::max());}));
}
