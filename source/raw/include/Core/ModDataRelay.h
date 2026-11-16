#pragma once
#include "nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PS::ModDataRelay {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline constexpr size_t ChunkBytes=1024,DefaultBytes=64*1024,LegacyMaxBytes=1024*1024,MaxBytes=4*LegacyMaxBytes;
inline constexpr size_t MaxPeers=128,MaxChannels=512,AssemblyBudget=32*LegacyMaxBytes,SnapshotBudget=32*LegacyMaxBytes;
inline constexpr size_t QueueBudget=64*LegacyMaxBytes,MaxDeliveries=8192,MaxDrainPackets=16,MaxPacketsPerPeer=4;
inline constexpr size_t WirePacketBytes=4096,PacketsPerSecond=16;
inline constexpr auto CapacityReceipt="relay-capacity-v2:";
inline void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
inline bool Token(const std::string& value,size_t max=193){return !value.empty()&&value.size()<=max&&std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==':';});}
inline uint32_t UpdateCrc(uint32_t result,std::string_view bytes){
    static constexpr auto table=[] {std::array<uint32_t,256> rows{};for(uint32_t i=0;i<256;++i){auto v=i;for(int bit=0;bit<8;++bit)v=(v>>1)^(0xedb88320u&uint32_t(-int(v&1)));rows[i]=v;}return rows;}();
    for(unsigned char c:bytes)result=(result>>8)^table[(result^c)&255];
    return result;
}
inline uint32_t Crc(std::string_view bytes){return ~UpdateCrc(~uint32_t{},bytes);}
inline std::string Hex(const std::string& value){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
inline std::string Unhex(const std::string& value){Require(value.size()<=ChunkBytes*2&&value.size()%2==0,"Relay chunk length is invalid");std::string out;out.reserve(value.size()/2);const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(size_t i=0;i<value.size();i+=2){const int a=digit(value[i]),b=digit(value[i+1]);Require(a>=0&&b>=0,"Relay chunk encoding is invalid");out+=char(a*16+b);}return out;}
struct Channel {std::string Key;uint32_t Version=1;size_t Limit=DefaultBytes;bool ClientPublish=false,Broadcast=false,Snapshot=false;bool operator==(const Channel&)const=default;};
inline Channel Declaration(const std::string& owner,const Json& value){
    Require(Token(owner,96)&&owner.find(':')==owner.npos&&value.is_object(),"Relay owner/declaration is invalid");
    for(const auto& [key,_]:value.items())Require(key=="Channel"||key=="Version"||key=="MaxBytes"||key=="ClientPublish"||key=="Broadcast"||key=="Snapshot","Unsupported Relay declaration field");
    for(const auto* field:{"Version","MaxBytes"})if(value.contains(field))Require(value[field].is_number_integer(),"Relay version/size must be integers");
    const auto name=value.at("Channel").get<std::string>();Require(Token(name,96)&&name.find(':')==name.npos,"Relay channel name is invalid");
    const auto version=value.value("Version",uint64_t{1});
    const auto limit=value.value("MaxBytes",uint64_t{DefaultBytes});
    Require(version>0&&version<=65535&&limit>0&&limit<=MaxBytes,"Relay declaration limits are invalid");
    Channel out{owner+":"+name,static_cast<uint32_t>(version),static_cast<size_t>(limit),value.value("ClientPublish",false),value.value("Broadcast",false),value.value("Snapshot",false)};
    Require(!out.Snapshot||out.Broadcast,"Relay snapshots require broadcast");return out;
}
struct Message {std::string Channel,Sender,Data;uint32_t Version=1;int64_t Sequence=0;uint32_t Checksum=0;};
inline Json Packet(const Message& message,size_t part){const auto total=(message.Data.size()+ChunkBytes-1)/ChunkBytes;Require(part<total,"Relay part is out of bounds");return {{"v",1},{"channel",message.Channel},{"version",message.Version},{"sender",message.Sender},{"sequence",message.Sequence},{"part",part},{"total",total},{"bytes",message.Data.size()},{"crc",message.Checksum},{"hex",Hex(message.Data.substr(part*ChunkBytes,ChunkBytes))}};}
struct Assembly {Message Value;size_t Bytes=0,Next=0,Total=0;Clock::time_point Deadline,IdleDeadline;uint32_t RunningCrc=~uint32_t{};};
class Assembler {
    std::map<std::string,Assembly> Pending;
    size_t Reserved=0;
public:
    void Clear(){Pending.clear();Reserved=0;}
    void Expire(Clock::time_point now){for(auto at=Pending.begin();at!=Pending.end();)if(at->second.Deadline<=now||at->second.IdleDeadline<=now){Reserved-=at->second.Bytes;at=Pending.erase(at);}else ++at;}
    void Remove(const std::string& sender){for(auto at=Pending.begin();at!=Pending.end();)if(at->second.Value.Sender==sender){Reserved-=at->second.Bytes;at=Pending.erase(at);}else ++at;}
    std::optional<Message> Push(const Channel& route,const std::string& sender,const Json& packet,Clock::time_point now,bool fairDelivery=false){
        Expire(now);Require(packet.is_object()&&packet.size()<=11&&packet.dump().size()<=WirePacketBytes&&Token(sender),"Relay packet exceeds its bounds");
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
            const auto seconds=bytes<=DefaultBytes?30:30+(bytes+4095)/4096;
            const auto deadline=fairDelivery?Clock::time_point::max():now+std::chrono::seconds(seconds);
            Assembly incoming{{route.Key,sender,{},route.Version,sequence,crc},bytes,0,total,deadline,now+std::chrono::seconds(30)};
            incoming.Value.Data.reserve(bytes);
            at=Pending.emplace(key,std::move(incoming)).first;Reserved+=bytes;
        }
        auto& assembly=at->second;
        Require(assembly.Value.Sequence==sequence&&assembly.Value.Checksum==crc&&assembly.Bytes==bytes&&assembly.Total==total&&assembly.Next==part,"Relay transfer is interleaved, duplicated or out of order");
        assembly.Value.Data+=chunk;assembly.RunningCrc=UpdateCrc(assembly.RunningCrc,chunk);
        assembly.IdleDeadline=now+std::chrono::seconds(30);
        ++assembly.Next;if(assembly.Next<total)return {};
        const auto checksum=~assembly.RunningCrc;
        auto value=std::move(assembly.Value);Reserved-=assembly.Bytes;Pending.erase(at);
        Require(checksum==crc,"Relay checksum mismatch");return value;
    }
};
class Server {
    struct Peer {std::map<std::string,size_t> Subscriptions;std::map<std::string,int64_t> Sequences;Clock::time_point Window{};size_t Packets=0,Bytes=0;};
    struct Delivery {std::shared_ptr<const Message> Value;size_t Part=0;};
    std::map<std::string,Channel> Channels;
    std::map<std::string,Peer> Peers;
    std::map<std::string,std::shared_ptr<const Message>> Snapshots;
    std::map<std::string,std::deque<Delivery>> Queue;
    std::map<const Message*,size_t> QueueReferences;
    std::string LastRecipient;
    size_t QueuedBytes=0,QueuedStorage=0,DeliveryCount=0,SnapshotBytes=0;
    Assembler Incoming;
    Peer& GetPeer(const std::string& key){Require(Token(key),"Relay peer identity is invalid");if(!Peers.contains(key))Require(Peers.size()<MaxPeers,"Relay peer capacity exceeded");return Peers[key];}
    static bool Wants(const Peer& peer,const Message& message){
        const auto at=peer.Subscriptions.find(message.Channel);
        return at!=peer.Subscriptions.end()&&message.Data.size()<=at->second;
    }
    void Enqueue(const std::vector<std::pair<std::string,std::shared_ptr<const Message>>>& messages){
        Require(messages.size()<=MaxDeliveries-DeliveryCount,"Relay delivery capacity exceeded");
        size_t storage=QueuedStorage;
        std::set<const Message*> added;
        for(const auto& [_,value]:messages)
            if(!QueueReferences.contains(value.get())&&added.insert(value.get()).second){
                Require(value->Data.size()<=QueueBudget-storage,"Relay queue storage exceeded");
                storage+=value->Data.size();
            }
        for(const auto& [peer,value]:messages){
            Queue[peer].push_back({value,0});
            ++QueueReferences[value.get()];
            QueuedBytes+=value->Data.size();
            ++DeliveryCount;
        }
        QueuedStorage=storage;
    }
    void Release(const Delivery& delivery){
        const auto& message=*delivery.Value;
        QueuedBytes-=message.Data.size();
        --DeliveryCount;
        const auto at=QueueReferences.find(&message);
        if(--at->second==0){
            QueuedStorage-=message.Data.size();
            QueueReferences.erase(at);
        }
    }
public:
    void Reset(){
        Peers.clear();Snapshots.clear();Queue.clear();QueueReferences.clear();LastRecipient.clear();Incoming.Clear();
        QueuedBytes=QueuedStorage=DeliveryCount=SnapshotBytes=0;
    }
    void Configure(const std::vector<Channel>& channels,const Channel* authoritySnapshot=nullptr){
        std::map<std::string,Channel> candidate;Require(channels.size()<=MaxChannels,"Too many relay channels");
        for(const auto& channel:channels){auto [at,inserted]=candidate.emplace(channel.Key,channel);Require(inserted||at->second==channel,"Conflicting relay channel registrations");}
        // The internal authority lane does not consume a mod's channel allowance.
        if(authoritySnapshot){Require(!authoritySnapshot->ClientPublish&&authoritySnapshot->Broadcast&&authoritySnapshot->Snapshot,"Invalid authority snapshot policy");Require(candidate.emplace(authoritySnapshot->Key,*authoritySnapshot).second,"Authority snapshot channel is reserved");}
        if(candidate!=Channels){Reset();Channels=std::move(candidate);}
    }
    const Channel& Route(const std::string& key)const{const auto at=Channels.find(key);Require(at!=Channels.end(),"Relay channel is not registered by this server");return at->second;}
    void Publish(const std::string& channel,const std::string& data,int64_t sequence){
        const auto& route=Route(channel);Require(route.Broadcast&&route.Snapshot&&!data.empty()&&data.size()<=route.Limit&&sequence>0,"Invalid authority snapshot");
        const auto key="authority/"+channel;const auto old=Snapshots.find(key);
        Require(old==Snapshots.end()||sequence>old->second->Sequence,"Authority snapshot sequence is stale");
        const auto prior=old==Snapshots.end()?0:old->second->Data.size();Require(data.size()<=SnapshotBudget-(SnapshotBytes-prior),"Relay snapshot capacity exceeded");
        auto value=std::make_shared<const Message>(Message{channel,"authority",data,route.Version,sequence,Crc(data)});
        std::vector<std::pair<std::string,std::shared_ptr<const Message>>> deliveries;
        for(const auto& [peer,recipient]:Peers)if(Wants(recipient,*value))deliveries.emplace_back(peer,value);
        Enqueue(deliveries);SnapshotBytes=SnapshotBytes-prior+data.size();Snapshots[key]=std::move(value);
    }
    void Subscribe(const std::string& peer,const std::string& channel,uint32_t version,bool resync=false,size_t receiveLimit=LegacyMaxBytes){
        const auto& route=Route(channel);Require(version==route.Version,"Relay subscription version mismatch");
        Require(receiveLimit>0&&receiveLimit<=MaxBytes,"Relay subscriber byte limit is invalid");
        Require(Token(peer),"Relay peer identity is invalid");
        const auto known=Peers.find(peer);
        if(known!=Peers.end()){
            const auto previous=known->second.Subscriptions.find(channel);
            if(previous!=known->second.Subscriptions.end()&&previous->second==receiveLimit&&!resync)return;
        }else Require(Peers.size()<MaxPeers,"Relay peer capacity exceeded");
        std::vector<std::pair<std::string,std::shared_ptr<const Message>>> replay;
        for(const auto& [_,value]:Snapshots)
            if(value->Channel==channel&&value->Data.size()<=receiveLimit)replay.emplace_back(peer,value);
        Enqueue(replay);GetPeer(peer).Subscriptions[channel]=receiveLimit;
    }
    void Remove(const std::string& peer){
        Peers.erase(peer);Incoming.Remove(peer);
        for(auto at=Snapshots.begin();at!=Snapshots.end();)
            if(at->second->Sender==peer){SnapshotBytes-=at->second->Data.size();at=Snapshots.erase(at);}else ++at;
        for(auto at=Queue.begin();at!=Queue.end();){
            const bool departed=at->first==peer;
            std::erase_if(at->second,[&](const Delivery& delivery){
                if(!departed&&delivery.Value->Sender!=peer)return false;
                Release(delivery);return true;
            });
            if(at->second.empty())at=Queue.erase(at);else ++at;
        }
    }
    std::optional<Message> Receive(const std::string& peer,const Json& packet,bool nativeAuthority,Clock::time_point now){
        const auto& route=Route(packet.at("channel").get<std::string>());Require(nativeAuthority||route.ClientPublish,"Client publishing is disabled for this channel");
        const auto subscriber=Peers.find(peer);
        Require(subscriber!=Peers.end()&&subscriber->second.Subscriptions.contains(route.Key),"Subscribe before publishing mod data");
        auto& client=subscriber->second;
        if(client.Window.time_since_epoch().count()==0||now-client.Window>=std::chrono::seconds(1)){client.Window=now;client.Packets=client.Bytes=0;}
        const auto wire=packet.dump().size();
        Require(++client.Packets<=PacketsPerSecond&&wire<=WirePacketBytes
            &&wire<=PacketsPerSecond*WirePacketBytes-client.Bytes,"Relay sender rate/byte limit exceeded");
        client.Bytes+=wire;
        const auto sequence=packet.at("sequence").get<int64_t>();Require(sequence>client.Sequences[route.Key],"Relay sequence was already accepted");
        auto complete=Incoming.Push(route,peer,packet,now);if(!complete)return {};
        auto shared=std::make_shared<const Message>(*complete);std::vector<std::pair<std::string,std::shared_ptr<const Message>>> deliveries;
        if(route.Broadcast)for(const auto& [key,recipient]:Peers)if(Wants(recipient,*shared))deliveries.emplace_back(key,shared);
        const auto snapshotKey=peer+"/"+route.Key;const auto old=Snapshots.find(snapshotKey);const auto prior=old==Snapshots.end()?0:old->second->Data.size();
        if(route.Snapshot)Require(shared->Data.size()<=SnapshotBudget-(SnapshotBytes-prior),"Relay snapshot capacity exceeded");
        Enqueue(deliveries);if(route.Snapshot){SnapshotBytes=SnapshotBytes-prior+shared->Data.size();Snapshots[snapshotKey]=shared;}client.Sequences[route.Key]=sequence;return complete;
    }
    std::vector<std::pair<std::string,Json>> Drain(size_t maximum,Clock::time_point now){
        Incoming.Expire(now);
        std::vector<std::pair<std::string,Json>> out;
        maximum=std::min(maximum,MaxDrainPackets);
        std::map<std::string,size_t> sent;
        while(!Queue.empty()&&out.size()<maximum){
            auto at=Queue.upper_bound(LastRecipient);
            if(at==Queue.end())at=Queue.begin();
            if(sent[at->first]==MaxPacketsPerPeer)break;
            auto& next=at->second.front();
            out.emplace_back(at->first,Packet(*next.Value,next.Part));
            LastRecipient=at->first;
            ++sent[LastRecipient];
            if(++next.Part*ChunkBytes>=next.Value->Data.size()){
                Release(next);
                at->second.pop_front();
                if(at->second.empty())Queue.erase(at);
            }
        }
        return out;
    }
    size_t PendingBytes()const{return QueuedBytes;}size_t SavedBytes()const{return SnapshotBytes;}
    size_t PendingStorage()const{return QueuedStorage;}
};
}
