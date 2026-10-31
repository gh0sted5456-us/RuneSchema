#include "HelpyOverlayFrames.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Overlay frame ownership regression");}
int main(){using namespace PS::HelpyOverlayFrames;Page page;page.GamePid=7;page.AppPid=9;page.Width=1280;page.Height=720;
    Check(Shape(page,7,9)&&!Shape(page,8,9));page.Width=MaxWidth+1;Check(!Shape(page,7,9));page.Width=1280;
    Check(Writable(page,0)==0);Check(page.Owner[0]==1);page.Owner[0]=2;page.Reading=0;page.Published=1;page.Consumed[2]=4;Check(Writable(page,3)==-1&&Writable(page,4)==2);page.Owner[2]=0;
    page.Ready[1]=8;Check(Latest(page,7)==0&&Latest(page,8)==1);Check(page.Owner[1]==2);Presented(page,1,9);Check(page.Reading==1&&page.Owner[0]==0&&page.Consumed[1]==9);
    page.Published=99;Check(Latest(page,8)==1);page.Published=2;page.Owner[2]=1;Check(Latest(page,100)==1); // Producer owns the next frame: keep the displayed frame.
    page.Visible=1;page.Heartbeat=100;Check(Alive(page,101)&&!Alive(page,99)&&!Alive(page,1601));
    for(unsigned i=0;i<InputCapacity;++i)Check(Push(page,{InputKind::Character,i}));Check(!Push(page,{}));Check(page.Visible==0&&page.InputOverflow==1);
    Input input;for(unsigned i=0;i<InputCapacity;++i){Check(Pop(page,input));Check(input.Code==i);}Check(!Pop(page,input));
}
