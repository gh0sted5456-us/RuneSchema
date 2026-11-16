#include "Core/AchievementToastTimeline.h"
#include <iostream>
#include <stdexcept>

using namespace PS;
namespace T = AchievementToastTimeline;

static void Check(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}

int main() try {
    std::deque<ModAchievements::Unlock> queue{{"mods/a|one","mods/a","A","One","",10,1},{"mods/a|two","mods/a","A","Two","",5,2}};
    T::Timeline line;
    Check(!line.At(0).Visible&&!line.Advance(queue,0,false)&&queue.size()==2,"A hidden HUD consumed an award");
    Check(line.Advance(queue,1,true)&&queue.size()==1,"First award did not start");
    const auto at = [&](double elapsed) { return line.At(1 + elapsed); };
    const auto start = at(0);
    Check(start.Offset==1&&start.Alpha==0&&start.Animating,"Award did not rise from below the viewport");
    const auto logo = at(T::Rise+T::Seal*.5);
    Check(logo.Offset==0&&logo.Rune==1&&logo.Brackets==0&&logo.Text==0,"The intact RuneSchema logo did not come first");
    const auto opening = at(T::BreakAt+T::Break*.5);
    Check(opening.Brackets>0&&opening.Brackets<1&&opening.Rune==1&&opening.Text==0,"Brackets did not leave before the R");
    const auto reveal = at(T::RevealAt+T::Reveal*.25);
    Check(reveal.Brackets==1&&reveal.Rune>0&&reveal.Rune<1&&reveal.Text==0,"The R did not fade before the reveal");
    const auto hold = at(T::HoldAt+1);
    Check(!hold.Animating&&hold.Brackets==1&&hold.Rune==0&&hold.Text==1,"The achievement did not hold with a clear title and points");
    const auto fade = at(T::FadeAt+T::Fade*.5);
    Check(fade.Text>0&&fade.Text<1&&fade.Rune==0,"The text did not fade before the R returned");
    const auto returning = at(T::RuneReturnAt+T::RuneReturn*.5);
    Check(returning.Text==0&&returning.Rune>0&&returning.Brackets==1,"The R did not return ahead of the brackets");
    const auto closing = at(T::CloseAt+T::CloseBrackets*.5);
    Check(closing.Brackets>0&&closing.Brackets<1&&closing.Rune==1&&closing.Text==0,"The logo failed to reassemble");
    const auto exit = at(T::ExitAt+T::Exit*.5);
    Check(exit.Brackets==0&&exit.Rune==1&&exit.Offset>0&&exit.Alpha<1,"The assembled logo did not close the popup");
    Check(!at(T::ExitAt+T::Exit+.01).Visible,"The inter-award gap showed stale pixels");
    line.Pause(3);
    Check(line.At(20).Text==1&&!line.Advance(queue,20,true)&&line.Current()->Title=="One","Focus loss skipped an award");
    const double end=1+17+line.Total();
    Check(!line.Advance(queue,end-.01,true)&&line.Advance(queue,end+.01,true)&&line.Current()->Title=="Two"&&queue.empty(),
        "The next award did not wait for the resumed first award");

    T::Timeline still;
    still.Animate=false;
    std::deque<ModAchievements::Unlock> one{{"k","mods/a","A","T","",1,1}};
    still.Advance(one,0,true);
    const auto accessible=still.At(0);
    Check(accessible.Visible&&accessible.Offset==0&&accessible.Text==1&&accessible.Rune==0&&accessible.Brackets==1&&
        !accessible.Animating&&!still.At(T::Hold+.01).Visible,"Reduced motion should show only the achievement text");
    std::cout<<"Gamer Points logo sequence, pause/resume, reduced motion and queue passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    return 1;
}
