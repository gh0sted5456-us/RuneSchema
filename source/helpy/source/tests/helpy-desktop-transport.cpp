#include "HelpyDesktopTransport.h"
#include <iostream>
#include <cassert>
#include <future>
using namespace PS::HelpyDesktop;
int main()try{
    const auto root=std::filesystem::temp_directory_path()/("HelpyDesktopPipe-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);std::atomic<int> mutations=0,disconnected=0;
    Server server;assert(server.Start(root,[&](const Json& request){
        if(request.value("Op",std::string{})=="Mutate"){
            if(request.value("Capacity",0)<128)return Json{{"Code",5},{"Size",128}};
            ++mutations;
        }return Json{{"Code",0}};
    },[&]{++disconnected;}));
    Client wrong;assert(!wrong.Connect(root/"different",false));
    Client rejected;assert(!rejected.Connect(root,true)); // The test executable is outside its assigned game folder.
    Client client;for(int i=0;i<100&&!client.Connect(root,false);++i)Sleep(10);
    if(client.Pid!=GetCurrentProcessId())std::cerr<<"Pipe client PID="<<client.Pid<<", server PID="<<GetCurrentProcessId()<<", Win32 error="<<GetLastError()<<'\n';
    assert(client.Pid==GetCurrentProcessId());
    assert(client.Notifications());const auto stamp=client.LastExchangeMs.load();assert(!client.WaitForUpdates(20)&&client.LastExchangeMs==stamp);
    Json reply;assert(client.Exchange({{"Version",99},{"Op","Mutate"},{"Capacity",0}},reply)&&reply["Code"]==5&&mutations==0);
    assert(client.Exchange({{"Op","Mutate"},{"Capacity",128}},reply)&&mutations==1);
    server.Push(R"({"Kind":"result","Payload":{"Status":"ready"}})");
    assert(client.Exchange({{"Version","future-label"},{"Op","Poll"}},reply)&&reply["Events"].size()==1);
    assert(!client.WaitForUpdates(20));
    auto waiting=std::async(std::launch::async,[&]{return client.WaitForUpdates(1000);});Sleep(10);
    const auto began=std::chrono::steady_clock::now();assert(client.Exchange({{"Op","Poll"}},reply));assert(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(300));
    server.Push(R"({"Kind":"query","Payload":{}})");assert(waiting.get());assert(client.Exchange({{"Op","Poll"}},reply)&&reply["Events"].size()==1);
    for(unsigned i=0;i<40;++i)server.Push(Json{{"Sequence",i}}.dump());
    unsigned received=0;for(int batch=0;batch<3;++batch){assert(client.WaitForUpdates(100));assert(client.Exchange({{"Op","Poll"}},reply));assert(reply["Events"].size()<17);for(const auto& event:reply["Events"])assert(event["Sequence"]==received++);}assert(received==40&&!client.WaitForUpdates(20));
    for(int i=0;i<2;++i)server.Push(Json{{"Payload",std::string(600*1024,'x')}}.dump());
    assert(client.Exchange({{"Op","Poll"}},reply)&&reply["Events"].size()==1&&client.WaitForUpdates(20));assert(client.Exchange({{"Op","Poll"}},reply)&&reply["Events"].size()==1);
    const auto cancelled=CreateEventW(nullptr,TRUE,TRUE,nullptr);assert(cancelled&&!client.WaitForUpdates(1000,cancelled));CloseHandle(cancelled);
    client.Disconnect();for(int i=0;i<100&&!disconnected;++i)Sleep(10);assert(disconnected>0);
    for(int i=0;i<100&&!client.Connect(root,false);++i)Sleep(10);
    assert(client.Exchange({{"Op","Poll"}},reply)&&reply["Events"].empty()&&mutations==1);
    client.Disconnect();server.Stop();std::filesystem::remove(root);
    bool rejectedDepth=false;try{Parse(std::string(66,'[')+"0"+std::string(66,']'));}catch(...){rejectedDepth=true;}assert(rejectedDepth);
    std::cout<<"Desktop routing, version metadata, one-shot mutation, push, reconnect and bounds passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
