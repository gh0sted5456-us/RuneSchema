#include "Core/HelpySourceCache.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
int main(int argc,char** argv)try{namespace fs=std::filesystem;using namespace PS::HelpySourceCache;
    if(argc==2){const auto start=std::chrono::steady_clock::now();try{const auto source=Inspect(argv[1]);std::cout<<source.Key<<" files="<<source.Files.size()<<" ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
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
    // Native plugin/GameFeature containers participate in source coherency,
    // including installations with loose feature-plugin directories.
    const auto plugin=base/"scoped"/"Plugins"/"GameFeatures"/"Brynmoor"/"Content"/"Paks"/"Brynmoor.pak";
    fs::create_directories(plugin.parent_path());{std::ofstream f(plugin);f<<"first native plugin pixels";}
    const auto featureBefore=Inspect(scoped,plan);assert(featureBefore.SourceKeys!=loaderChanged.SourceKeys);
    const auto descriptor=plugin.parent_path().parent_path().parent_path()/"Brynmoor.uplugin";
    {std::ofstream f(descriptor);f<<"{}";}
    auto nativeSources=Inspect(scoped,plan);assert(nativeSources.NativePlugins.contains("brynmoor"));
    Current={};assert(AcceptRemote(Document(nativeSources)));assert(Current.NativePlugins.contains("brynmoor"));
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
    assert(PS::HelpyCacheSources::Source("/Game/RuneSchema/MaxCape/ITEM_Cape.ITEM_Cape")=="game/runeschema/maxcape");
    assert(PS::HelpyCacheSources::Source("/../outside/item.item").empty());
    PS::HelpyCacheSources::Reset();PS::HelpyCacheSources::Declare("First","/FirstMount/Items/A.A");
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    assert(!PS::HelpyCacheSources::Snapshot().Registered.contains("sharedicons"));
    PS::HelpyCacheSources::Registered("/FirstMount/Items/A.A");
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    const auto referenced=PS::HelpyCacheSources::Snapshot();assert(referenced.Registered.contains("sharedicons"));
    assert(referenced.Owners.at("sharedicons").contains("first"));
    PS::HelpyCacheSources::Reference("/FirstMount/Items/A.A","/SharedIcons/Icons/A.A");
    assert(PS::HelpyCacheSources::Snapshot().Revision==referenced.Revision);
    fs::remove_all(NativePath(base));
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
