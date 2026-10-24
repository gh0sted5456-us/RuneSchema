#include "Core/HelpyOverlayGeometry.h"
#include <cassert>
#include <type_traits>

// Deliberately model Win32 LONG as long (distinct from int on the test host).
// The production helper is compiled with these types, not a copied formula.
struct MockPoint { long x,y; };
struct MockRect { long left,top,right,bottom; };
static_assert(!std::is_same_v<long,int>);

int main(){
    using G=PS::HelpyOverlayGeometry;
    const auto bounds=G::Normalize(MockPoint{70,46},MockRect{0,0,1600,900},MockRect{0,0,1920,1080});
    static_assert(std::is_same_v<decltype(bounds.ClientY),int>);
    assert(bounds.ClientX==70&&bounds.ClientY==46&&bounds.ClientWidth==1600);
    assert(G::AvailableWidth(bounds,14)==1572);
    assert(G::MaxHeight(bounds,14)==1006);
    auto xy=G::Place(bounds,390,220,14);
    assert(xy.X==1266&&xy.Y==60);
    const auto offscreen=G::Normalize(MockPoint{-1800,-300},MockRect{0,0,1800,900},MockRect{-1920,0,0,1040});
    auto clipped=G::Place(offscreen,390,300,14);
    assert(clipped.X==-404&&clipped.Y==0);
    assert(G::MaxHeight(offscreen,14)==1026);
    const auto tiny=G::Normalize(MockPoint{0,0},MockRect{0,0,200,120},MockRect{0,0,200,120});
    assert(G::AvailableWidth(tiny,14)==172);
    return 0;
}
