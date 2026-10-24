#include "Core/HelpySourceCache.h"
#include "Core/ItemSource.h"
#include "Core/CacheBundleStore.h"
#include "Core/HelpyCacheOwnership.h"
#include <iostream>
#include <cassert>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
namespace {
void Number(std::string& out,uint64_t value,size_t size=4){for(size_t i=0;i<size;++i)out+=char(value>>(8*i));}
void Text(std::string& out,const std::string& value){Number(out,value.size()+1);out+=value;out+='\0';}
void Write(const std::filesystem::path& file,const std::string& value){std::filesystem::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out.write(value.data(),value.size());if(!out)throw std::runtime_error("fixture write failed");}
std::string Sha(const std::string& data){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};unsigned char digest[20]{};
    assert(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,nullptr,0)>=0);
    assert(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0);
    assert(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),static_cast<ULONG>(data.size()),0)>=0);
    assert(BCryptFinishHash(hash,digest,20,0)>=0);BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    return std::string(reinterpret_cast<char*>(digest),20);
}
std::string Pak(){
    std::string directory;Number(directory,1);Text(directory,"Items/");Number(directory,2);Text(directory,"Stone.uasset");Number(directory,0);Text(directory,"Gone.uasset");Number(directory,0x80000000);
    std::string index;Text(index,"../../../RSDragonwilds/Content/");Number(index,2);Number(index,0,8);Number(index,0);Number(index,1);
    Number(index,0,8);Number(index,directory.size(),8);index+=Sha(directory);
    std::string footer(16,'\0');footer+='\0';Number(footer,0x5a6f12e1);Number(footer,11);Number(footer,directory.size(),8);Number(footer,index.size(),8);footer+=Sha(index);footer+=std::string(160,'\0');
    return directory+index+footer;
}
std::string LegacyPak(const std::string& name="Items/Stone.uasset",bool compressed=false){
    std::string index;Text(index,"../../../RSDragonwilds/Content/");Number(index,1);Text(index,name);
    Number(index,0,8);Number(index,16,8);Number(index,16,8);Number(index,compressed?1:0);index+=std::string(20,'\0');
    if(compressed){Number(index,1);Number(index,0,8);Number(index,16,8);}
    Number(index,0,1);Number(index,65536);
    std::string footer;Number(footer,0x5a6f12e1);Number(footer,3);Number(footer,128,8);Number(footer,index.size(),8);footer+=Sha(index);
    return std::string(128,'\0')+index+footer;
}
std::string Toc(const std::string& mount="../../../RSDragonwilds/Content/",uint32_t sibling=UINT32_MAX){
    std::string directory;Text(directory,mount);Number(directory,1);
    Number(directory,UINT32_MAX);Number(directory,UINT32_MAX);Number(directory,sibling);Number(directory,0);
    Number(directory,1);Number(directory,0);Number(directory,UINT32_MAX);Number(directory,0);
    Number(directory,1);Text(directory,"Stone.uasset");
    std::string header="-==--==--==--==-";Number(header,8,1);Number(header,0,3);Number(header,144);Number(header,1);Number(header,0);Number(header,12);
    Number(header,0);Number(header,32);Number(header,65536);Number(header,directory.size());Number(header,1);header+=std::string(24,'\0');Number(header,8,1);header.resize(144,'\0');
    return header+std::string(22,'\0')+directory;
}
}
int main(int argc,char** argv)try{
    namespace fs=std::filesystem;namespace P=PS::PakDiscovery;namespace C=PS::HelpyCacheSources;namespace S=PS::HelpySourceCache;
    if(argc>1){for(int i=1;i<argc;++i){const auto start=std::chrono::steady_clock::now();const auto result=P::Read(argv[i],"RSDragonwilds");
        std::cout<<fs::path(argv[i]).filename().string()<<" "<<nlohmann::json{{"Complete",result.Complete},{"Packages",result.Packages.size()},{"IndexKey",result.IndexKey},{"Reason",result.Error},{"Milliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()}}.dump()<<'\n';}return 0;}
    const auto fixture=fs::temp_directory_path()/("runeschema-pak-discovery-"+std::to_string(GetCurrentProcessId()));
    const auto root=fixture/"RSDragonwilds"/"Binaries"/"Win64"/"ue4ss"/"Mods"/"RuneSchema";
    const auto file=root/"mods"/"Example"/"paks"/"Example.utoc";Write(file,Toc());Write(file.parent_path()/"Example.ucas","payload");
    const auto toc=P::Read(file,"RSDragonwilds");assert(toc.Complete&&toc.Packages==std::set<std::string>{"/game/stone"});
    const auto pak=file.parent_path()/"Example.pak";Write(pak,Pak());const auto indexed=P::Read(pak,"RSDragonwilds");
    assert(indexed.Complete&&indexed.Packages==std::set<std::string>{"/game/items/stone"});
    for(bool compressed:{false,true}){Write(pak,LegacyPak("Items/Stone.uasset",compressed));const auto legacy=P::Read(pak,"RSDragonwilds");
        assert(legacy.Complete&&legacy.Packages==std::set<std::string>{"/game/items/stone"});}
    Write(pak,LegacyPak("../Private/Stone.uasset"));assert(!P::Read(pak,"RSDragonwilds").Complete);
    {auto bad=LegacyPak();bad[130]^=1;Write(pak,bad);assert(!P::Read(pak,"RSDragonwilds").Complete);}
    for(size_t cut=0;cut<LegacyPak().size();++cut){Write(pak,LegacyPak().substr(0,cut));assert(!P::Read(pak,"RSDragonwilds").Complete);}
    Write(pak,Pak());
    assert(P::Package("../../../RSDragonwilds/Plugins/GameFeatures/Colors/Content/Icons/T.uasset","RSDragonwilds")=="/colors/icons/t");
    assert(P::Package("../../../Engine/Content/Icons/T.uasset","RSDragonwilds")=="/engine/icons/t");
    assert(P::Package("../../../RSDragonwilds/Content/../Private/T.uasset","RSDragonwilds").empty());
    assert(P::Package("../../../OtherProject/Content/Items/A.uasset","RSDragonwilds")=="/otherproject/items/a");
    assert(P::Package("../../../RSDragonwilds/Content/Items/A.txt","RSDragonwilds").empty());
    Write(pak,Pak());{auto bad=Pak();bad[4]^=1;Write(pak,bad);}assert(!P::Read(pak,"RSDragonwilds").Complete);Write(pak,Pak());
    {auto bad=Toc();bad[80]=10;Write(file,bad);}assert(!P::Read(file,"RSDragonwilds").Complete);
    {auto bad=Toc();bad.resize(bad.size()-1);Write(file,bad);}assert(!P::Read(file,"RSDragonwilds").Complete);
    Write(file,Toc("../../../RSDragonwilds/Content/",0));assert(!P::Read(file,"RSDragonwilds").Complete);
    for(size_t cut=0;cut<Toc().size();++cut){Write(file,Toc().substr(0,cut));assert(!P::Read(file,"RSDragonwilds").Complete);}
    Write(file,Toc());C::Plan plan;plan.Authoritative=true;plan.Registered={"game"};
    P::ResetRoots();P::RememberRoot(file.parent_path(),"Example");
    const auto snapshot=S::Inspect(root,plan);assert(snapshot.ContainerAssets->at("/game/stone")==std::set<std::string>{"mods/example"});
    assert(snapshot.ContainerFiles->at("/game/stone").begin()->ends_with("/mods/example/paks/example.utoc"));
    assert(snapshot.Containers.size()==2&&snapshot.Owners.at("game").contains("example"));
    auto itemPlan=PS::CacheBundleStore::Plan(snapshot);
    assert(PS::ContentSource::Choices(itemPlan).contains("mods/example"));
    auto item=PS::ItemSource::Describe("/Game/Stone.Stone",itemPlan);assert(item.Group==PS::ItemSource::Kind::Modded&&item.Name=="example"&&!item.Containers.empty());
    auto row=PS::ItemSource::Fields(item);row["Path"]="/Game/Stone.Stone";assert(PS::ItemSource::Row(row).Group==PS::ItemSource::Kind::Modded);
    assert(PS::ItemSource::Reattribute(row,{},{}).Group==PS::ItemSource::Kind::Modded);
    const auto fetch=PS::HelpyCacheOwnership::Fetch(snapshot,"mods/example",[&](const auto& request,auto& response){response=PS::HelpyCacheOwnership::Page(snapshot,request);return true;});
    assert(fetch.ContainerAssets->at("/game/stone")==std::set<std::string>{"mods/example"});
    row["Name"]="Stone";
    const auto record=PS::CacheBundle::Parse(PS::CacheBundle::FromRow("items",row));
    assert(PS::CacheBundle::Owned(record,PS::CacheBundleStore::Plan(fetch),{}));
    const auto cache=fixture/"cache";PS::CacheBundleStore::Commit(cache,root,fetch,"test",{record});
    const auto cached=PS::CacheBundleStore::Read(cache,root,fetch,"test","items",false);
    assert(cached.size()==1&&PS::ItemSource::Row(cached[0]["Data"]).Group==PS::ItemSource::Kind::Modded);
    const auto originalKey=snapshot.SourceKeys.at("game");
    const auto sameSize=file;const auto stamp=fs::last_write_time(sameSize);auto changed=Toc();const auto at=changed.find("Stone.uasset");changed.replace(at,5,"Other");Write(sameSize,changed);fs::last_write_time(sameSize,stamp);
    assert(S::Inspect(root,plan).SourceKeys.at("game")!=originalKey);Write(file,Toc());
    const auto second=root/"mods"/"Other"/"paks"/"Other.utoc";Write(second,Toc());P::RememberRoot(second.parent_path(),"Other");
    const auto conflicting=S::Inspect(root,plan);itemPlan=PS::CacheBundleStore::Plan(conflicting);
    item=PS::ItemSource::Describe("/Game/Stone.Stone",itemPlan);assert(item.Group==PS::ItemSource::Kind::Unknown&&item.Name.empty()&&item.Containers.size()==2);
    assert(PS::ContentSource::Origins("/Game/Stone.Stone",itemPlan).size()==2);
    assert(!PS::CacheBundle::Owned(record,itemPlan,{}));
    // Declaration/registry metadata remains independent; container discovery cannot register items.
    assert(C::Snapshot().Registered.empty());
    P::ResetRoots();const auto disabled=S::Inspect(root,plan);assert(disabled.ContainerAssets->empty());
    const auto global=fixture/"RSDragonwilds"/"Content"/"Paks"/"global.utoc";
    auto metadata=Toc().substr(0,144);metadata.replace(48,4,4,'\0');metadata.replace(56,8,8,char(0xff));metadata[80]=0;metadata+=std::string(22,'\0');
    Write(global,metadata);const auto native=P::Read(global,"RSDragonwilds");assert(native.Complete&&native.MetadataOnly&&native.Packages.empty());
    const auto globals=S::Inspect(root,plan);assert(globals.Containers.at("content/paks/global.utoc").at("Status")=="MetadataOnly");
    assert(globals.Containers.at("content/paks/global.utoc").at("Owner")=="core"&&globals.ContainerAssets->empty());
    auto remote=S::Document(globals);(void)S::AcceptRemote(remote);
    remote["Containers"]["content/paks/global.utoc"]["Owner"]="mods/example";
    bool rejected=false;try{S::AcceptRemote(remote);}catch(...){rejected=true;}assert(rejected);
    metadata[56]=0;Write(global,metadata);assert(!P::Read(global,"RSDragonwilds").Complete);
    fs::remove_all(fixture);
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
