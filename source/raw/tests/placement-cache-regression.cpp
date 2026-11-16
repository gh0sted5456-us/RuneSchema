#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <iostream>
static std::string Read(const std::filesystem::path& path){std::ifstream f(path);if(!f)throw std::runtime_error("Placement/cache input missing");return {std::istreambuf_iterator<char>(f),{}};}
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    Check(argc==2,"Expected raw source root");const std::filesystem::path root(argv[1]);
    const auto npc=Read(root/"src/Loader/DragonWildsNpcLoader.cpp");
    const auto human=Read(root/"src/Loader/HumanNpc.inl");
    const auto cleanup=Read(root/"src/Loader/NpcCleanup.inl");
    const auto event=Read(root/"src/Loader/DragonWildsSpawnLoader.cpp");
    const auto catalog=Read(root/"src/Generator/AssetSearch.cpp");
    const auto searches=Read(root/"src/Runtime/SearchJobs.cpp");
    Check(searches.find("ForEachUObject")==std::string::npos&&searches.find("scanned<512")!=std::string::npos&&searches.find("microseconds(500)")!=std::string::npos,"Optional runtime searches no longer yield between bounded object slices");
    const auto exportJob=searches.find("writer=std::async");const auto exportEnd=searches.find("});",exportJob);
    const auto exportBody=searches.substr(exportJob,exportEnd-exportJob);
    Check(exportJob!=std::string::npos&&exportBody.find("report.dump(2)")!=std::string::npos&&exportBody.find("UObject")==std::string::npos&&exportBody.find("HostServices")==std::string::npos,"Search report serialization is not an owned worker operation");
    Check(searches.find("writer.wait_for(std::chrono::seconds(0))")!=std::string::npos&&searches.find("Search cancelled on world transition")!=std::string::npos,"Search export backpressure or world reset is missing");
    const auto bulk=Read(root/"src/Loader/IconCacheBuild.inl");
    for(const auto* point:{"FinishSpawningActor","component preflight","ActorComponent class resolution"})
        Check(npc.find(std::string("RequireNpcActor(")+ (std::string(point)=="FinishSpawningActor"?"constructionIdentity,world,\"":"identity,expectedWorld,\"")+point+"\")")!=std::string::npos,"NPC native actor identity is not rechecked after a construction/class boundary");
    Check(npc.find("native IsValid rejected actor")!=std::string::npos&&npc.find("NPC component class is abstract:")!=std::string::npos,"NPC validity and component contract failures are still conflated");
    Check(human.find("Human preview class loading")<human.find("auto* anchor=FindMeshComponent(actor)"),"Human preview loads can leave an unchecked actor token");
    Check(npc.find("definition.LifecycleFailures<2")!=std::string::npos&&cleanup.find("definition->LifecycleFailures<3")!=std::string::npos,"NPC lifecycle retries are not bounded and cleanup acknowledged");
    Check(cleanup.find("retired();")<cleanup.find("it=m_pendingNpcCleanup.erase(it)"),"Collected failed NPCs strand a pending retry");
    const auto spawn=event.substr(event.find("AActor* DragonWildsSpawnLoader::SpawnEventAI("),event.find("void DragonWildsSpawnLoader::HandleNetworkWorldState(")-event.find("AActor* DragonWildsSpawnLoader::SpawnEventAI("));
    for(const auto* proof:{"groundOffset","PrepareEventGrounded(key,world","/Script/Engine.Actor:K2_TeleportTo","if(!relocate.Result<bool>())","EventCapsule(actor)","requireLive();","non-finite actor location"})
        Check(spawn.find(proof)!=std::string::npos,"Event reconciliation bypasses ground/collision/capsule/lifetime verification");
    Check(spawn.find("SetActorLocation(")==std::string::npos,"Event reconciliation uses an unchecked positional write");
    const auto metadata=catalog.find("static std::string mountedScope");
    const auto scope=catalog.find("const auto metadataScope=json::array({generation,HelpySourceCache::PublishedCatalogKey(),HelpySourceCache::Current.Authoritative,HelpySourceCache::Current.Provisional}).dump()",metadata);
    const auto refresh=catalog.find("if(refreshMounted||!mountedReady||mountedScope!=metadataScope)",scope);
    const auto reset=catalog.find("if(mountedScope!=metadataScope){mountedReady=false;mountedItems.clear();}",refresh);
    const auto ensure=catalog.find("if(!classMetadataReady&&CookedAssets::TryEnsure())",reset);
    const auto committed=catalog.find("mountedReady=true",ensure);
    Check(metadata!=std::string::npos&&scope!=std::string::npos&&refresh!=std::string::npos&&reset!=std::string::npos&&ensure!=std::string::npos&&committed!=std::string::npos,"An unready registry is cached as a completed item census or content/world changes fail to invalidate its scope");
    const auto classQuery=catalog.find("RecipeAssetCatalog::MountedPaths(TEXT(\"ItemData\"))");
    const auto emptyGuard=catalog.find("if(paths.empty())throw",classQuery),classCommit=catalog.find("mountedReady=classMetadataReady=true",classQuery);
    Check(classQuery!=std::string::npos&&emptyGuard<classCommit&&classCommit<ensure,"Targeted ItemData metadata must reject an empty/unready registry before committing");
    Check(catalog.find("cache.ReadRecovering(")!=std::string::npos,"Stale alphabetic cursors strand the item list instead of safely replacing the window");
    Check(catalog.find("indexedPaths.emplace(to_string(item->GetPathName()),indexedItems.back())")!=std::string::npos
        &&catalog.find("captured->second.Get()==object")!=std::string::npos
        &&catalog.find("if(Usable(loaded))continue")==std::string::npos,
        "Mounted items loaded after the class snapshot must remain discoverable, including recycled UObject handles");
    Check(catalog.find("ItemManifest.Restore(manifest,diskScope,basicCache,legacyScope)")!=std::string::npos&&catalog.find("ItemManifest.Persist(manifest,diskScope,cache)")!=std::string::npos,"Item browsing must restore and retain a verified source-scoped manifest");
    Check(catalog.find("PersistentItemCatalog::DiskScope(published,pathDigest,language)")!=std::string::npos&&catalog.find("pathDigest=CacheBundleIO::Hash(json(paths).dump())")!=std::string::npos,"Item browsing must use the stable content catalogue and roster fingerprints");
    Check(catalog.find("published.value(\"Authoritative\",false)")<catalog.find("ItemManifest.Restore("),"Saved item rows must wait for the live mounted source manifest");
    Check(npc.find("PS::WorkSlice work")!=std::string::npos&&npc.find("m_npcWorkPending || groundRetryReady")!=std::string::npos,"NPC batches must resume across frames without consuming scan/retry gates");
    Check(bulk.find("MetadataWindow(MetadataAt,128)")!=std::string::npos&&bulk.find("page.Revision!=MetadataRevision")!=std::string::npos,"Icon discovery must use bounded, revision-checked metadata windows");
    for(const auto* proof:{"Waiting for mounted asset registry","F2Catalog::MountedCandidate(record.path,record.name,record.type)","IconReferences(asset)","DiscoveryFailed","Completed with failures"})
        Check(bulk.find(proof)!=std::string::npos,"Bulk icon discovery misses unloaded owners or hides partial failures");
    const auto disk=bulk.find("PS::ItemIconThumbnail::Cached(Icons[At].Path)");
    Check(disk!=std::string::npos&&disk<bulk.find("if(!world){"),"Cached icons must bypass the menu/world renderer gate");
    Check(bulk.find("std::chrono::milliseconds(2)",disk)!=std::string::npos||bulk.find("cacheDeadline")!=std::string::npos,"Disk reuse must remain frame-budgeted");
    Check(bulk.find("if(old!=next)PS::ConfigFiles::Write")!=std::string::npos,"Difference scans rewrite unchanged icon mappings");
    const auto thumbnail=Read(root/"src/Generator/ItemIconThumbnail.cpp");
    const auto buildingRuntime=Read(root/"src/Loader/BuildingPropRuntime.inl");
    for(const auto* proof:{"ManagedBuildingSkipFunction","/Script/Dominion.BaseBuildingActor","ReflectedContract::Count(function)!=1","TrackManagedBuildingSaveExclusion(actor)","token.Actor.Get()!=actor||token.Function.Get()!=function","m_managedBuildingSaveTokens.size()>=4096"})
        Check(buildingRuntime.contains(proof),"Managed buildings require a verified SPUD contract and bounded serial-valid actor/function identities");
    const auto guard=event.substr(event.find("bool DragonWildsSpawnLoader::SetupProtectedBuildingDestroyGuard()"),event.find("void DragonWildsSpawnLoader::LoadSpawns(")-event.find("bool DragonWildsSpawnLoader::SetupProtectedBuildingDestroyGuard()"));
    Check(guard.contains("token.Actor.Get()==source&&token.Function.Get()==function")&&guard.contains("!source->HasAnyFlags(RF_Transient)"),"SPUD save-exclusion override must affect only tracked transient RuneSchema instances");
    Check(guard.find("functionName!=canDestroy")<guard.find("IsProtectedBuildingActor(actor)"),"Unrelated global calls must not scan building ownership");
    Check(thumbnail.contains("PendingWrite=std::async(std::launch::async,[file=cachedFile,pixels=std::move(pixels)]")&&thumbnail.contains("PendingWrite.wait_for(std::chrono::seconds(0))")&&thumbnail.contains("PendingWrite.get()"),"Thumbnail writes must copy owned pixels to a bounded worker and confirm completion without blocking the game tick");
    const auto writeJob=thumbnail.substr(thumbnail.find("PendingWrite=std::async"),thumbnail.find("ActiveCapture.reset();return {{\"Icon\",iconPath}")-thumbnail.find("PendingWrite=std::async"));
    Check(!writeJob.contains("HelpySourceCache::")&&!writeJob.contains("world")&&!writeJob.contains("capture."),"Thumbnail worker must not access mutable engine or source-cache objects");
    const auto tools=Read(root/"src/Loader/SpawnTools.inl"),icons=Read(root/"src/Loader/SpawnItemIcons.inl");
    Check(event.contains("PS::WorkSlice helpyWork")&&tools.contains("work.MayStart()")&&icons.contains("work.MayStart()")&&event.contains("iconsFirst=!iconsFirst"),"Catalogue and icons require a shared optional frame budget without starvation");
    const auto journal=Read(root/"src/Loader/DragonWildsJournalModLoader.cpp"),assets=Read(root/"src/Loader/DragonWildsAssetModLoader.cpp");
    const auto journalStart=journal.find("void DragonWildsJournalModLoader::ObserveAcquisition(");
    Check(journal.find("const auto name=function->GetFName()",journalStart)<journal.find("const auto path=function->GetPathName()",journalStart),"Journal must reject unrelated event names before allocating paths");
    const auto assetStart=assets.find("m_characterMenuPatchHook = Hook::RegisterProcessEventPreCallback");
    Check(assets.find("const auto name=function->GetFName()",assetStart)<assets.find("const auto classPath =",assetStart),"Character-menu observer must filter event names before class paths");
    Check(event.contains("const auto name = function->GetFName();")&&!event.contains("const auto name = function->GetName();"),"Player activity observer must use cached names");
    const auto cached=thumbnail.substr(thumbnail.find("std::optional<nlohmann::json> Cached("),thumbnail.find("nlohmann::json Render(")-thumbnail.find("std::optional<nlohmann::json> Cached("));
    Check(cached.find("last_write_time")==std::string::npos&&cached.find("ActiveCapture")==std::string::npos&&cached.find("MaintainCache")==std::string::npos,"Read-only cache checks must not reset captures or rewrite cache timestamps");
    Check(thumbnail.find("recursive_directory_iterator")==std::string::npos&&thumbnail.find("remove_all")==std::string::npos,"Native icon rendering must not walk or clear the cache on the game thread");
    Check(thumbnail.find("DiskMaintenance.Post(")!=std::string::npos,"Icon maintenance must be delegated to its filesystem worker");
    Check(Read(root/"src/Loader/DragonWildsSpawnLoader.cpp").find("PS::ItemIconThumbnail::ShutdownDiskCache()")!=std::string::npos,"Icon disk worker must join before the DLL unloads");
    for(const auto* phase:{"helpy.icons.cache-read","helpy.icons.cache-write","helpy.icons.asset-load","helpy.icons.draw","helpy.icons.gpu-readback"})Check(thumbnail.find(phase)!=std::string::npos,"Slow native captures need stage attribution");
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
