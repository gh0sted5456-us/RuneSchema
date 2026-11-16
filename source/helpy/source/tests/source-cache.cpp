#include "Core/HelpySourceCache.h"
#include <cassert>
#include <iostream>
#include <thread>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
struct CacheProviderManifest {
    struct Lane {std::vector<std::string> Paths;};
    std::string Owner;std::vector<Lane> AssetLanes;
    std::vector<std::string> WearableMeshData,MeleeAttackClasses,RangedAttackClasses,CacheAssets;
    nlohmann::json LoaderDefinitions=nlohmann::json::object();
};
int main(int argc,char** argv)try{namespace fs=std::filesystem;using namespace PS::HelpySourceCache;
    if(argc==3&&std::string_view(argv[1])=="--native-providers"){
        const auto start=std::chrono::steady_clock::now();const auto providers=PS::NativeContentProviders::Read(argv[2],"RSDragonwilds");
        std::cout<<nlohmann::json{{"Providers",providers},{"ReadMilliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()}}.dump()<<'\n';
        return providers.contains("umbralsands")?0:2;
    }
    if(argc==2){const auto start=std::chrono::steady_clock::now();const auto source=Inspect(argv[1]);
        const auto inspected=std::chrono::steady_clock::now();
        for(int run=0;run<20;++run){const auto reused=Replan(source,{});assert(reused.Key==source.Key&&reused.ContainerAssets==source.ContainerAssets);}
        const auto replanned=std::chrono::steady_clock::now();
        std::cout<<nlohmann::json{{"Key",source.Key},{"Files",source.Files.size()},{"Containers",source.Containers.size()},
            {"PackageAssociations",source.ContainerAssets?source.ContainerAssets->size():0},
            {"FullDiscoveryUs",std::chrono::duration_cast<std::chrono::microseconds>(inspected-start).count()},
            {"MetadataReuseMeanUs",std::chrono::duration_cast<std::chrono::microseconds>(replanned-inspected).count()/20},
            {"Iterations",20},{"MatchingKeysAndSharedIndexes",true}}.dump()<<'\n';return 0;}
    {
        using Index=PS::HelpyCacheSources::Plan::AssetOwnerMap;
        const std::shared_ptr<const Index> published=std::make_shared<Index>(Index{{"/game/item",{"mods/first"}}});
        std::thread::id freedOn;
        std::shared_ptr<const Index> duplicate(new Index(*published),[&](const Index* value){freedOn=std::this_thread::get_id();delete value;});
        auto job=std::async(std::launch::async,[index=std::move(duplicate),published]()mutable{ReuseUnchangedIndex(index,published);return index;});
        assert(job.get()==published&&freedOn!=std::thread::id{}&&freedOn!=std::this_thread::get_id());
        for(const Index changed:{Index{{"/game/item",{"mods/second"}}},Index{},Index{{"/game/item",{"mods/first"}},{"/game/new",{"mods/first"}}}}){
            std::shared_ptr<const Index> next=std::make_shared<Index>(changed);const auto identity=next;
            ReuseUnchangedIndex(next,published);assert(next==identity&&*next==changed);
        }
        std::shared_ptr<const Index> missing;ReuseUnchangedIndex(missing,published);assert(!missing);
        auto present=published;ReuseUnchangedIndex(present,{});assert(present==published);
    }
    const auto base=fs::temp_directory_path()/("helpy-source-"+std::to_string(GetCurrentProcessId()));
    const auto root=base/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema";
    fs::create_directories(root/"mods"/"Custom"/"assets");const auto file=root/"mods"/"Custom"/"assets"/"item.jsonc";
    {std::ofstream f(file);f<<"{\"PowerLevel\":1}";}const auto stamp=fs::last_write_time(file);const auto a=Inspect(root),same=Inspect(root);assert(a.Key==same.Key&&a.Files.size()==1);
    // Physical installation aliases/copies must agree in both storefront lanes.
    for(const auto* lane:{"WinGDK","Win64"}) {
        const auto original=base/"original"/"Binaries"/lane/"ue4ss"/"Mods"/"RuneSchema";
        const auto alias=base/"alias"/"Binaries"/lane/"ue4ss"/"Mods"/"RuneSchema";
        for(const auto& candidate:{original,alias}) {
            fs::create_directories(candidate/"mods"/"Custom"/"assets");
            const auto copy=candidate/"mods"/"Custom"/"assets"/"item.jsonc";
            fs::copy_file(file,copy);fs::last_write_time(copy,stamp);
        }
        const auto identity=Inspect(original);assert(identity.Key==Inspect(alias).Key);
        if(std::string(lane)=="WinGDK")assert(identity.Key==a.Key);else assert(identity.Key!=a.Key);
    }
    const auto longRoot=NativePath(base/std::string(120,'a')/std::string(120,'b')/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema");
    fs::create_directories(longRoot/"mods"/"Custom"/"assets");
    const auto longFile=longRoot/"mods"/"Custom"/"assets"/"item.jsonc";
    fs::copy_file(file,longFile);fs::last_write_time(longFile,stamp);
    assert(Inspect(longRoot).Key==a.Key);
    {std::ofstream f(file);f<<"{\"PowerLevel\":2}";}fs::last_write_time(file,stamp);const auto edit=Inspect(root);assert(edit.Key!=a.Key);
    fs::create_directories(root/"runtime"/"saved");{std::ofstream f(root/"runtime"/"saved"/"ignored.json");f<<"{}";}assert(Inspect(root).Key==edit.Key);
    // Windows directory names are case-insensitive. Cache/export writes must
    // never invalidate registered content merely because a mod uses /Cache.
    fs::create_directories(root/"mods"/"Custom"/"Cache");
    {std::ofstream f(root/"mods"/"Custom"/"Cache"/"manifest.json");f<<"{\"icons\":1}";}
    fs::create_directories(root/"Exports");{std::ofstream f(root/"Exports"/"copy.json");f<<"{}";}
    assert(Inspect(root).Key==edit.Key);
    fs::remove(file);const auto removed=Inspect(root);assert(removed.Key!=edit.Key&&removed.Files.empty());
    const auto list=root/"runeschema.txt";
    {std::ofstream f(list);f<<"Dye:1";}const auto enabled=Inspect(root);const auto listStamp=fs::last_write_time(list);
    {std::ofstream f(list);f<<"Dye:0";}fs::last_write_time(list,listStamp);assert(Inspect(root).Key!=enabled.Key);fs::remove(list);
    fs::create_directories(base/"other"/"Mods"/"RuneSchema");assert(Inspect(base/"other"/"Mods"/"RuneSchema").Key!=removed.Key);
    fs::create_directories(root/"settings");const auto settings=root/"settings"/"settings.ini";
    {std::ofstream f(settings);f<<"[RuneSchema]\nadvancedLogging=true \n";}
    const auto firstIni=Inspect(root);const auto iniStamp=fs::last_write_time(settings);
    {std::ofstream f(settings);f<<"[RuneSchema]\nadvancedLogging=false\n";}fs::last_write_time(settings,iniStamp);
    assert(Inspect(root).Key!=firstIni.Key);fs::remove(settings);assert(Inspect(root).Key==removed.Key);
    Worker=std::async(std::launch::async,[root]{return Inspect(root);});Shutdown();assert(!Worker.valid());
    // Icon identities are source-local. Disabled/unregistered managed folders
    // must not be scanned or cause unrelated sources to be recaptured.
    const auto scoped=base/"scoped"/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema";
    const auto first=scoped/"mods"/"First"/"assets"/"item.jsonc";
    const auto second=scoped/"mods"/"Second"/"assets"/"item.jsonc";
    const auto disabled=scoped/"mods"/"Disabled"/"assets"/"item.jsonc";
    for(const auto& path:{first,second,disabled}){fs::create_directories(path.parent_path());std::ofstream f(path);f<<"{\"Value\":1}";}
    PS::HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Revision=7;
    plan.Registered={"game","firstmount","secondmount"};plan.Owners={{"firstmount",{"first"}},{"secondmount",{"second"}}};
    const auto scopedBefore=Inspect(scoped,plan);assert(scopedBefore.SourceKeys.size()==4);
    assert(scopedBefore.Files.size()==2);assert(!scopedBefore.SourceKeys.contains("disabled"));
    const auto secondTime=fs::last_write_time(second);{std::ofstream f(second);f<<"{\"Value\":2}";}fs::last_write_time(second,secondTime);
    const auto scopedAfter=Inspect(scoped,plan);
    assert(scopedAfter.SourceKeys.at("firstmount")==scopedBefore.SourceKeys.at("firstmount"));
    assert(scopedAfter.SourceKeys.at("game")==scopedBefore.SourceKeys.at("game"));
    assert(scopedAfter.SourceKeys.at("secondmount")!=scopedBefore.SourceKeys.at("secondmount"));
    {std::ofstream f(disabled);f<<"{\"Value\":12345}";}
    assert(Inspect(scoped,plan).Key==scopedAfter.Key);
    // Loader DLL/config churn must not invalidate native texture pixels.
    const auto loader=scoped/"dlls"/"main.dll";fs::create_directories(loader.parent_path());{std::ofstream f(loader);f<<"loader update";}
    const auto loaderChanged=Inspect(scoped,plan);assert(loaderChanged.Key!=scopedAfter.Key);
    assert(loaderChanged.SourceKeys==scopedAfter.SourceKeys);
    const auto exact=scoped/"mods"/"First"/"assets"/"exact.jsonc";
    {std::ofstream f(exact);f<<R"({"/Shared/Items/Bronze.Bronze":{"MaxStackSize":1},"$Comment":"not an asset","/Shared/Items/Invalid.Invalid":false})";}
    const auto exactSources=Inspect(scoped,plan);assert(exactSources.AuthoredAssets&&exactSources.AuthoredAssets->at("/shared/items/bronze")==std::set<std::string>{"first"});
    assert(!exactSources.AuthoredAssets->contains("/shared/items/invalid"));fs::remove(exact);
    // Native plugin/GameFeature containers participate in source coherency,
    // including installations with loose feature-plugin directories.
    const auto plugin=base/"scoped"/"Plugins"/"GameFeatures"/"Brynmoor"/"Content"/"Paks"/"Brynmoor.pak";
    fs::create_directories(plugin.parent_path());{std::ofstream f(plugin);f<<"first native plugin pixels";}
    const auto featureBefore=Inspect(scoped,plan);assert(featureBefore.SourceKeys!=loaderChanged.SourceKeys);
    const auto descriptor=plugin.parent_path().parent_path().parent_path()/"Brynmoor.uplugin";
    {std::ofstream f(descriptor);f<<"{}";}
    auto nativeSources=Inspect(scoped,plan);assert(nativeSources.NativePlugins.contains("brynmoor")&&!nativeSources.NativePlugins.contains(""));
    Current={};assert(AcceptRemote(Document(nativeSources)));assert(Current.NativePlugins.contains("brynmoor"));
    {auto offline=Document(nativeSources);offline["PresentationOnly"]=true;assert(AcceptRemote(offline));assert(Current.PresentationOnly&&Current.Provisional&&!Current.Authoritative);
        assert(!AcceptRemote(offline));assert(AcceptRemote(Document(nativeSources)));assert(!Current.PresentationOnly&&!Current.Provisional&&Current.Authoritative);}
    const auto nativePak=base/"scoped"/"Content"/"Paks"/"scoped-Windows.pak";fs::create_directories(nativePak.parent_path());
    const nlohmann::json packedManifest={{"Contents",nlohmann::json::array({
        {{"File","../../../scoped/Plugins/GameFeatures/UmbralSands/UmbralSands.uplugin"},{"Descriptor",{{"CanContainContent",true}}}},
        {{"File","../../../OtherGame/Plugins/Fake/Fake.uplugin"},{"Descriptor",{{"CanContainContent",true}}}},
        {{"File","../../../scoped/Plugins/NoContent/NoContent.uplugin"},{"Descriptor",{{"CanContainContent",false}}}}
    })}};
    {std::ofstream f(nativePak,std::ios::binary);f<<std::string(128,'\0')<<packedManifest.dump()<<std::string(64,'\0');}
    const auto packaged=Inspect(scoped,plan);assert(packaged.NativePlugins.contains("umbralsands")&&!packaged.NativePlugins.contains("fake")&&!packaged.NativePlugins.contains("nocontent"));
    assert(PS::NativeContentProviders::Read(nativePak,"scoped")==std::set<std::string>{"umbralsands"});
    const auto roguePak=nativePak.parent_path()/"scoped-Mod_P.pak";
    {std::ofstream f(roguePak,std::ios::binary);f<<packedManifest.dump();}
    assert(PS::NativeContentProviders::Read(roguePak,"scoped").empty());fs::remove(roguePak);
    assert(PS::NativeContentProviders::Embedded("{\"Contents\": broken}","scoped").empty());
    fs::remove(nativePak);
    auto invalidNative=Document(nativeSources);invalidNative["NativePlugins"]={"../escape"};
    bool badNative=false;try{AcceptRemote(invalidNative);}catch(...){badNative=true;}assert(badNative);
    fs::remove(descriptor);
    {std::ofstream f(plugin);f<<"updated native plugin pixels";}
    const auto featureAfter=Inspect(scoped,plan);assert(featureAfter.SourceKeys!=featureBefore.SourceKeys);
    fs::remove(plugin);assert(Inspect(scoped,plan).SourceKeys==loaderChanged.SourceKeys);
    assert(!InvalidatesCatalog(Snapshot{},scopedAfter));
    assert(!InvalidatesCatalog(scopedAfter,loaderChanged));
    auto borrowedPlan=plan;borrowedPlan.Owners["game"].insert("first");++borrowedPlan.Revision;
    const auto borrowedMetadata=Inspect(scoped,borrowedPlan);
    assert(borrowedMetadata.SourceKeys!=loaderChanged.SourceKeys&&borrowedMetadata.CatalogKey==loaderChanged.CatalogKey);
    assert(!InvalidatesCatalog(loaderChanged,borrowedMetadata));
    auto addedSource=scopedAfter;addedSource.SourceKeys["another"]="1234567890abcdef";addedSource.Key="fedcba0987654321";
    assert(!InvalidatesCatalog(scopedAfter,addedSource));
    assert(InvalidatesCatalog(scopedBefore,scopedAfter));
    auto removedSource=scopedAfter;removedSource.SourceKeys.erase("secondmount");assert(InvalidatesCatalog(scopedAfter,removedSource));
    Current={};assert(AcceptRemote(Document(scopedAfter)));assert(CatalogInvalidationRevision!=Revision);
    assert(Fingerprint("/FirstMount/Art/T_Item.T_Item")==scopedAfter.SourceKeys.at("firstmount"));
    const auto iconFolder=IconDirectory("/FirstMount/Art/T_Item.T_Item",false);
    assert(iconFolder.parent_path().filename().string().starts_with("firstmount-"));
    assert(iconFolder.filename()==scopedAfter.SourceKeys.at("firstmount"));
    assert(IconDirectory("/Unregistered/Art/T_Item.T_Item",false).empty());
    auto malformed=Document(scopedAfter);malformed["Key"]="1234567890abcdef";malformed["Sources"]["../outside"]="1234567890abcdef";
    bool rejected=false;try{AcceptRemote(malformed);}catch(...){rejected=true;}assert(rejected);
    auto withoutSecond=plan;withoutSecond.Registered.erase("secondmount");++withoutSecond.Revision;
    assert(AcceptRemote(Document(Inspect(scoped,withoutSecond))));assert(RemovalRevision==Revision);
    assert(Fingerprint("/SecondMount/Art/T_Item.T_Item").empty());
    assert(PS::HelpyCacheSources::Source("/Game/Mods/Conjurer/Icons/T_Item.T_Item")=="game/mods/conjurer");
    assert(PS::HelpyCacheSources::Source("/Game/Gameplay/Mods/Example/Icons/I.I")=="game/gameplay/mods/example");
    assert(PS::HelpyCacheSources::Source("/Game/RuneSchema/MaxCape/ITEM_Cape.ITEM_Cape")=="game/runeschema/maxcape");
    assert(PS::HelpyCacheSources::Source("/../outside/item.item").empty());
    PS::HelpyCacheSources::Reset();PS::HelpyCacheSources::Declare("First","/FirstMount/Items/A.A");
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    assert(!PS::HelpyCacheSources::Snapshot().Registered.contains("sharedicons"));
    PS::HelpyCacheSources::Registered("/FirstMount/Items/A.A");
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    assert(!PS::HelpyCacheSources::Snapshot().Complete);PS::HelpyCacheSources::Seal();
    const auto referenced=PS::HelpyCacheSources::Snapshot();assert(referenced.Complete&&referenced.Registered.contains("sharedicons"));
    const auto contentRevision=referenced.ContentRevision;
    {const auto before=Inspect(scoped,referenced);auto updated=referenced;
        updated.Registered.insert("borrowedicons");updated.Owners["borrowedicons"].insert("first");updated.DisplayNames["first"]="First mod";++updated.Revision;
        const auto reused=Replan(before,updated),rescanned=Inspect(scoped,updated);
        assert(Document(reused)==Document(rescanned));assert(reused.ContainerAssets==before.ContainerAssets);
        assert(reused.FileKey==before.FileKey&&reused.CatalogKey==before.CatalogKey);
        ++updated.ContentRevision;bool rejectedContent=false;try{Replan(before,updated);}catch(...){rejectedContent=true;}assert(rejectedContent);
        {std::lock_guard lock(PublishedMutex);Published=Document(reused);}assert(PublishedCatalogKey()==reused.CatalogKey);
    }
    assert(referenced.Owners.at("sharedicons").contains("first"));
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    assert(PS::HelpyCacheSources::Snapshot().Revision==referenced.Revision);
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/OtherIcons/Icons/A.A");
    assert(PS::HelpyCacheSources::Snapshot().ContentRevision==contentRevision);
    {auto stable=Current;stable.Authoritative=true;stable.Key="0123456789abcdef";stable.SourceKeys["game"]="0123456789abcdef";stable.PlanRevision=referenced.Revision;stable.ContentRevision=contentRevision;
        {std::lock_guard lock(PublishedMutex);Published=Document(stable);}assert(ForClient().value("Ready",false)&&ForClient().value("CheckingChanges",false));
        PS::HelpyCacheSources::Declare("NewMod","/NewMount/Items/X.X");assert(!ForClient().value("Ready",true));}
    // An unchanged fingerprint must still advance a metadata-only revision;
    // otherwise ForClient remains unready forever and strands cached browsing.
    Current={};Current.Key="0123456789abcdef";Current.Authoritative=true;Current.SourceKeys["game"]="0123456789abcdef";
    const auto nowPlan=PS::HelpyCacheSources::Snapshot();auto metadataOnly=Current;metadataOnly.PlanRevision=nowPlan.Revision;metadataOnly.ContentRevision=nowPlan.ContentRevision;Current.PlanRevision=nowPlan.Revision-1;
    InspectingRevision=nowPlan.Revision;Worker=std::async(std::launch::async,[metadataOnly]{return metadataOnly;});Worker.wait();
    assert(!Poll(root));assert(Current.PlanRevision==nowPlan.Revision&&ForClient().value("Ready",false));
    assert(!Worker.valid()&&Next-std::chrono::steady_clock::now()>std::chrono::seconds(50));
    {auto remote=Document(Current);remote["PlanRevision"]=Current.PlanRevision+1;remote["ContentRevision"]=Current.ContentRevision+1;
        assert(!AcceptRemote(remote));assert(Current.PlanRevision==remote["PlanRevision"]&&Current.ContentRevision==remote["ContentRevision"]);
    }
    Shutdown();
    {auto partial=Current;partial.Authoritative=true;partial.Complete=false;assert(!Document(partial).value("Ready",true));
        const auto sourceFile=Directory(false)/"sources.json";fs::create_directories(sourceFile.parent_path());{std::ofstream out(sourceFile);out<<"last-good-source-snapshot";}
        Persist(partial);std::ifstream input(sourceFile);std::string retained;std::getline(input,retained);assert(retained=="last-good-source-snapshot");}
    // Accepted sharded PAKs must identify their real provider files, not only
    // unrelated loose quest mods that happen to mention the same mount.
    PS::HelpyCacheSources::Reset();
    CacheProviderManifest provider;provider.Owner="Second";provider.AssetLanes={{{"/SharedMount/Items/I.I","/SharedMount/Items/J.J"}}};
    provider.WearableMeshData={"/WearableMount/M.M"};provider.LoaderDefinitions={{"Recipes",{{"/RecipeMount/R.R",nlohmann::json::object()}}}};
    provider.CacheAssets={"/SharedMount/Icons/Original.Original"};
    PS::HelpyCacheSources::DeclareManifest(provider);
    auto providerPlan=PS::HelpyCacheSources::Snapshot();assert(providerPlan.Registered.empty());
    assert(providerPlan.AssetOwners->at(PS::HelpyCacheSources::AssetKey("/SharedMount/Items/I.I")).contains("second"));
    assert(providerPlan.AssetOwners->at(PS::HelpyCacheSources::AssetKey("/SharedMount/Items/J.J")).contains("second"));
    assert(providerPlan.AssetOwners->at("/sharedmount/icons/original")==std::set<std::string>{"second"});
    assert(providerPlan.Owners.at("sharedmount").contains("second")&&providerPlan.Owners.at("wearablemount").contains("second")&&providerPlan.Owners.at("recipemount").contains("second"));
    PS::HelpyCacheSources::Registered("/SharedMount/Items/I.I");providerPlan=PS::HelpyCacheSources::Snapshot();
    const auto pak=second.parent_path().parent_path()/"paks"/"Second.pak";fs::create_directories(pak.parent_path());{std::ofstream out(pak);out<<"original provider pixels";}
    const auto pakBefore=Inspect(scoped,providerPlan);
    assert(std::any_of(pakBefore.Files.begin(),pakBefore.Files.end(),[](const std::string& value){return value.ends_with("/mods/second/paks/second.pak");}));
    {std::ofstream out(pak);out<<"changed provider texture pixels";}
    assert(Inspect(scoped,providerPlan).SourceKeys.at("sharedmount")!=pakBefore.SourceKeys.at("sharedmount"));
    {
        PS::HelpyCacheSources::Seal();const auto oldPlan=PS::HelpyCacheSources::Snapshot();
        PS::HelpyCacheSources::SetDisplayName("Second","Second's Mod");const auto namedPlan=PS::HelpyCacheSources::Snapshot();
        assert(namedPlan.DisplayNames.at("second")=="Second's Mod"&&namedPlan.ContentRevision==oldPlan.ContentRevision&&namedPlan.Revision>oldPlan.Revision);
        PS::HelpyCacheSources::SetDisplayName("Bad/Owner","Bad");PS::HelpyCacheSources::SetDisplayName("Second","Bad\nLabel");
        assert(PS::HelpyCacheSources::Snapshot().DisplayNames==namedPlan.DisplayNames);
        const auto unnamed=Inspect(scoped,oldPlan),named=Inspect(scoped,namedPlan);
        assert(named.Key==unnamed.Key&&named.CatalogKey==unnamed.CatalogKey&&named.SourceKeys==unnamed.SourceKeys);
        Current={};assert(AcceptRemote(Document(unnamed)));const auto invalidation=CatalogInvalidationRevision;
        assert(AcceptRemote(Document(named))&&Current.DisplayNames==namedPlan.DisplayNames&&CatalogInvalidationRevision==invalidation);
        auto bad=Document(named);bad["ModNames"]["../escape"]="No";bool rejected=false;try{AcceptRemote(bad);}catch(...){rejected=true;}assert(rejected);
        bad=Document(named);bad["ModNames"]["second"]="No\nControl";rejected=false;try{AcceptRemote(bad);}catch(...){rejected=true;}assert(rejected);
    }
    // Borrowed icon mounts reuse verified directories without postponing the
    // periodic filesystem check. Content changes still require full discovery.
    {PS::HelpyCacheSources::Seal();const auto planBefore=PS::HelpyCacheSources::Snapshot();
        Current=Inspect(scoped,planBefore);InspectingRevision=planBefore.Revision;Error.clear();
        const auto authored=Current.AuthoredAssets,assets=Current.ContainerAssets,files=Current.ContainerFiles;
        const auto sourceKey=Current.Key;Next={};Poll(scoped);assert(Worker.valid()&&RefreshingFiles);Worker.wait();Poll(scoped);
        assert(Current.Key==sourceKey&&Current.AuthoredAssets==authored&&Current.ContainerAssets==assets&&Current.ContainerFiles==files);
        Next=std::chrono::steady_clock::now()+std::chrono::seconds(30);const auto deadline=Next;
        PS::HelpyCacheSources::SetDisplayName("First","A new label");const auto planAfter=PS::HelpyCacheSources::Snapshot();
        assert(planAfter.Revision!=planBefore.Revision&&planAfter.ContentRevision==planBefore.ContentRevision);
        Poll(scoped);assert(Worker.valid()&&!RefreshingFiles);Worker.wait();Poll(scoped);
        assert(Current.PlanRevision==planAfter.Revision&&Next==deadline&&!Worker.valid());
        assert(Document(Current)==Document(Inspect(scoped,planAfter)));
        auto invalid=planAfter;++invalid.ContentRevision;
        bool refused=false;try{Replan(Current,invalid);}catch(...){refused=true;}assert(refused);
        // A label discovered while a full scan runs must not throw away its indexes.
        const auto completed=Current;PS::HelpyCacheSources::SetDisplayName("First","An even newer label");
        Worker=std::async(std::launch::async,[completed]{return completed;});Worker.wait();RefreshingFiles=true;
        Poll(scoped);assert(Current.DisplayNames.at("first")=="An even newer label");Shutdown();
    }
    // The host must expose saved references while its live file census is
    // pending, without publishing them as a confirmed native source contract.
    {
        const auto live=Inspect(scoped,PS::HelpyCacheSources::Snapshot());
        std::promise<nlohmann::json> saved;CachedSourcesWorker=saved.get_future();
        std::promise<Snapshot> census;Worker=census.get_future();CachedSourcesAttempted=true;
        Current={};Published={{"Ready",false}};saved.set_value(Document(live));
        assert(Poll(scoped)&&Current.Provisional&&Current.SourceKeys==live.SourceKeys);
        assert(!Document(Current).at("Ready").get<bool>()&&!ForClient().at("Ready").get<bool>());
        const auto provisionalRevision=Revision;census.set_value(live);
        assert(Poll(scoped)&&!Current.Provisional&&Revision>provisionalRevision);
        assert(Document(Current).at("Ready").get<bool>());Shutdown();
    }
    // Version hints request one fresh file census without changing the cached
    // content identity or discarding presentation while that census is pending.
    {
        PS::HelpyCacheSources::SetDisplayName("First","First mod","1");
        const auto initialPlan=PS::HelpyCacheSources::Snapshot();Current=Inspect(scoped,initialPlan);
        {std::lock_guard lock(PublishedMutex);Published=Document(Current);}
        const auto keys=Current.SourceKeys,cachedVersions=Current.Versions;const auto invalidation=CatalogInvalidationRevision;
        InspectingRevision=initialPlan.Revision;Next=std::chrono::steady_clock::now()+std::chrono::seconds(30);Error.clear();
        PS::HelpyCacheSources::SetDisplayName("First","First mod","2");Poll(scoped);
        assert(Worker.valid()&&RefreshingFiles&&Current.Versions==cachedVersions);
        assert(Ready()&&ForClient().value("Ready",false)&&ForClient().value("CheckingChanges",false));
        const auto pendingRevision=PS::HelpyCacheSources::Snapshot().Revision;
        PS::HelpyCacheSources::SetDisplayName("First","First mod","2");assert(PS::HelpyCacheSources::Snapshot().Revision==pendingRevision);
        Worker.wait();assert(Poll(scoped));assert(!Worker.valid()&&Current.Versions.at("first")=="2"&&Current.SourceKeys==keys);
        assert(CatalogInvalidationRevision==invalidation&&!ForClient().value("CheckingChanges",true));
        Poll(scoped);assert(!Worker.valid());
        auto document=Document(Current);assert(document.at("UpdateAware").get<bool>()&&document.at("ModVersions").at("first")=="2");
        for(const nlohmann::json invalid:{nlohmann::json{{"first",123}},nlohmann::json{{"../escape","3"}},nlohmann::json{{"first","bad\nversion"}},nlohmann::json{{"first",std::string(129,'v')}}}){
            auto bad=document;bad["ModVersions"]=invalid;bool rejected=false;try{AcceptRemote(bad);}catch(...){rejected=true;}assert(rejected);
        }
        auto legacy=document;legacy.erase("ModVersions");legacy.erase("UpdateAware");Current={};assert(AcceptRemote(legacy));
        assert(Current.Versions.empty());assert(AcceptRemote(document)&&Current.Versions.at("first")=="2");
        PS::HelpyCacheSources::SetDisplayName("First","First mod","3");Poll(scoped);Worker.wait();
        PS::HelpyCacheSources::SetDisplayName("First","First mod","4");assert(!Poll(scoped));assert(Current.Versions.at("first")=="2");
        Poll(scoped);assert(Worker.valid()&&RefreshingFiles);Worker.wait();assert(Poll(scoped)&&Current.Versions.at("first")=="4");Shutdown();
    }
    fs::remove_all(NativePath(base));
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
