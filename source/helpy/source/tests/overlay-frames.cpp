#include "HelpyOverlayFrames.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Overlay frame ownership regression");}
int main(){using namespace PS::HelpyOverlayFrames;Page page;page.GamePid=7;page.AppPid=9;page.Width=1280;page.Height=720;
    Check(Shape(page,7,9)&&!Shape(page,8,9));page.Width=MaxWidth+1;Check(!Shape(page,7,9));page.Width=1280;
    Check(Writable(page,0)==0);Check(page.Owner[0]==1);page.Owner[0]=2;page.Reading=0;page.Published=1;page.Consumed[2]=4;Check(Writable(page,3)==-1&&Writable(page,4)==2);page.Owner[2]=0;
    page.Ready[1]=8;Check(Latest(page,7)==0&&Latest(page,8)==1);Check(page.Owner[1]==2);Presented(page,1,9);Check(page.Reading==1&&page.Owner[0]==0&&page.Consumed[1]==9);
    page.Published=99;Check(Latest(page,8)==1);page.Published=2;page.Owner[2]=1;Check(Latest(page,100)==1); // Producer owns the next frame: keep the displayed frame.
    page.Visible=1;page.Heartbeat=100;Check(Alive(page,101)&&!Alive(page,99)&&!Alive(page,1601));
    Check(CapturesInput(page,101));page.Visible=2;Check(Alive(page,101)&&!CapturesInput(page,101));page.Visible=3;Check(!Alive(page,101)&&!CapturesInput(page,101));page.Visible=1;
    for(unsigned i=0;i<InputCapacity;++i)Check(Push(page,{InputKind::Character,i}));Check(!Push(page,{}));Check(page.Visible==0&&page.InputOverflow==1);
    Input input;for(unsigned i=0;i<InputCapacity;++i){Check(Pop(page,input));Check(input.Code==i);}Check(!Pop(page,input));
    Page pending,target;
    Check(WindowInput(WM_LBUTTONUP,0,MAKELPARAM(12,34),input)&&input.Kind==InputKind::Button&&input.Code==WM_LBUTTONUP&&input.X==12&&input.Y==34);
    Check(WindowInput(WM_KEYUP,VK_BACK,LPARAM(1u<<31),input)&&input.Kind==InputKind::Key&&input.Flags==(1u<<31));
    Check(WindowInput(WM_CHAR,'f',0,input)&&input.Kind==InputKind::Character&&input.Code=='f');
    Check(!WindowInput(WM_PAINT,0,0,input));
    Check(Push(pending,{InputKind::Character,'f'}));Check(Push(pending,{InputKind::Key,VK_BACK,0,0,0}));
    Check(Push(pending,{InputKind::Key,VK_BACK,0,0,1u<<31}));Check(Push(pending,{InputKind::Character,'u'}));
    for(unsigned i=0;i<InputCapacity;++i)Check(Push(target,{InputKind::Character,'x'}));
    ForwardInput(pending,target);Check(pending.InputRead==0&&target.InputOverflow==0);
    Check(Pop(target,input));ForwardInput(pending,target);Check(pending.InputRead==1);
    while(Pop(target,input)){}ForwardInput(pending,target);
    Check(Pop(target,input)&&input.Kind==InputKind::Key&&input.Code==VK_BACK&&input.Flags==0);
    Check(Pop(target,input)&&input.Kind==InputKind::Key&&input.Code==VK_BACK&&input.Flags==(1u<<31));
    Check(Pop(target,input)&&input.Kind==InputKind::Character&&input.Code=='u'&&!Pop(target,input));
    for(unsigned i=0;i<InputCapacity;++i)Check(Push(pending,{InputKind::Character,'z'}));Check(!Push(pending,{}));
    target.Visible=1;ForwardInput(pending,target);Check(target.Visible==0&&target.InputOverflow==1&&pending.InputRead==pending.InputWrite);
}
