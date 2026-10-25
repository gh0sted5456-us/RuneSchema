#include "Core/HelpyIconPack.h"
#include "Runtime/HelpyDownloads.h"
#include "Runtime/HelpyPaths.h"
#include <iostream>
#include <thread>

// Explicit integration probe; never part of the offline/default CTest suite.
int main(int argc,char** argv){try{
    if(argc!=5)throw std::runtime_error("Usage: helpy-base-cache-probe RuneSchema-root isolated-output bundle-or-download game-version");
    using namespace PS;namespace fs=std::filesystem;
    const auto root=fs::absolute(argv[1]),output=fs::absolute(argv[2]);
    fs::create_directories(output/"local-appdata");
    if(!SetEnvironmentVariableW(L"LOCALAPPDATA",(output/"local-appdata").c_str()))throw std::runtime_error("Cannot isolate cache");
    HelpyPaths::Configure(root/"plugins"/"RuneSchema.Helpy");
    fs::path file=argv[3];
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Registered={"game","engine"};
    // Index native PAKs and mod ownership without launching the game.
    auto discovered=HelpySourceCache::Inspect(root);
    for(const auto& [path,owners]:*discovered.ContainerAssets){const auto group=HelpyCacheSources::Source(path);plan.Registered.insert(group);
        for(const auto& owner:owners)if(owner.starts_with("mods/")){plan.Owners[group].insert(owner.substr(5));plan.DeclaredOwners[group].insert(owner.substr(5));}}
    for(const auto& provider:discovered.NativePlugins)plan.Registered.insert(provider);
    const auto snapshot=HelpySourceCache::Inspect(root,plan);
    const bool download=file=="download";
    if(download){
        if(!HelpyDownloads::StartBaseCache(snapshot.SourceKeys,snapshot.NativePlugins,"Steam",argv[4]))throw std::runtime_error("Download did not start");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
        do{HelpyDownloads::Poll();std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Download timed out");
        }while(HelpyDownloads::Busy());
        file=HelpyDownloads::TakeBaseCache();
        if(file.empty())throw std::runtime_error(HelpyDownloads::Current().Status);
    }
    const auto cache=output/"cache"/"item-icons";
    const auto bundleDigest=HelpyIconPack::Hash(HelpyIconPack::Read(file,HelpyIconPack::MaxPack));
    HelpyIconPack::Progress progress;
    const auto policy=download?HelpyIconPack::ImportPolicy::NativeArtworkFallback:HelpyIconPack::ImportPolicy::VerifiedContent;
    const auto first=HelpyIconPack::Import(file,root,cache,snapshot,256ull*1024*1024,progress,policy);
    if(!(first.Imported||first.Unchanged||first.RecordsImported))throw std::runtime_error("No compatible content imported: "+first.Message);
    const auto records=CacheBundleStore::Read(cache,root,snapshot,CacheBundleStore::Locale());
    const auto second=HelpyIconPack::Import(file,root,cache,snapshot,256ull*1024*1024,progress,policy);
    if(second.Imported||second.RecordsImported)throw std::runtime_error("Repeated import was not idempotent");
    const nlohmann::json receipt={{"BundleSHA256",bundleDigest},{"ImportedIcons",first.Imported},{"UnchangedIcons",first.Unchanged},{"ImportedRecords",first.RecordsImported},
        {"ReadableRecords",records.size()},{"Skipped",first.Skipped},{"RepeatUnchangedIcons",second.Unchanged},{"RepeatNewRecords",second.RecordsImported},
        {"Message",first.Message},{"ManagedStagingRemoved",HelpyDownloads::ReleaseImportedPack(file)}};
    std::ofstream(output/"receipt.json")<<receipt.dump(2);std::cout<<receipt.dump(2)<<'\n';
    HelpyDownloads::Shutdown();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';PS::HelpyDownloads::Shutdown();return 1;}}
