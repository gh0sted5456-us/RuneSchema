#include "Core/ServerModList.h"
#include "Core/ModIdentity.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(v) ((v)?void():throw std::runtime_error(#v))
template<class F>void Reject(F call){bool failed=false;try{call();}catch(...){failed=true;}assert(failed);}
int main()try{
    namespace M=PS::ServerModList;namespace R=PS::ModDataRelay;using J=nlohmann::json;
    J rows=J::array({{{"Id","Siege"},{"Name","Siege Equipment"},{"Version","0.4.26"},{"Folder","PRIVATE"},{"PIN","SECRET"}},
        {{"Id","ScriptsOnly"},{"Name","Scripts only"},{"Version",""}}});
    const auto clean=M::Decode(M::Encode(rows));assert(clean.size()==2&&clean[0].size()==3&&!clean[0].contains("Folder")&&!clean[0].contains("PIN"));
    assert(M::Decode(M::Encode(J::array())).empty());
    const std::string website="https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=files";
    const auto identity=PS::ModIdentity::Parse("Name: RuneSchema\nWebsite: "+website,"RuneSchema");
    auto linked=rows;linked[0]["Website"]=identity.Website;
    linked[0]["ModId"]="StableSiegeId";assert(M::Decode(M::Encode(linked))[0]["ModId"]=="StableSiegeId");
    assert(M::Decode(M::Encode(linked))[0]["Website"]==website);
    linked[0]["Website"]="";assert(!M::Decode(M::Encode(linked))[0].contains("Website"));
    for(const auto& url:{"file:///private/mod.zip","javascript:alert(1)","https://","https://@host/","https://bad host/","https://host/\n", "https://host/\\file"}){
        linked[0]["Website"]=url;Reject([&]{M::Validate(linked);});
    }
    linked[0]["Website"]=1;Reject([&]{M::Validate(linked);});
    linked[0]["Website"]="https://host/"+std::string(2048,'x');Reject([&]{M::Validate(linked);});
    assert(PS::ModDownloadLink::Valid("http://example.com/mod?file=2#downloads"));
    auto invalid=rows;invalid[0]["Name"]="Bad\nName";Reject([&]{M::Validate(invalid);});invalid=rows;invalid[1]["Id"]="Siege";Reject([&]{M::Validate(invalid);});
    invalid=rows;invalid[0]["Version"]=1;Reject([&]{M::Validate(invalid);});
    invalid=rows;invalid[0]["Name"]=std::string(257,'x');Reject([&]{M::Validate(invalid);});
    J large=J::array();for(size_t i=0;i<M::MaxMods;++i)large.push_back({{"Id","Mod"+std::to_string(i)},{"Name",std::string(256,'n')},{"Version",std::string(64,'v')}});
    const auto wire=M::Encode(large);assert(wire.size()>16384&&wire.size()<M::Route.Limit);
    R::Server server;server.Configure({M::Route});server.Publish(M::Route.Key,wire,1);server.Subscribe("joining-player",M::Route.Key,1);
    R::Assembler client;std::optional<R::Message> complete;const auto now=R::Clock::now();size_t packets=0;
    while(server.PendingBytes())for(const auto& [peer,packet]:server.Drain(4,now+std::chrono::milliseconds(packets*250))){
        assert(packet.dump().size()<=4096&&peer=="joining-player");complete=client.Push(M::Route,"authority",packet,now+std::chrono::milliseconds(packets++*250));
    }
    assert(complete&&M::Decode(complete->Data)==large);
    large.push_back(large[0]);Reject([&]{M::Validate(large);});
    Reject([&]{M::Decode("{\"Kind\":\"other\",\"Mods\":[]}");});Reject([&]{M::Decode(std::string(M::Route.Limit+1,'x'));});
    std::cout<<"Public enabled mod list, bounded transfer, late join and validation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
