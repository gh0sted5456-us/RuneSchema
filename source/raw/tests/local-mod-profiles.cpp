#include "Core/LocalModProfiles.h"
#include <cassert>
#include <future>
using namespace PS::LocalModProfiles;
namespace CacheBundleIO=PS::CacheBundleIO;
#undef assert
#define assert(value) do {if(!(value))throw std::runtime_error("Assertion failed: " #value);}while(false)
template<class F> void Reject(F run){bool rejected=false;try{run();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
    const auto root=fs::temp_directory_path()/("RuneSchema-profile-test-"+std::to_string(GetCurrentProcessId()));
    assert(!fs::exists(root));
    struct Cleanup {fs::path Root;~Cleanup(){std::error_code error;fs::remove_all(Root,error);}}cleanup{root};
    const std::string owner="example:body",guid="00112233445566778899AABBCCDDEEFF";
    assert(Class({{"Class","/Game/Example/BPC_Profile.BPC_Profile_C"}}).ends_with("_C"));
    Reject([]{Class({{"Class","/Script/Engine.ActorComponent"}});});
    Reject([]{Class({{"Class","/Game/../Other_C"}});});
    Reject([]{Class({{"Class","/Game/Example_C"},{"Path","elsewhere"}});});
    const auto missing=Read(root,owner,guid);assert(!missing.Found&&missing.Revision==0&&missing.Payload.empty());
    const auto first=Write(root,owner,guid,0,R"({"SchemaVersion":99,"Values":[1.5,2.5],"Enabled":false})");
    assert(first.Found&&first.Revision==1);
    const auto read=Read(root,owner,guid);assert(read.Payload==first.Payload&&read.Revision==1);
    assert(Json::parse(read.Payload)["SchemaVersion"]==99);
    assert(!Read(root,"other:body",guid).Found);
    Reject([&]{Write(root,owner,guid,0,"{}");});
    assert(Read(root,owner,guid).Payload==first.Payload);
    auto race=[&]{try{return Write(root,owner,guid,1,R"({"Changed":true})").Revision==2;}catch(const std::exception&){return false;}};
    auto a=std::async(std::launch::async,race),b=std::async(std::launch::async,race);
    const bool one=a.get(),two=b.get();assert(one!=two&&Read(root,owner,guid).Revision==2);
    {WriteLock held(File(root,owner,guid));Reject([&]{Write(root,owner,guid,2,"{}");});}
    assert(Read(root,owner,guid).Revision==2);
    Reject([&]{Write(root,owner,"../../another",0,"{}");});
    Reject([&]{Write(root,owner,std::string(32,'0'),0,"{}");});
    Reject([&]{Write(root,owner,guid,-1,"{}");});
    Reject([&]{Write(root,owner,guid,2,"[]");});
    Reject([&]{Write(root,owner,guid,2,"{broken");});
    Reject([&]{Write(root,owner,guid,2,std::string(MaximumPayloadBytes+1,'x'));});
    Reject([&]{Write(root,owner,guid,2,std::string(70,'[')+"0"+std::string(70,']'));});
    const std::string deep=std::string("{\"x\":")+std::string(61,'[')+"0"+std::string(61,']')+"}";
    assert(Write(root,"example:deep",guid,0,deep).Revision==1);
    assert(Json::parse(Read(root,"example:deep",guid).Payload)==Json::parse(deep));
    const auto file=File(root,owner,guid);const auto before=CacheBundleIO::Read(file,MaximumPayloadBytes+4096);
    auto corrupt=Json::parse(before);corrupt["CharacterGuid"]="FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
    CacheBundleIO::Publish(file,corrupt.dump());const auto preserved=CacheBundleIO::Read(file,MaximumPayloadBytes+4096);
    Reject([&]{Read(root,owner,guid);});Reject([&]{Write(root,owner,guid,2,"{}");});
    assert(CacheBundleIO::Read(file,MaximumPayloadBytes+4096)==preserved);
    corrupt=Json::parse(before);corrupt["Payload"]["Changed"]=false;CacheBundleIO::Publish(file,corrupt.dump());
    Reject([&]{Read(root,owner,guid);});
}
