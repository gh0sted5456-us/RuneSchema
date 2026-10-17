#include "Core/HelpyIconPack.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(condition) do {if(!(condition))throw std::runtime_error(std::string("check failed at line ")+std::to_string(__LINE__)+": "+#condition);}while(false)
using namespace PS;
namespace fs=std::filesystem;
template<class F> void Reject(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}assert(caught);}
int Run(){
    using namespace HelpyIconPack;
    const auto fixture=fs::temp_directory_path()/("rsmk-icon-pack-"+std::to_string(GetCurrentProcessId()));fs::create_directories(fixture);
    const auto root=fixture/"first"/"Game"/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema";
    const auto root2=fixture/"second"/"Game"/"Binaries"/"Win64"/"ue4ss"/"Mods"/"RuneSchema";
    const auto write=[](const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<text;};
    for(const auto& r:{root,root2}){write(r/"mods"/"First"/"assets"/"item.jsonc","{\"Item\":1}");write(r/"mods"/"Second"/"paks"/"Second.pak","same mod content");
        write(r.parent_path().parent_path().parent_path().parent_path().parent_path()/"Content"/"Paks"/"Game.pak","same native texture content");}
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Revision=3;plan.Registered={"game","firstmount","secondmount"};plan.Owners={{"firstmount",{"first"}},{"secondmount",{"second"}}};
    auto first=HelpySourceCache::Inspect(root,plan);const auto cache=fixture/"cache",cache2=fixture/"cache2";
    const std::set<std::string> wanted={"game","firstmount","secondmount"};Progress progress;
    const auto keys=Portable(root,first,wanted,cache,progress);assert(keys.size()==3);
    // Logical content, not physical path, storefront lane or file times.
    fs::last_write_time(root2/"mods"/"First"/"assets"/"item.jsonc",fs::file_time_type::clock::now()+std::chrono::hours(24));
    auto second=HelpySourceCache::Inspect(root2,plan);assert(second.Key!=first.Key);assert(Portable(root2,second,wanted,cache2,progress)==keys);
    const std::string icon="/FirstMount/Art/T_A.T_A",icon2="/SecondMount/Art/T_B.T_B";
    std::vector<char> pixels(16+64*64*4,100);std::memcpy(pixels.data(),"RSI3",4);uint32_t w=64,n=64*64*4;std::memcpy(pixels.data()+4,&w,4);std::memcpy(pixels.data()+8,&w,4);std::memcpy(pixels.data()+12,&n,4);Pixels(pixels);
    for(const auto& path:{icon,icon2}){const auto target=Target(cache,first,path);Publish(target,pixels);Index(target,path);}
    const auto pack=fixture/"Shared.rsmk-icons";assert(Export(pack,root,cache,first,progress).Exported==2);
    assert(progress.Stage==Phase::Complete&&progress.Done==2&&progress.Total==2);
    assert(progress.BytesTotal==2*pixels.size()&&progress.BytesDone==progress.BytesTotal);
    Index(Target(cache,first,icon),icon,{"mods/colorsofmoney"});Index(Target(cache,first,icon),icon); // browsing must retain origin metadata
    const auto originIndex=Json(Read(Target(cache,first,icon).wstring()+L".json",8192));assert(originIndex["Origins"][0]=="mods/colorsofmoney");
    const auto sourcePack=fixture/"ColorsOfMoney.rsmk-icons";
    assert(Export(sourcePack,root,cache,first,progress,"mods/colorsofmoney").Exported==1);
    {std::ifstream file(sourcePack,std::ios::binary);uint64_t begin=0;const auto filtered=Header(file,begin,fs::file_size(sourcePack));assert(filtered.Icons.size()==1&&filtered.Icons[0].Icon==icon&&filtered.Icons[0].Origins.contains("mods/colorsofmoney"));}
    Reject([&]{Export(fixture/"empty.rsmk-icons",root,cache,first,progress,"mods/absent");});assert(!fs::exists(fixture/"empty.rsmk-icons"));
    Reject([&]{Export(fixture/"invalid.rsmk-icons",root,cache,first,progress,"../outside");});
    auto result=Import(pack,root2,cache2,second,1024*1024,progress);assert(result.Imported==2&&result.Skipped==0);assert(Read(Target(cache2,second,icon),100000)==pixels);
    assert(Import(sourcePack,root2,cache2,second,1024*1024,progress).Imported==1);
    Index(Target(cache2,second,icon),icon);
    assert(Json(Read(Target(cache2,second,icon).wstring()+L".json",8192))["Origins"][0]=="mods/colorsofmoney");
    // Editing one mod must neither import its stale icons nor exclude others.
    write(root2/"mods"/"Second"/"paks"/"Second.pak","new texture data");second=HelpySourceCache::Inspect(root2,plan);
    result=Import(pack,root2,cache2,second,1024*1024,progress);assert(result.Imported==1&&result.Skipped==1);assert(!fs::exists(Target(cache2,second,icon2)));
    auto missing=plan;missing.Registered.erase("secondmount");second=HelpySourceCache::Inspect(root2,missing);result=Import(pack,root2,cache2,second,1024*1024,progress);assert(result.Imported==1&&result.Skipped==1);
    Reject([&]{Import(pack,root2,cache2,second,1,progress);});
    auto corrupted=Read(pack,MaxPack);corrupted.back()^=1;const auto bad=fixture/"Bad.rsmk-icons";Publish(bad,corrupted);Reject([&]{Import(bad,root2,cache2,second,1024*1024,progress);});
    const auto cleanCache=fixture/"nothing-published";Reject([&]{Import(bad,root2,cleanCache,second,1024*1024,progress);});assert(!fs::exists(Target(cleanCache,second,icon)));
    std::ifstream input(pack,std::ios::binary);uint64_t start=0;auto manifest=Header(input,start,fs::file_size(pack));assert(manifest.Icons.size()==2);
    assert(!IconPath("/FirstMount/../../evil.x"));assert(!IconPath("/FirstMount/Art/T_A:T_A"));assert(!IconPath(std::string("/FirstMount/T\0.T",16)));
    Reject([&]{auto p=pixels;p[4]=0;Pixels(p);});
    nlohmann::json document={{"Version",1},{"Sources",{{"firstmount",keys.at("firstmount")}}},{"Icons",nlohmann::json::array({{{"Icon",icon},{"Source","firstmount"},{"SHA256",Hash(pixels)},{"Bytes",pixels.size()}}})}};
    assert(Parse(document,pixels.size()).Icons.size()==1);Reject([&]{Parse(document,pixels.size()+1);});
    auto malformed=document;malformed["Icons"][0]["Source"]="../../";Reject([&]{Parse(malformed,pixels.size());});
    malformed=document;malformed["Icons"].push_back(malformed["Icons"][0]);Reject([&]{Parse(malformed,2*pixels.size());});
    malformed=document;malformed["Icons"][0]["Bytes"]=int64_t(-1);Reject([&]{Parse(malformed,pixels.size());});
    Reject([&]{Json(std::string(30,'[')+"0"+std::string(30,']'));});
    progress.Cancel=true;Reject([&]{Export(fixture/"cancel.rsmk-icons",root,cache,first,progress);});assert(!fs::exists(fixture/"cancel.rsmk-icons"));
    const auto previousPack=Read(pack,MaxPack);Reject([&]{Export(pack,root,cache,first,progress);});assert(Read(pack,MaxPack)==previousPack); // cancel preserves old pack
    progress.Cancel=false;auto changed=first;changed.Key="0000000000000000";Reject([&]{Portable(root,changed,wanted,cache,progress);});
    // A server cook may contain live definitions but no client textures. An
    // explicit image-only import can still display active exact-path visuals.
    const auto native2=root2.parent_path().parent_path().parent_path().parent_path().parent_path()/"Content"/"Paks"/"Game.pak";
    write(native2,"dedicated server cook without texture pixels");second=HelpySourceCache::Inspect(root2,missing);
    const auto serverCache=fixture/"server-visuals";
    assert(Import(pack,root2,serverCache,second,1024*1024,progress).Imported==0);
    result=Import(pack,root2,serverCache,second,1024*1024,progress,ImportPolicy::ServerDisplayOnly);
    assert(result.Imported==1&&result.Skipped==1&&result.Message.find("Display-only")!=std::string::npos);
    assert(Read(Target(serverCache,second,icon),100000)==pixels);
    const auto serverIndex=Target(serverCache,second,icon).wstring()+L".json";
    assert(Json(Read(serverIndex,8192))["DisplayOnly"]==true);
    Index(Target(serverCache,second,icon),icon,{"mods/first"});
    assert(Json(Read(serverIndex,8192))["DisplayOnly"]==true); // ordinary browsing cannot certify it
    Reject([&]{Export(fixture/"not-certified.rsmk-icons",root2,serverCache,second,progress);});
    Reject([&]{Import(bad,root2,fixture/"server-corrupt",second,1024*1024,progress,ImportPolicy::ServerDisplayOnly);});
    auto stale=second;stale.Key="0000000000000000";
    Reject([&]{Import(pack,root2,serverCache,stale,1024*1024,progress,ImportPolicy::ServerDisplayOnly);});
    Reject([&]{Import(pack,root2,serverCache,second,1,progress,ImportPolicy::ServerDisplayOnly);});
    input.close();fs::remove_all(fixture);std::cout<<"Portable icon packs, rekeying, corrupt/missing/changed source and bounds tests passed\n";return 0;
}
int main(){try{return Run();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
