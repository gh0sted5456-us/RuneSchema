#include "RuneSchemaCompanionSurface.h"
#include <cassert>
int main(){
    namespace S=PS::CompanionSurface;
    S::BindRequest request;request.HostPid=123;strcpy_s(request.Nonce,"0123456789abcdef0123456789abcdef");
    assert(S::Valid(request)&&!S::Name(request.HostPid,request.Nonce).empty());
    request.Nonce[0]='/';assert(!S::Valid(request));request.Nonce[0]='0';
    S::Session session;session.HostPid=123;assert(S::Shape(session,123)&&!S::Shape(session,124));
    S::Frame frame;frame.AppPid=456;frame.Width=960;frame.Height=640;strcpy_s(frame.Nonce,request.Nonce);
    S::Frame read;assert(!S::ReadFrame(session,read));S::Publish(session,frame);assert(S::ReadFrame(session,read)&&read.AppPid==456);
    InterlockedIncrement(&session.FrameSequence);assert(!S::ReadFrame(session,read));
    assert(!S::Alive(session.HostHeartbeat,100));InterlockedExchange64(&session.HostHeartbeat,100);assert(S::Alive(session.HostHeartbeat,100)&&!S::Alive(session.HostHeartbeat,1700));
}
