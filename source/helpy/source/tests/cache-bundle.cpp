#include "Core/HelpyIconPack.h"
#include "Core/CacheBundleItems.h"
#include "Core/HelpyCacheOwnership.h"
#include <cassert>
#include <iostream>
#include <thread>
#undef assert
#define assert(x) do{if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": "+#x);}while(false)
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;
template<class F> void Reject(F fn){bool caught=false;try{fn();}catch(...){caught=true;}assert(caught);}
int main(){try{
    assert(ContentSource::Mod("RSDWSpears")=="mods/rsdwspears");
    assert(ContentSource::Mod("Dye_GoldTrim")=="mods/dye_goldtrim");
    assert(ContentSource::Mod("../Other").empty());
    // Context actions must use the same canonical owner as the source dropdown.
    {std::ifstream input(fs::path(__FILE__).parent_path()/"../include/Generator/ModsPanel.h");
        assert(input.good());const std::string source((std::istreambuf_iterator<char>(input)),{});
        assert(source.find("const auto source=ContentSource::Mod(owner);")!=source.npos);
        size_t count=0,at=0;while((at=source.find("HelpyIconPacks::SelectedSource=source;",at))!=source.npos){++count;++at;}
        assert(count==2&&source.find("HelpyIconPacks::SelectedSource=\"mods/\"+owner;")==source.npos);
        count=0;at=0;while((at=source.find("nullptr,false,!source.empty()",at))!=source.npos){++count;++at;}assert(count==3);
    }
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
    {auto declared=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>(),authoredConflict=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();
        (*declared)[HelpyCacheSources::AssetKey(path)]={"example"};(*authoredConflict)[HelpyCacheSources::AssetKey(path)]={"other"};
        auto conflicting=shared;conflicting.AssetOwners=declared;conflicting.AuthoredAssets=authoredConflict;
        assert(!CacheBundle::OriginalOwnerProven(row,conflicting,{}));}
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
    {auto saved=snapshot;saved.Provisional=true;
        const auto rows=CacheBundleStore::Read(cache,root,saved,locale);
        assert(rows.size()==2&&!rows[0]["Data"].at("CacheSourceVerified").get<bool>());
        assert(CacheBundleStore::Read(cache,root,saved,locale,{},false).empty());
    }
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
    const CacheBundleItems::Index prepared(references);
    assert(prepared.Page("Health","",40)==page&&prepared.Page("missing","",40)["Rows"].empty());
    assert(prepared.Page("Health","",40)==page); // Repeated searches reuse the sorted rows and stable cursor identity.
    auto stoneReference=references[0];stoneReference["Data"]={{"Path","/Game/Items/ITEM_Stone.ITEM_Stone"},{"Name","Stone"},{"Category","Building"},{"Tags","Item.BuildingResource ItemFilter.Material.Mineral"}};
    const auto stonePage=CacheBundleItems::Page(Json::array({stoneReference}),"__helpycat:resources stone","",40);
    assert(stonePage["Rows"].size()==1&&stonePage["Rows"][0]["Category"]=="Resources");
    {
        Json set=Json::array();int id=0;
        for(const auto* name:{"Hard Leather Body (Dyed Black, Mirror Trim)","Hard Leather Chaps (Dyed Black, Mirror Trim)","Hard Leather Cowl (Dyed Black, Mirror Trim)","Hard Leather Cowl (Dyed Blue, Mirror Trim)","Hard Leather Chaps (Dyed Black, Gold Trim)"}){
            auto reference=references[0];reference["Data"]={{"Path","/ExampleMount/Items/Armor"+std::to_string(++id)},{"Name",name},{"Category","Armor"}};set.push_back(std::move(reference));
        }
        const CacheBundleItems::Index armor(set);
        const auto query="__helpyfamily:"+ItemFamily::Key("Hard Leather Body (Dyed Black, Mirror Trim)");
        const auto first=armor.Page(query,"",2),last=armor.Page(query,first.at("NextCursor"),2);
        assert(first.at("Rows").size()==2&&first.at("HasMore")==true);
        assert(last.at("Rows").size()==1&&last.at("HasMore")==false);
        assert(first.at("Rows")[0].at("Name")=="Hard Leather Body (Dyed Black, Mirror Trim)");
        assert(first.at("Rows")[1].at("Name")=="Hard Leather Chaps (Dyed Black, Mirror Trim)");
        assert(last.at("Rows")[0].at("Name")=="Hard Leather Cowl (Dyed Black, Mirror Trim)");
    }
    const auto bundle=fixture/"Example.rsmk-cache";HelpyIconPack::Progress progress;
    const auto exported=HelpyIconPack::Export(bundle,root,cache,snapshot,progress,{},Json::object(),Json::array({original,journal}));
    assert(exported.Exported==0&&exported.RecordsExported==2);
    {std::ifstream file(bundle,std::ios::binary);uint64_t start=0;const auto manifest=HelpyIconPack::Header(file,start,fs::file_size(bundle));assert(manifest.Version==2&&manifest.Records.size()==2&&manifest.Locale==locale);}
    const auto destination=fixture/"import"/"item-icons";
    auto imported=HelpyIconPack::Import(bundle,root,destination,snapshot,1024*1024,progress);assert(imported.RecordsImported==2);
    imported=HelpyIconPack::Import(bundle,root,destination,snapshot,1024*1024,progress);assert(imported.RecordsImported==0);
    assert(CacheBundleStore::Read(destination,root,snapshot,locale).size()==2);
    {
        const auto awaitPage=[&](CacheBundleItems::Store& store){
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            for(;;){if(auto page=store.Lookup(destination,root,snapshot,locale,"","",40))return *page;
                assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        };
        CacheBundleItems::Store first;auto page=awaitPage(first);assert(page.at("Rows").size()==1);
        auto update=row;update.Data["Name"]="Immediately imported item";
        assert(CacheBundleStore::Commit(destination,root,snapshot,locale,{update})==1);
        first.Invalidate();assert(!first.Lookup(destination,root,snapshot,locale,"","",40));
        page=awaitPage(first);assert(page.at("Rows")[0].at("Name")=="Immediately imported item");first.Shutdown();
        CacheBundleItems::Store reopened;
        page=awaitPage(reopened);assert(page.at("Rows")[0].at("Name")=="Immediately imported item");reopened.Shutdown();
    }
    imported=HelpyIconPack::Import(bundle,root,fixture/"denied"/"item-icons",snapshot,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{},{},"mods/other");assert(imported.RecordsImported==0);
    imported=HelpyIconPack::Import(bundle,root,fixture/"visuals"/"item-icons",snapshot,1024*1024,progress,HelpyIconPack::ImportPolicy::ServerDisplayOnly);assert(imported.RecordsImported==0);
    auto document=Json{{"Version",2},{"Kind","RuneSchemaCacheBundle"},{"Locale",locale},{"Sources",{{"examplemount",std::string(64,'0')}}},{"Icons",Json::array()},{"Records",Json::array({original})}};
    assert(HelpyIconPack::Parse(document,0).Records.size()==1);document["Records"][0]["Path"]="/Other/Bad.Bad";Reject([&]{HelpyIconPack::Parse(document,0);});
    {
        const std::string borrowedIcon="/Game/Art/T_Health.T_Health",unrelatedIcon="/Game/Art/T_Unrelated.T_Unrelated";
        std::vector<char> thumb(20,100);std::memcpy(thumb.data(),"RSI3",4);const uint32_t dimensions[]{1,1,4};std::memcpy(thumb.data()+4,dimensions,12);
        for(const auto& icon:{borrowedIcon,unrelatedIcon}){const auto target=HelpyIconPack::Target(cache,snapshot,icon);HelpyIconPack::Publish(target,thumb);HelpyIconPack::Index(target,icon,{"core"});}
        const auto targeted=fixture/"Example-targeted.rsmk-cache";
        assert(HelpyIconPack::Export(targeted,root,cache,snapshot,progress,"mods/example",Json::object(),Json::array({original,journal})).Exported==1);
        {std::ifstream file(targeted,std::ios::binary);uint64_t start=0;const auto manifest=HelpyIconPack::Header(file,start,fs::file_size(targeted));assert(manifest.Icons.size()==1&&manifest.Icons[0].Icon==borrowedIcon&&manifest.Icons[0].Origins==std::set<std::string>{"core"}&&manifest.Records.size()==2);}
        const auto targetCache=fixture/"targeted-import"/"item-icons";
        const auto targetedImport=HelpyIconPack::Import(targeted,root,targetCache,snapshot,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{}, {"examplemount"},"mods/example");
        assert(targetedImport.Imported==1&&targetedImport.RecordsImported==2&&!fs::exists(HelpyIconPack::Target(targetCache,snapshot,unrelatedIcon)));
        auto sharedSnapshot=snapshot;sharedSnapshot.DeclaredOwners["examplemount"].insert("other");
        Reject([&]{HelpyIconPack::Export(fixture/"unproven.rsmk-cache",root,cache,sharedSnapshot,progress,"mods/example",Json::object(),Json::array({original,journal}));});
        auto exact=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();(*exact)[HelpyCacheSources::AssetKey(path)]={"example"};sharedSnapshot.AssetOwners=exact;
        const auto proven=HelpyIconPack::Export(fixture/"proven.rsmk-cache",root,cache,sharedSnapshot,progress,"mods/example",Json::object(),Json::array({original,journal}));assert(proven.RecordsExported==1&&proven.Exported==1);
        // The remote DLL must recover exact declarations, including original
        // artwork, without borrowing another mod's assets in the same mount.
        const std::string ownArt="/ExampleMount/Art/T_Original.T_Original",otherArt="/ExampleMount/Art/T_Other.T_Other";
        for(const auto& icon:{ownArt,otherArt}){const auto target=HelpyIconPack::Target(cache,snapshot,icon);HelpyIconPack::Publish(target,thumb);HelpyIconPack::Index(target,icon,{icon==ownArt?"mods/example":"mods/other"});}
        (*exact)[HelpyCacheSources::AssetKey(ownArt)]={"example"};(*exact)[HelpyCacheSources::AssetKey(otherArt)]={"other"};
        auto remote=sharedSnapshot;remote.AssetOwners.reset();remote.AuthoredAssets.reset();
        const auto verified=HelpyCacheOwnership::Fetch(remote,"mods/example",[&](const Json& q,Json& r){r=HelpyCacheOwnership::Page(sharedSnapshot,q);return true;});
        const auto remotePack=fixture/"remote-owned.rsmk-cache";
        const auto remoteResult=HelpyIconPack::Export(remotePack,root,cache,verified,progress,"mods/example",Json::object(),Json::array({original,journal}));
        assert(remoteResult.RecordsExported==1&&remoteResult.Exported==2);
        const auto remoteImport=HelpyIconPack::Import(remotePack,root,fixture/"remote-import"/"item-icons",verified,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{},{"examplemount"},"mods/example");
        assert(remoteImport.Imported==2&&remoteImport.RecordsImported==1);
        assert(!fs::exists(HelpyIconPack::Target(fixture/"remote-import"/"item-icons",verified,otherArt)));
        {std::ofstream changed(mod);changed<<"{\"changed\":true}";}const auto changed=HelpySourceCache::Inspect(root,plan);
        const auto skipped=HelpyIconPack::Import(targeted,root,fixture/"changed-import"/"item-icons",changed,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{}, {"examplemount"},"mods/example");assert(skipped.Imported==0&&skipped.RecordsImported==0);
        {std::ofstream restored(mod);restored<<"{}";}
    }
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
    // A new cook can reuse native pictures at proven existing paths, without IDs.
    {const auto gameFile=root.parent_path().parent_path().parent_path().parent_path().parent_path()/"Content"/"Paks"/"Game.pak";
        assert(fs::is_regular_file(gameFile));{std::ofstream out(gameFile);out<<"new native cook";}
        auto next=HelpySourceCache::Inspect(root,patchedPlan);
        auto exact=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();(*exact)[HelpyCacheSources::AssetKey(art)]={"core"};next.ContainerAssets=exact;
        const auto references=fixture/"native-artwork-fallback"/"item-icons";
        auto fallback=HelpyIconPack::Import(nativeFile,root,references,next,1024*1024,progress,HelpyIconPack::ImportPolicy::NativeArtworkFallback);
        assert(fallback.Imported==1&&fallback.RecordsImported==0&&fallback.Message.find("Different game cook")!=std::string::npos);
        assert(HelpyIconPack::Json(HelpyIconPack::Read(HelpyIconPack::Target(references,next,art).wstring()+L".json",8192)).at("DisplayOnly")==true);
        assert(CacheBundleStore::Read(references,root,next,locale).empty());
        (*exact)[HelpyCacheSources::AssetKey(art)]={"mods/example"};
        fallback=HelpyIconPack::Import(nativeFile,root,fixture/"no-mod-override"/"item-icons",next,1024*1024,progress,HelpyIconPack::ImportPolicy::NativeArtworkFallback);
        assert(fallback.Imported==0&&fallback.RecordsImported==0);
        exact->clear();fallback=HelpyIconPack::Import(nativeFile,root,fixture/"no-missing-path"/"item-icons",next,1024*1024,progress,HelpyIconPack::ImportPolicy::NativeArtworkFallback);
        assert(fallback.Imported==0&&fallback.RecordsImported==0);
    }
    {
        const auto mapCache=fixture/"map-source"/"item-icons",serverCache=fixture/"map-server"/"item-icons";
        std::string pixels(24,'\0');std::memcpy(pixels.data(),"RSI3",4);const uint32_t width=2,height=1,count=8;
        std::memcpy(pixels.data()+4,&width,4);std::memcpy(pixels.data()+8,&height,4);std::memcpy(pixels.data()+12,&count,4);
        MapProjection::Projection projection;projection.Center={100,200};projection.Right={1,0};projection.Up={0,1};projection.HalfWidth=1000;projection.HalfHeight=2000;projection.MinZ=-100;projection.MaxZ=100;
        const Json metadata={{"Version",1},{"Asset",MapPresentationCache::Asset},{"Projection",projection.ToJson()},{"Bytes",pixels.size()},{"SHA256",CacheBundleIO::Hash(pixels)}};
        auto worldData=metadata;worldData["Players"]={"private player"};worldData["World"]="private world";
        assert(!MapPresentationCache::Metadata(worldData).contains("Players")&&!MapPresentationCache::Metadata(worldData).contains("World"));
        MapPresentationCache::Install(mapCache,metadata,pixels);const auto before=MapPresentationCache::Reference(mapCache);
        assert(!before.empty()&&MapPresentationCache::Bytes(mapCache,before)==pixels);
        auto bad=metadata;bad["Projection"]["HalfWidth"]=0;Reject([&]{MapPresentationCache::Install(mapCache,bad,pixels);});
        auto corrupt=pixels;corrupt.back()='x';Reject([&]{MapPresentationCache::Install(mapCache,metadata,corrupt);});
        Reject([&]{MapPresentationCache::Pixels(std::string_view(pixels).substr(0,20));});
        assert(MapPresentationCache::Reference(mapCache)==before);
        const auto current=HelpySourceCache::Inspect(root,plan);const auto file=fixture/"world-map.rsmk-cache";
        HelpyIconPack::Progress progress;const auto exported=HelpyIconPack::Export(file,root,mapCache,current,progress);
        assert(exported.Message.find("World map and alignment included")!=std::string::npos);
        HelpySourceCache::Snapshot idleServer;
        const auto imported=HelpyIconPack::Import(file,root,serverCache,idleServer,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);
        assert(imported.MapImported&&MapPresentationCache::Bytes(serverCache,MapPresentationCache::Reference(serverCache))==pixels);
        assert(MapPresentationCache::Reference(serverCache)["Projection"]==projection.ToJson());
        auto bytes=HelpyIconPack::Read(file,HelpyIconPack::MaxPack);bytes.back()='x';HelpyIconPack::Publish(fixture/"bad-map.rsmk-cache",bytes);
        const auto installed=MapPresentationCache::Reference(serverCache);
        Reject([&]{HelpyIconPack::Import(fixture/"bad-map.rsmk-cache",root,serverCache,idleServer,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);});
        assert(MapPresentationCache::Reference(serverCache)==installed);
        Reject([&]{HelpyIconPack::Import(file,root,serverCache,idleServer,16,progress,HelpyIconPack::ImportPolicy::Presentation);});
        HelpyPresentationCache::Group group;group.Owner="mods/example";group.Fingerprint=CacheBundleIO::Hash("saved-source");group.Locale=locale;
        group.Records=Json::array({CacheBundle::Document(row)});const std::string icon="/ExampleMount/Art/T_Saved.T_Saved";
        group.Icons[icon]={{"SHA256",CacheBundleIO::Hash(pixels)},{"Bytes",uint64_t(pixels.size())}};
        HelpyPresentationCache::Commit(serverCache,{group},1024*1024,[&](const auto&){return std::vector<char>(pixels.begin(),pixels.end());});
        idleServer.Provisional=true;idleServer.Key="changed";
        const auto savedFile=fixture/"current-cache.rsmk-cache",renamedFile=fixture/"renamed-cache.rsmk-cache";
        const auto saved=HelpyIconPack::Export(savedFile,fixture/"game-removed",serverCache,idleServer,progress,{},Json::object(),Json::array(),true);
        assert(saved.RecordsExported==1&&saved.Exported==1);
        idleServer.Key="another-layout";
        HelpyIconPack::Export(renamedFile,fixture/"game-renamed",serverCache,idleServer,progress,{},Json::object(),Json::array(),true);
        assert(HelpyIconPack::Read(savedFile,HelpyIconPack::MaxPack)==HelpyIconPack::Read(renamedFile,HelpyIconPack::MaxPack));
        const auto destination=fixture/"saved-reimport"/"item-icons";
        const auto restored=HelpyIconPack::Import(savedFile,root,destination,idleServer,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);
        assert(restored.RecordsImported==1&&restored.Imported==1&&restored.MapImported);
        const auto cached=HelpyPresentationCache::Records(destination,locale);assert(cached.size()==1&&!CacheBundleStore::Browsable(cached[0],idleServer));
        Reject([&]{HelpyIconPack::Import(savedFile,root,destination,idleServer,1024*1024,progress,HelpyIconPack::ImportPolicy::VerifiedContent);});
    }
    // All paths generated from digests; no arbitrary member extraction.
    fs::remove_all(fixture);std::cout<<"Content bundles preserve paths/IDs, isolate owners, retain last-good state and support metadata-only round trips.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

