#include "InputBindings.h"
#include <cassert>
#include <set>
using namespace PS::InputBindings;
template<class F>void Reject(F call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
    assert(Text(Parse("shift + ctrl + f2"))=="Ctrl+Shift+F2");assert(Parse("None").empty());
    assert(Primary(Parse("Ctrl+A+B"))=='B');assert(Text(Parse("Num0+NumAdd"))=="Num0+NumAdd");
    for(const auto& key:SpecialKeys)assert(Code(Name(key.Code))==key.Code);
    Reject([]{Parse("Ctrl");});Reject([]{Parse("Ctrl+Control+F2");});Reject([]{Parse("F2+");});Reject([]{Parse("Alt+F4");});Reject([]{Parse("Win+F2");});Reject([]{Parse("Ctrl+Alt+Delete");});
    std::set<int> keys;const auto read=[&](int key){return keys.contains(key);};keys={0x71};assert(Down(Parse("F2"),read));keys.insert(0x11);assert(!Down(Parse("F2"),read)&&Down(Parse("Ctrl+F2"),read));keys.insert(0x5b);assert(!Down(Parse("Ctrl+F2"),read));
    assert(!Overlap(Parse("F2"),Parse("Ctrl+F2")));assert(Overlap(Parse("A+B"),Parse("A+B+C")));assert(!Overlap(Parse("Ctrl+A"),Parse("Shift+A")));
    assert(ParsePad("Guide+West")== (Guide|0x4000));assert(ParsePad(PadText(Guide|Touchpad|0x100))==(Guide|Touchpad|0x100));Reject([]{ParsePad("West+West");});Reject([]{ParsePad("Bogus");});Reject([]{PadText(0x100000);});
    assert(Overlap(Guide,Guide|0x4000));assert(!Overlap(0x20|0x4000,0x20|0x8000));
    Press press;assert(!press.Poll("F2",true,true));assert(!press.Poll("F2",false,true));assert(press.Poll("F2",true,true));assert(!press.Poll("F2",true,true));assert(!press.Poll("F2",true,false));assert(!press.Poll("F2",true,true));assert(!press.Poll("F2",false,true));assert(press.Poll("F2",true,true));assert(!press.Poll("F3",true,true));
}
