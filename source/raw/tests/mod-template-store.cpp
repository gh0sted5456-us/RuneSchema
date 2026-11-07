#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Core/ModTemplateStore.h"
#include <cassert>
#include <chrono>
#include <limits>
using namespace PS::ModTemplates;
template<class F> void Reject(F operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
    const auto directory=fs::temp_directory_path()/("rs-mod-templates-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);struct Cleanup{fs::path Path;~Cleanup(){std::error_code error;fs::remove_all(Path,error);}} cleanup{directory};
    const auto source=Parse(R"({"name":"Preset","values":[1,2,3],"nested":{"a/b":{"~key":true}},"-":9})");
    assert(Read(source,"/nested/a~1b/~0key")==true&&Read(source,"/values/1")==2&&Read(source,"/-")==9);
    for(const auto& bad:std::vector<std::string>{"[]","null",R"({"a":1,"a":2})",R"({"a":{"b":1,"b":2}})",R"({"a":1e999})",std::string("{\"x\":\"")+char(0xff)+"\"}",std::string(65537,' ')})Reject([&]{Parse(bad);});
    Reject([&]{Parse(std::string(34,'[')+"0"+std::string(34,']'));});
    auto document=source;Set(document,"/values/-",4);assert(Read(document,"/values/3")==4);Set(document,"/nested/a~1b/~0key",false);assert(Read(document,"/nested/a~1b/~0key")==false);
    for(const auto* path:{"/values/-","/values/01","/values/99999999999999999999999","/missing","/name/key","/bad~2key"})Reject([&]{Read(document,path);});
    for(const auto* path:{"/missing/child","/values/6","/values/01","/name/key","/bad~2key"}){const auto before=document;Reject([&]{Set(document,path,1);});assert(document==before);}
    Reject([&]{Set(document,"",Json::array());});assert(document.is_object());
    const auto before=document;Reject([&]{Set(document,"/huge",std::string(65536,'x'));});assert(document==before);
    Reject([&]{Set(document,"/nan",std::numeric_limits<double>::quiet_NaN());});assert(document==before);
    Array(Json::array({1,2.5}),true);Array(Json::array({"a","b"}),false);
    Reject([&]{Array(Json::array({1,"b"}),true);});Reject([&]{Array(Json::array({1}),false);});
    auto large=Json::array();for(int i=0;i<1025;++i)large.push_back(i);Reject([&]{Array(large,true);});
    for(const auto* bad:{"../a.json","C:bad.json","bad.json.","bad.json ","CON.json","nul.any.json","COM1.json","/a.json","a.txt","a/b.json"})Reject([&]{FileName(bad);});
    Reject([&]{Store bad(directory,"../Other");});
    Store store(directory,"JBM"),other(directory,"Other");assert(store.List().Ids.empty());
    const auto id=store.Create(source);assert(store.Read(id)==source&&other.List().Ids.empty());
    const auto root=directory/"SavedGames/RSDragonwilds/JBM";assert(fs::is_regular_file(root/id));
    fs::copy_file(root/id,root/"My copied preset.json");assert(store.Read("My copied preset.json")==source);
    const auto listing=store.List();assert(listing.Ids.size()==2&&listing.Ids[0]=="My copied preset.json"&&listing.Labels[0]=="My copied preset");
    const auto another=store.Create(source);assert(another!=id&&store.Read(id)==source&&store.Read("My copied preset.json")==source);
    Reject([&]{store.Read("../Other/test.json");});
    std::ofstream(root/"bad.json")<<"{bad";Reject([&]{store.Read("bad.json");});assert(fs::exists(root/"bad.json"));
    assert(Serialize(store.Read(id))==Serialize(source));
}
