#include "Core/HelpyMarkSync.h"
#include <cassert>
using namespace PS;using Json=nlohmann::json;
template<class F>void Reject(F f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv){
    assert(argc==2);namespace P=HelpyPersonalization;namespace S=HelpyMarkSync;
    const auto root=std::filesystem::path(argv[1])/std::to_string(GetCurrentProcessId());
    const std::string owner="11111111222222223333333344444444",other="aaaaaaaa222222223333333344444444";
    assert(P::Load(root/"owner.json"));Json item={{"PlayerGuid",owner},{"Path","/Game/Stone.Stone"},{"Instance",{1,2,3,4}}};
    assert(P::Set(P::TypeKey(item),{{"Favorite",true},{"Nickname","Private name"},{"Tags",Json::array({"private","junk"})},{"Locked",true}}));
    assert(P::Set(P::StackKey(item),{{"Favorite",false}}));
    auto foreign=item;foreign["PlayerGuid"]=other;assert(P::Set(P::StackKey(foreign),{{"Favorite",true}}));
    for(uint32_t i=0;i<75;++i){auto copy=item;copy["Instance"]={i+100,2,3,4};assert(P::Set(P::StackKey(copy),{{"Favorite",true}}));}
    std::vector<Json> packets;size_t offset=0;uint64_t version=0;
    do{auto batch=S::Export({{"CharacterGuid",owner},{"Offset",offset},{"Version",version}});assert(batch.dump().size()<=3200);
        assert(batch.dump().find("private")==std::string::npos&&batch.dump().find("Locked")==std::string::npos&&batch.dump().find(other)==std::string::npos);
        packets.push_back(batch);offset=batch.at("Next");version=batch.at("Version");if(batch.at("Done").get<bool>())break;
    }while(packets.size()<100);
    assert(packets.size()>1&&packets.back().at("Done")==true);
    assert(P::Load(root/"server.json"));auto remote=item;remote["MarksReadOnly"]=true;
    for(size_t i=0;i<packets.size();++i){const auto receipt=S::Receive(packets[i]);assert(receipt==S::Receive(packets[i]));if(i+1<packets.size())assert(!P::Tagged(remote,"junk"));}
    assert(!P::Favorite(remote)&&P::Tagged(remote,"junk")&&!P::Locked(remote)&&!P::Effective(remote).contains("Nickname"));
    assert(!P::Favorite(item)&&!P::Tagged(item,"junk"));
    const auto unchanged=ConfigFiles::Read(root/"server.json");
    auto bad=packets[0];bad["CharacterGuid"]=other;Reject([&]{S::Receive(bad);});
    bad=packets[0];bad["Rows"][0]["Labels"]["PrivateTags"]={"leak"};Reject([&]{S::Receive(bad);});
    bad=packets[0];bad["Offset"]=size_t(900);Reject([&]{S::Receive(bad);});
    assert(ConfigFiles::Read(root/"server.json")==unchanged);
    assert(P::Load(root/"owner.json"));auto clear=S::Export({{"CharacterGuid",other}});
    assert(P::Load(root/"server.json"));S::Receive(clear);assert(P::Tagged(remote,"junk"));
    Json empty={{"CharacterGuid",owner},{"Version",uint64_t(99)},{"Offset",size_t(0)},{"Next",size_t(0)},{"Done",true},{"Rows",Json::array()}};
    S::Receive(empty);assert(!P::Favorite(remote)&&!P::Tagged(remote,"junk"));
    assert(P::Load(root/"server.json")&&!P::Tagged(remote,"junk"));
}
