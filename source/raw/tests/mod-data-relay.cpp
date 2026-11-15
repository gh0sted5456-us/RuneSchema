#include "Core/ModDataRelay.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(v) ((v)?void():throw std::runtime_error(#v))
template<class F>void Reject(F call){bool failed=false;try{call();}catch(...){failed=true;}assert(failed);}
int main()try{
    namespace R=PS::ModDataRelay;using J=R::Json;
    assert(R::Crc("123456789")==0xcbf43926u);assert(R::Unhex(R::Hex(std::string("a\0b",3)))==std::string("a\0b",3));
    const auto route=R::Declaration("RSHC",{{"Channel","appearance"},{"Version",1},{"ClientPublish",true},{"Broadcast",true},{"Snapshot",true}});
    assert(route.Limit==65536);
    const auto bulk=R::Declaration("RSHC",{{"Channel","bulk"},{"MaxBytes",R::MaxBytes},{"ClientPublish",true}});
    R::Message large{bulk.Key,"p",std::string(R::MaxBytes,'z'),1,1,0}; large.Checksum=R::Crc(large.Data);
    assert(large.Checksum==1962557537u);
    R::Assembler largeReceiver; std::optional<R::Message> largeResult; const auto start=R::Clock::now();
    for(size_t part=0;part<(large.Data.size()+1023)/1024;++part) largeResult=largeReceiver.Push(bulk,"p",R::Packet(large,part),start+std::chrono::milliseconds(part*250));
    assert(largeResult&&largeResult->Data==large.Data);
    const auto slowData=std::string(3072,'s');
    const R::Message slow{bulk.Key,"p",slowData,1,2,R::Crc(slowData)};
    assert(!largeReceiver.Push(bulk,"p",R::Packet(slow,0),start,true));
    assert(!largeReceiver.Push(bulk,"p",R::Packet(slow,1),start+std::chrono::seconds(20),true));
    assert(largeReceiver.Push(bulk,"p",R::Packet(slow,2),start+std::chrono::seconds(40),true)->Data==slowData);
    Reject([&]{R::Declaration("RSHC",{{"Channel","too-big"},{"MaxBytes",R::MaxBytes+1}});});
    Reject([&]{R::Declaration("RSHC",{{"Channel","wrapped-version"},{"Version",4294967297ull}});});
    std::vector<R::Channel> many;
    for(size_t i=0;i<R::MaxChannels;++i) many.push_back(R::Declaration("RSHC",{{"Channel","lane"+std::to_string(i)}}));
    R::Server manyServer; manyServer.Configure(many); many.push_back(R::Declaration("RSHC",{{"Channel","overflow"}})); Reject([&]{manyServer.Configure(many);});
    Reject([&]{R::Declaration("RSHC",{{"Channel","../bad"}});});Reject([&]{R::Declaration("RSHC",{{"Channel","ok"},{"MaxBytes",-1}});});
    Reject([&]{R::Declaration("RSHC",{{"Channel","ok"},{"Snapshot",true}});});
    R::Server server;server.Configure({route});Reject([&]{server.Subscribe("player-1","Missing:x",1);});Reject([&]{server.Subscribe("player-1",route.Key,2);});
    server.Subscribe("player-1",route.Key,1);server.Subscribe("player-2",route.Key,1);
    const std::string data=std::string(64000,'x')+"/Game/Parts/Custom.Custom";
    R::Message message{route.Key,"forged-player",data,1,1,R::Crc(data)};const auto now=R::Clock::now();
    std::optional<R::Message> accepted;for(size_t part=0;part<(data.size()+1023)/1024;++part)accepted=server.Receive("player-1",R::Packet(message,part),false,now+std::chrono::milliseconds(part*250));
    assert(accepted&&accepted->Sender=="player-1"&&accepted->Data==data&&server.SavedBytes()==data.size());
    Reject([&]{server.Receive("player-1",R::Packet(message,0),false,now+std::chrono::seconds(20));});
    R::Assembler receiver;std::optional<R::Message> received;size_t packets=0;
    for(int batch=0;batch<50;++batch)for(const auto& [peer,p]:server.Drain(8,now+std::chrono::seconds(21+batch)))if(peer=="player-2"){received=receiver.Push(route,p.at("sender").get<std::string>(),p,now+std::chrono::seconds(21));++packets;}
    assert(received&&received->Data==data&&received->Sender=="player-1"&&packets==(data.size()+1023)/1024&&server.PendingBytes()==0);
    server.Subscribe("late-joiner",route.Key,1);assert(server.PendingBytes()==data.size());server.Subscribe("late-joiner",route.Key,1);assert(server.PendingBytes()==data.size());
    server.Remove("player-1");assert(server.PendingBytes()==0&&server.SavedBytes()==0);
    auto privateRoute=route;privateRoute.Key="RSHC:server-only";privateRoute.ClientPublish=false;privateRoute.Broadcast=false;privateRoute.Snapshot=false;
    server.Configure({privateRoute});server.Subscribe("player-1",privateRoute.Key,1);
    R::Message small{privateRoute.Key,"claimed-server","/Game/Asset.Asset",1,2,0};small.Checksum=R::Crc(small.Data);
    Reject([&]{server.Receive("player-1",R::Packet(small,0),false,now);});assert(server.Receive("player-1",R::Packet(small,0),true,now)->Data==small.Data);assert(!server.PendingBytes()&&!server.SavedBytes());
    R::Assembler broken;auto packet=R::Packet(message,0);packet["bytes"]=R::MaxBytes+1;Reject([&]{broken.Push(route,"p",packet,now);});
    packet=R::Packet(message,1);Reject([&]{broken.Push(route,"p",packet,now);});packet=R::Packet(message,0);assert(!broken.Push(route,"p",packet,now));Reject([&]{broken.Push(route,"p",packet,now);});
    auto fractional=packet;fractional["sequence"]=1.5;Reject([&]{broken.Push(route,"p",fractional,now);});
    auto newer=message;newer.Sequence=2;assert(!broken.Push(route,"p",R::Packet(newer,0),now));broken.Remove("p");
    broken.Expire(now+std::chrono::seconds(31));assert(!broken.Push(route,"p",packet,now+std::chrono::seconds(32)));broken.Remove("p");
    R::Message corrupt{route.Key,"p","broken",1,7,0};Reject([&]{broken.Push(route,"p",R::Packet(corrupt,0),now);});
    auto damaged=R::Packet(slow,1);
    damaged["hex"]=R::Hex(std::string(1024,'x'));
    assert(!broken.Push(bulk,"p",R::Packet(slow,0),now));
    assert(!broken.Push(bulk,"p",damaged,now));
    Reject([&]{broken.Push(bulk,"p",R::Packet(slow,2),now);});
    for(size_t part=0;part<3;++part)received=broken.Push(bulk,"p",R::Packet(slow,part),now);
    assert(received&&received->Data==slowData);
    server.Reset();assert(server.SavedBytes()==0&&server.PendingBytes()==0);
    auto authorityRoute=route;authorityRoute.Key="RuneSchema:server-mods";authorityRoute.ClientPublish=false;
    many.pop_back();manyServer.Configure(many,&authorityRoute);assert(manyServer.Route(authorityRoute.Key)==authorityRoute);
    Reject([&]{manyServer.Configure({authorityRoute},&authorityRoute);});
    server.Configure({authorityRoute});server.Publish(authorityRoute.Key,"first",1);server.Subscribe("new-player",authorityRoute.Key,1);
    auto authorityPackets=server.Drain(4,now);assert(authorityPackets.size()==1&&authorityPackets[0].second.at("sender")=="authority");
    server.Publish(authorityRoute.Key,"second",2);assert(server.PendingBytes()==6&&server.SavedBytes()==6);
    Reject([&]{server.Publish(authorityRoute.Key,"stale",2);});
    R::Message spoof{authorityRoute.Key,"authority","spoof",1,3,R::Crc("spoof")};
    Reject([&]{server.Receive("new-player",R::Packet(spoof,0),false,now);});
    server.Remove("new-player");assert(server.SavedBytes()==6&&server.PendingBytes()==0);
    server.Subscribe("late-player",authorityRoute.Key,1);auto replay=server.Drain(4,now);assert(replay.size()==1&&replay[0].second.at("sequence")==2);
    auto wide=bulk;wide.Broadcast=wide.Snapshot=true;
    R::Server fanout;fanout.Configure({wide,authorityRoute});
    fanout.Subscribe("legacy",wide.Key,1);
    for(int peer=0;peer<80;++peer)fanout.Subscribe("player-"+std::to_string(peer),wide.Key,1,false,R::MaxBytes);
    fanout.Subscribe("quick",authorityRoute.Key,1);
    fanout.Publish(wide.Key,large.Data,1);
    fanout.Subscribe("legacy-late",wide.Key,1);
    assert(fanout.PendingBytes()==80*large.Data.size()&&fanout.PendingStorage()==large.Data.size());
    fanout.Publish(authorityRoute.Key,"short",1);
    std::map<std::string,size_t> nextPart;
    bool deliveredShort=false;
    for(int batch=0;batch<6;++batch){
        const auto packets=fanout.Drain(R::MaxDrainPackets,now);
        assert(packets.size()==R::MaxDrainPackets);
        for(const auto& [peer,p]:packets){
            assert(!peer.starts_with("legacy"));
            assert(p.at("part")==nextPart[peer]++);
            if(peer=="quick")deliveredShort=true;
        }
    }
    assert(deliveredShort&&nextPart.size()==81&&fanout.PendingStorage()==large.Data.size());
    for(int peer=0;peer<80;++peer)fanout.Remove("player-"+std::to_string(peer));
    assert(fanout.PendingStorage()==0&&fanout.PendingBytes()==0);
    fanout.Subscribe("latest",wide.Key,1,false,R::MaxBytes);
    assert(fanout.PendingStorage()==large.Data.size());
    fanout.Reset();assert(!fanout.PendingBytes()&&!fanout.PendingStorage()&&!fanout.SavedBytes());
    std::cout<<"Relay registration, ownership, chunking, CRC, replay, policy, late join and cleanup passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
