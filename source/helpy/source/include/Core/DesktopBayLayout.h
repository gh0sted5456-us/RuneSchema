#pragma once
#include <algorithm>
namespace PS::DesktopBayLayout {
struct Rect {float X=0,Y=0,Width=0,Height=0;};
struct Canvas {Rect Main,Bay;float X=0,Y=0,Width=0,Height=0;};
inline Canvas Arrange(Rect main,Rect bay){
    Canvas result;result.X=std::min(main.X,bay.X);result.Y=std::min(main.Y,bay.Y);
    result.Width=std::max(main.X+main.Width,bay.X+bay.Width)-result.X;
    result.Height=std::max(main.Y+main.Height,bay.Y+bay.Height)-result.Y;
    main.X-=result.X;main.Y-=result.Y;bay.X-=result.X;bay.Y-=result.Y;result.Main=main;result.Bay=bay;return result;
}
}
