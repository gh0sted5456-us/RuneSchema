#include "Core/HelpyIconPack.h"
#include "Core/CacheBundleItems.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(x) do{if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": "+#x);}while(false)
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;
template<class F> void Reject(F fn){bool caught=false;try{fn();}catch(...){caught=true;}assert(caught);}
int main(){try{
    std::vector<unsigned char> pixels(SourceBanner::Width*SourceBanner::Height*4,130);const auto banner=SourceBanner::Encode(pixels);assert(SourceBanner::Decode(banner)==pixels);
    auto bannerRecord=CacheBundle::Parse({{"Kind","art"},{"Owner","mods/dye"},{"Path","/Dye/__Helpy__/Banner.Banner"},{"Data",{{"Name","Dye"},{"SourceBannerPixels",banner}}}});
    assert(CacheBundle::Parse(CacheBundle::Document(bannerRecord)).Data.at("SourceBannerPixels")==banner);
    auto invalid=CacheBundle::Document(bannerRecord);invalid["Kind"]="items";Reject([&]{CacheBundle::Parse(invalid);});invalid=CacheBundle::Document(bannerRecord);invalid["Data"]["SourceBannerPixels"]["Width"]=4096;Reject([&]{CacheBundle::Parse(invalid);});
    auto malformed=banner;malformed["RGBAHex"]=std::string(SourceBanner::Width*SourceBanner::Height*8,'z');Reject([&]{SourceBanner::Decode(malformed);});
    assert(std::string(SourceBanner::Builtin("mods/dye_goldtrim"))=="source-colorfuldye.png");assert(std::string(SourceBanner::Builtin("mods/flintlocks"))=="source-gunslinger.png");assert(std::string(SourceBanner::Builtin("mods/rsdwspears"))=="source-spears-scythes.png");assert(std::string(SourceBanner::Builtin("mods/shadowmancerarmor"))=="source-shadowmancer.png");assert(std::string(SourceBanner::Builtin("mods/unrelated"))=="");
    assert(std::string(SourceBanner::Builtin("plugins/dowdunreach"))=="source-bastion.png");
    assert(SourceBanner::OwnerForItem({{"Path","/Game/Items/Tea.Tea"},{"SourceKind","modded"},{"SourceName","RuneSchema"},{"ModifiedByRuneSchema",true}})=="core");
    assert(SourceBanner::OwnerForItem({{"Path","/Dye/Items/Hat.Hat"},{"SourceKind","modded"},{"SourceName","Dye_GoldTrim"},{"Icon","/Game/UI/Icons/Tea.Tea"}})=="mods/dye_goldtrim");
    assert(SourceBanner::OwnerForItem({{"Path","/UmbralSands/Items/Tea.Tea"},{"SourceKind","expansion"},{"SourceName","UmbralSands"},{"ModifiedByRuneSchema",true},{"ModifiedBy","Dye"}})=="plugins/umbralsands");
    assert(SourceBanner::OwnerForItem({{"Path","/Shared/Items/Hat.Hat"},{"SourceKind","unknown"},{"SourceName",""}}).empty());
    const std::string path="/ExampleMount/Items/DA_Health.DA_Health";
    const Json original={{"Kind","items"},{"Owner","mods/example"},{"Source","examplemount"},{"Path",path},{"PersistenceID","Health-Tincture:Exact-ID"},{"Data",{{"Name","Health Tincture"},{"Icon","/Game/Art/T_Health.T_Health"},{"Available",true},{"Handle",123},{"Command","Grant"},{"Read",true}}}};
    const auto row=CacheBundle::Parse(original);assert(row.Path==path&&row.PersistenceID=="Health-Tincture:Exact-ID");
    assert(row.Data["BundleReference"]==true&&row.Data["ObservedCurrent"]==false&&row.Data["PersistenceIDKnown"]==true);
    assert(!row.Data.contains("Available")&&!row.Data.contains("Command")&&!row.Data.contains("Handle")&&!row.Data.contains("Read"));
    assert(CacheBundle::Parse(CacheBundle::Document(row)).PersistenceID==row.PersistenceID);
    auto bad=original;bad["Path"]="/Game/../Unsafe.Unsafe";Reject([&]{CacheBundle::Parse(bad);});
    bad=original;bad["Path"]="/Engine/Transient/Test.Test";Reject([&]{CacheBundle::Parse(bad);});
    bad=original;bad["PersistenceID"]=std::string(257,'x');Reject([&]{CacheBundle::Parse(bad);});
    bad=original;bad["Data"]["PowerLevel"]=-1;Reject([&]{CacheBundle::Parse(bad);});
    auto nativeUnknown=original["Data"];nativeUnknown["Path"]=path;nativeUnknown["SourceKind"]="modded";nativeUnknown["SourceName"]="example";nativeUnknown["PersistenceID"]="exact-native-id";nativeUnknown["PowerLevel"]=-1;
    const auto unknownReference=CacheBundle::FromRow("items",nativeUnknown);
    assert(!unknownReference.is_null()&&unknownReference["PersistenceID"]=="exact-native-id"&&!unknownReference["Data"].contains("PowerLevel"));
    bad=original;bad["Source"]="game";Reject([&]{CacheBundle::Parse(bad);});
    Reject([&]{CacheBundle::Records(Json::array({original,original}));});
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Revision=1;plan.Registered={"game","examplemount"};plan.Owners={{"examplemount",{"example"}}};plan.DeclaredOwners=plan.Owners;
    assert(CacheBundle::Owned(row,plan,{}));
    auto native=row;native.Owner="mods/example";native.Path="/Game/Items/DA_Tea.DA_Tea";native.Source="game";
    auto authored=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();(*authored)[HelpyCacheSources::AssetKey(native.Path)]={"example"};plan.AuthoredAssets=authored;
    assert(!CacheBundle::Owned(native,plan,{})); // Native patch isn't original ownership.
    native.Owner="core";assert(CacheBundle::Owned(native,plan,{}));
    native.Path=path;native.Source="examplemount";assert(!CacheBundle::Owned(native,plan,{}));
    auto borrowed=row;borrowed.Owner="mods/other";assert(!CacheBundle::Owned(borrowed,plan,{}));
    auto shared=plan;shared.DeclaredOwners["examplemount"].insert("other");assert(CacheBundle::Owned(row,shared,{})&&!CacheBundle::OriginalOwnerProven(row,shared,{}));
    const auto fixture=fs::temp_directory_path()/("rsmk-content-bundle-"+std::to_string(GetCurrentProcessId()));fs::create_directories(fixture);
    const auto root=fixture/"Game"/"Binaries"/"Win64"/"ue4ss"/"Mods"/"RuneSchema",cache=fixture/"cache"/"item-icons";
    const auto pak=fixture/"Game"/"Content"/"Paks"/"Game.pak";fs::create_directories(pak.parent_path());{std::ofstream out(pak);out<<"native content";}
    const auto mod=root/"mods"/"Example"/"assets"/"example.jsonc";fs::create_directories(mod.parent_path());{std::ofstream out(mod);out<<"{}";}
    auto snapshot=HelpySourceCache::Inspect(root,plan);snapshot.DeclaredOwners=plan.DeclaredOwners;
    const auto locale=CacheBundleStore::Locale();
    assert(CacheBundleStore::Commit(cache,root,snapshot,locale,{row})==1);
    assert(CacheBundleStore::Commit(cache,root,snapshot,locale,{row})==0);
    auto references=CacheBundleStore::Read(cache,root,snapshot,locale,"items");assert(references.size()==1&&references[0]["PersistenceID"]==row.PersistenceID);
    assert(references[0]["Data"]["CacheSourceVerified"]==true);
    auto journal=original;journal["Kind"]="journal";journal["Path"]="/ExampleMount/Journal/DA_Note.DA_Note";journal["Data"]={{"Name","A note"},{"Pages",{"Native text"}},{"Icon","/ExampleMount/Art/T_Note.T_Note"}};
    const auto note=CacheBundle::Parse(journal);assert(CacheBundleStore::Commit(cache,root,snapshot,locale,{note})==1);
    const auto journalFolder=fixture/"journal";
    auto nativeNote=note.Data;nativeNote["SourceKind"]="modded";nativeNote["SourceName"]="example";
    const auto journalText=Json{{"Version",1},{"SourceKey",snapshot.CatalogKey},{"Entries",Json::array({nativeNote})}}.dump();
    CacheBundleIO::Publish(journalFolder/"catalog.json",journalText);
    // Catalogue identity remains valid when icon settings change the broad key.
    auto iconSettingsChanged=snapshot;iconSettingsChanged.Key="different-icon-key";
    const auto collected=CacheBundleStore::Collect(fixture/"fresh"/"item-icons",root,journalFolder,iconSettingsChanged);
    assert(collected.size()==1&&collected[0]["Kind"]=="journal");
    assert(CacheBundleStore::Read(cache,root,snapshot,locale).size()==2);
    assert(CacheBundleStore::Read(cache,root,snapshot,"unmatched-locale").empty());
    const auto index=CacheBundleIO::Read(CacheBundleStore::Directory(cache,root)/"index.json",1024*1024);
    auto replacement=row;replacement.Data["Name"]="Changed";
    Reject([&]{CacheBundleStore::Commit(cache,root,snapshot,locale,{replacement},[]{throw std::runtime_error("cancel");});});
    assert(CacheBundleIO::Read(CacheBundleStore::Directory(cache,root)/"index.json",1024*1024)==index);
    auto changed=snapshot;changed.SourceKeys["examplemount"]="0123456789abcdef";
    assert(CacheBundleStore::Read(cache,root,changed,locale,"items",false).empty());
    assert(CacheBundleStore::Read(cache,root,changed,locale,"items",true)[0]["Data"]["CacheSourceVerified"]==false);
    auto inactive=snapshot;inactive.SourceKeys.erase("examplemount");assert(CacheBundleStore::Read(cache,root,inactive,locale).empty());
    auto page=CacheBundleItems::Page(references,"Health","",40);assert(page["Rows"].size()==1&&page["Rows"][0]["PersistenceID"]==row.PersistenceID);
    assert(page["CachePreview"]==true&&!page["HasMore"].get<bool>());
    assert(CacheBundleItems::Page(references,"missing","",40)["Rows"].empty());
    const auto bundle=fixture/"Example.rsmk-cache";HelpyIconPack::Progress progress;
    const auto exported=HelpyIconPack::Export(bundle,root,cache,snapshot,progress,{},Json::object(),Json::array({original,journal}));
    assert(exported.Exported==0&&exported.RecordsExported==2);
    {std::ifstream file(bundle,std::ios::binary);uint64_t start=0;const auto manifest=HelpyIconPack::Header(file,start,fs::file_size(bundle));assert(manifest.Version==2&&manifest.Records.size()==2&&manifest.Locale==locale);}
    const auto destination=fixture/"import"/"item-icons";
    auto imported=HelpyIconPack::Import(bundle,root,destination,snapshot,1024*1024,progress);assert(imported.RecordsImported==2);
    imported=HelpyIconPack::Import(bundle,root,destination,snapshot,1024*1024,progress);assert(imported.RecordsImported==0);
    assert(CacheBundleStore::Read(destination,root,snapshot,locale).size()==2);
    imported=HelpyIconPack::Import(bundle,root,fixture/"denied"/"item-icons",snapshot,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{},{},"mods/other");assert(imported.RecordsImported==0);
    imported=HelpyIconPack::Import(bundle,root,fixture/"visuals"/"item-icons",snapshot,1024*1024,progress,HelpyIconPack::ImportPolicy::ServerDisplayOnly);assert(imported.RecordsImported==0);
    auto document=Json{{"Version",2},{"Kind","RuneSchemaCacheBundle"},{"Locale",locale},{"Sources",{{"examplemount",std::string(64,'0')}}},{"Icons",Json::array()},{"Records",Json::array({original})}};
    assert(HelpyIconPack::Parse(document,0).Records.size()==1);document["Records"][0]["Path"]="/Other/Bad.Bad";Reject([&]{HelpyIconPack::Parse(document,0);});
    // Native baseline bundles remain useful with optional native-namespace patches.
    auto patchedPlan=plan;patchedPlan.Owners["game"]={"example"};patchedPlan.DeclaredOwners["game"]={"example"};
    auto patched=HelpySourceCache::Inspect(root,patchedPlan);patched.DeclaredOwners=patchedPlan.DeclaredOwners;
    const auto nativeKeys=HelpyIconPack::Portable(root,patched,{"game"},cache,progress,true);
    const auto moddedKeys=HelpyIconPack::Portable(root,patched,{"game"},cache,progress);
    assert(nativeKeys.at("game")!=moddedKeys.at("game"));
    auto base=original;base["Owner"]="core";base["Source"]="game";base["Path"]="/Game/Items/DA_Tea.DA_Tea";
    std::vector<char> pixel(20);std::memcpy(pixel.data(),"RSI3",4);const uint32_t dimensions[]={1,1,4};std::memcpy(pixel.data()+4,dimensions,12);
    const std::string art="/Game/Art/T_Tea.T_Tea";
    Json nativeDoc={{"Version",2},{"Kind","RuneSchemaCacheBundle"},{"Locale",locale},{"Sources",nativeKeys},{"Records",Json::array({base})},
        {"Icons",Json::array({{{"Icon",art},{"Source","game"},{"SHA256",HelpyIconPack::Hash(pixel)},{"Bytes",pixel.size()},{"Origins",{"core"}}}})}};
    const auto nativeFile=fixture/"native.rsmk-cache";const auto nativeText=nativeDoc.dump();
    {std::ofstream out(nativeFile,std::ios::binary);const uint32_t length=uint32_t(nativeText.size());out.write(HelpyIconPack::BundleMagic,8);out.write(reinterpret_cast<const char*>(&length),4);out.write(nativeText.data(),nativeText.size());out.write(pixel.data(),pixel.size());}
    const auto nativeDestination=fixture/"native-import"/"item-icons";
    const auto nativeImported=HelpyIconPack::Import(nativeFile,root,nativeDestination,patched,1024*1024,progress);
    assert(nativeImported.RecordsImported==1&&nativeImported.Imported==1);
    const auto baselineSidecar=HelpyIconPack::Json(HelpyIconPack::Read(HelpyIconPack::Target(nativeDestination,patched,art).wstring()+L".json",8192));
    assert(baselineSidecar.at("DisplayOnly")==true);
    // All paths generated from digests; no arbitrary member extraction.
    fs::remove_all(fixture);std::cout<<"Content bundles preserve paths/IDs, isolate owners, retain last-good state and support metadata-only round trips.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

