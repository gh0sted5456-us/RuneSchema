#pragma once
#include "nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::ModDataRelay {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline constexpr size_t ChunkBytes=1024,MaxBytes=65536,MaxPeers=64,MaxChannels=64,AssemblyBudget=2*1024*1024,SnapshotBudget=2*1024*1024,QueueBudget=4*1024*1024;
inline void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
inline bool Token(const std::string& value,size_t max=193){return !value.empty()&&value.size()<=max&&std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==':';});}
inline uint32_t Crc(const std::string& bytes){uint32_t result=~uint32_t{};for(unsigned char c:bytes){result^=c;for(int bit=0;bit<8;++bit)result=(result>>1)^(0xedb88320u&uint32_t(-int(result&1)));}return ~result;}
inline std::string Hex(const std::string& value){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
inline std::string Unhex(const std::string& value){Require(value.size()<=ChunkBytes*2&&value.size()%2==0,"Relay chunk length is invalid");std::string out;out.reserve(value.size()/2);const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(size_t i=0;i<value.size();i+=2){const int a=digit(value[i]),b=digit(value[i+1]);Require(a>=0&&b>=0,"Relay chunk encoding is invalid");out+=char(a*16+b);}return out;}
struct Channel {std::string Key;uint32_t Version=1;size_t Limit=MaxBytes;bool ClientPublish=false,Broadcast=false,Snapshot=false;bool operator==(const Channel&)const=default;};
inline Channel Declaration(const std::string& owner,const Json& value){
    Require(Token(owner,96)&&owner.find(':')==owner.npos&&value.is_object(),"Relay owner/declaration is invalid");
    for(const auto& [key,_]:value.items())Require(key=="Channel"||key=="Version"||key=="MaxBytes"||key=="ClientPublish"||key=="Broadcast"||key=="Snapshot","Unsupported Relay declaration field");
    for(const auto* field:{"Version","MaxBytes"})if(value.contains(field))Require(value[field].is_number_integer(),"Relay version/size must be integers");
    const auto name=value.at("Channel").get<std::string>();Require(Token(name,96)&&name.find(':')==name.npos,"Relay channel name is invalid");
    Channel out{owner+":"+name,value.value("Version",uint32_t{1}),value.value("MaxBytes",MaxBytes),value.value("ClientPublish",false),value.value("Broadcast",false),value.value("Snapshot",false)};
    Require(out.Version>0&&out.Version<=65535&&out.Limit>0&&out.Limit<=MaxBytes&&(!out.Snapshot||out.Broadcast),"Relay declaration limits/policy are invalid");return out;
}
struct Message {std::string Channel,Sender,Data;uint32_t Version=1;int64_t Sequence=0;uint32_t Checksum=0;};
inline Json Packet(const Message& message,size_t part){const auto total=(message.Data.size()+ChunkBytes-1)/ChunkBytes;Require(part<total,"Relay part is out of bounds");return {{"v",1},{"channel",message.Channel},{"version",message.Version},{"sender",message.Sender},{"sequence",message.Sequence},{"part",part},{"total",total},{"bytes",message.Data.size()},{"crc",message.Checksum},{"hex",Hex(message.Data.substr(part*ChunkBytes,ChunkBytes))}};}
struct Assembly {Message Value;size_t Bytes=0,Next=0,Total=0;Clock::time_point Deadline;};
class Assembler {
    std::map<std::string,Assembly> Pending;
    size_t Reserved=0;
public:
    void Clear(){Pending.clear();Reserved=0;}
    void Expire(Clock::time_point now){for(auto at=Pending.begin();at!=Pending.end();)if(at->second.Deadline<=now){Reserved-=at->second.Bytes;at=Pending.erase(at);}else ++at;}
    void Remove(const std::string& sender){for(auto at=Pending.begin();at!=Pending.end();)if(at->second.Value.Sender==sender){Reserved-=at->second.Bytes;at=Pending.erase(at);}else ++at;}
    std::optional<Message> Push(const Channel& route,const std::string& sender,const Json& packet,Clock::time_point now){
        Expire(now);Require(packet.is_object()&&packet.size()<=11&&packet.dump().size()<=4096&&Token(sender),"Relay packet exceeds its bounds");
        Require(packet.at("v")==1&&packet.at("channel")==route.Key&&packet.at("version")==route.Version,"Relay channel/protocol mismatch");
        for(const auto* field:{"v","version","sequence","part","total","bytes","crc"})Require(packet.at(field).is_number_integer()&&packet.at(field)>=0,"Relay integer field is invalid");
        Require(packet.at("crc")<=uint64_t{0xffffffff},"Relay checksum field is invalid");
        const auto sequence=packet.at("sequence").get<int64_t>();const auto part=packet.at("part").get<size_t>(),total=packet.at("total").get<size_t>(),bytes=packet.at("bytes").get<size_t>();
        const auto crc=packet.at("crc").get<uint32_t>();const auto chunk=Unhex(packet.at("hex").get<std::string>());
        Require(sequence>0&&bytes>0&&bytes<=route.Limit&&total==(bytes+ChunkBytes-1)/ChunkBytes&&part<total
            &&chunk.size()==std::min(ChunkBytes,bytes-part*ChunkBytes),"Relay chunk metadata is invalid");
        const auto key=sender+"/"+route.Key;auto at=Pending.find(key);
        if(at!=Pending.end()&&part==0&&sequence>at->second.Value.Sequence){Reserved-=at->second.Bytes;Pending.erase(at);at=Pending.end();}
        if(at==Pending.end()){
            Require(part==0&&Pending.size()<256&&bytes<=AssemblyBudget-Reserved,"Relay assembly capacity/order exceeded");
            size_t count=0;for(const auto& [_,row]:Pending)if(row.Value.Sender==sender)++count;Require(count<4,"Too many incomplete transfers for this sender");
            at=Pending.emplace(key,Assembly{{route.Key,sender,{},route.Version,sequence,crc},bytes,0,total,now+std::chrono::seconds(30)}).first;Reserved+=bytes;at->second.Value.Data.reserve(bytes);
        }
        auto& assembly=at->second;
        Require(assembly.Value.Sequence==sequence&&assembly.Value.Checksum==crc&&assembly.Bytes==bytes&&assembly.Total==total&&assembly.Next==part,"Relay transfer is interleaved, duplicated or out of order");
        assembly.Value.Data+=chunk;++assembly.Next;if(assembly.Next<total)return {};
        auto value=std::move(assembly.Value);Reserved-=assembly.Bytes;Pending.erase(at);Require(Crc(value.Data)==crc,"Relay checksum mismatch");return value;
    }
};
class Server {
    struct Peer {std::map<std::string,uint32_t> Subscriptions;std::map<std::string,int64_t> Sequences;Clock::time_point Window{};size_t Packets=0,Bytes=0;};
    struct Delivery {std::string Peer;std::shared_ptr<const Message> Value;size_t Part=0;};
    std::map<std::string,Channel> Channels;
    std::map<std::string,Peer> Peers;
    std::map<std::string,std::shared_ptr<const Message>> Snapshots;
    std::deque<Delivery> Queue;size_t QueuedBytes=0,SnapshotBytes=0;
    Assembler Incoming;
    Peer& GetPeer(const std::string& key){Require(Token(key),"Relay peer identity is invalid");if(!Peers.contains(key))Require(Peers.size()<MaxPeers,"Relay peer capacity exceeded");return Peers[key];}
    void Enqueue(const std::vector<std::pair<std::string,std::shared_ptr<const Message>>>& messages){
        size_t bytes=0;for(const auto& [_,value]:messages){Require(value->Data.size()<=QueueBudget-bytes,"Relay queue capacity exceeded");bytes+=value->Data.size();}
        Require(bytes<=QueueBudget-QueuedBytes&&messages.size()<=1024-Queue.size(),"Relay queue capacity exceeded");
        for(const auto& [peer,value]:messages)Queue.push_back({peer,value,0});QueuedBytes+=bytes;
    }
public:
    void Reset(){Peers.clear();Snapshots.clear();Queue.clear();Incoming.Clear();QueuedBytes=SnapshotBytes=0;}
    void Configure(const std::vector<Channel>& channels){std::map<std::string,Channel> candidate;Require(channels.size()<=MaxChannels,"Too many relay channels");for(const auto& channel:channels){auto [at,inserted]=candidate.emplace(channel.Key,channel);Require(inserted||at->second==channel,"Conflicting relay channel registrations");}if(candidate!=Channels){Reset();Channels=std::move(candidate);}}
    const Channel& Route(const std::string& key)const{const auto at=Channels.find(key);Require(at!=Channels.end(),"Relay channel is not registered by this server");return at->second;}
    void Subscribe(const std::string& peer,const std::string& channel,uint32_t version,bool resync=false){
        const auto& route=Route(channel);Require(version==route.Version,"Relay subscription version mismatch");auto& client=GetPeer(peer);if(client.Subscriptions.contains(channel)&&!resync)return;
        std::vector<std::pair<std::string,std::shared_ptr<const Message>>> replay;for(const auto& [_,value]:Snapshots)if(value->Channel==channel)replay.emplace_back(peer,value);
        Enqueue(replay);client.Subscriptions[channel]=version;
    }
    void Remove(const std::string& peer){Peers.erase(peer);Incoming.Remove(peer);for(auto at=Snapshots.begin();at!=Snapshots.end();)if(at->second->Sender==peer){SnapshotBytes-=at->second->Data.size();at=Snapshots.erase(at);}else ++at;for(auto at=Queue.begin();at!=Queue.end();)if(at->Peer==peer||at->Value->Sender==peer){QueuedBytes-=at->Value->Data.size();at=Queue.erase(at);}else ++at;}
    std::optional<Message> Receive(const std::string& peer,const Json& packet,bool nativeAuthority,Clock::time_point now){
        const auto& route=Route(packet.at("channel").get<std::string>());Require(nativeAuthority||route.ClientPublish,"Client publishing is disabled for this channel");
        auto& client=GetPeer(peer);Require(client.Subscriptions.contains(route.Key),"Subscribe before publishing mod data");
        if(client.Window.time_since_epoch().count()==0||now-client.Window>=std::chrono::seconds(1)){client.Window=now;client.Packets=client.Bytes=0;}
        const auto wire=packet.dump().size();Require(++client.Packets<=8&&wire<=32768-client.Bytes,"Relay sender rate/byte limit exceeded");client.Bytes+=wire;
        const auto sequence=packet.at("sequence").get<int64_t>();Require(sequence>client.Sequences[route.Key],"Relay sequence was already accepted");
        auto complete=Incoming.Push(route,peer,packet,now);if(!complete)return {};
        auto shared=std::make_shared<const Message>(*complete);std::vector<std::pair<std::string,std::shared_ptr<const Message>>> deliveries;
        if(route.Broadcast)for(const auto& [key,recipient]:Peers)if(recipient.Subscriptions.contains(route.Key))deliveries.emplace_back(key,shared);
        const auto snapshotKey=peer+"/"+route.Key;const auto old=Snapshots.find(snapshotKey);const auto prior=old==Snapshots.end()?0:old->second->Data.size();
        if(route.Snapshot)Require(shared->Data.size()<=SnapshotBudget-(SnapshotBytes-prior),"Relay snapshot capacity exceeded");
        Enqueue(deliveries);if(route.Snapshot){SnapshotBytes=SnapshotBytes-prior+shared->Data.size();Snapshots[snapshotKey]=shared;}client.Sequences[route.Key]=sequence;return complete;
    }
    std::vector<std::pair<std::string,Json>> Drain(size_t maximum,Clock::time_point now){Incoming.Expire(now);std::vector<std::pair<std::string,Json>> out;maximum=std::min(maximum,size_t{8});
        while(!Queue.empty()&&out.size()<maximum){auto& next=Queue.front();out.emplace_back(next.Peer,Packet(*next.Value,next.Part++));if(next.Part*ChunkBytes>=next.Value->Data.size()){QueuedBytes-=next.Value->Data.size();Queue.pop_front();}}
        return out;
    }
    size_t PendingBytes()const{return QueuedBytes;}size_t SavedBytes()const{return SnapshotBytes;}
};
}
