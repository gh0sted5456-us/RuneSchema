#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& path){std::ifstream f(path);if(!f)throw std::runtime_error("Placement/cache input missing");return {std::istreambuf_iterator<char>(f),{}};}
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    Check(argc==2,"Expected raw source root");const std::filesystem::path root(argv[1]);
    const auto npc=Read(root/"src/Loader/DragonWildsNpcLoader.cpp");
    const auto human=Read(root/"src/Loader/HumanNpc.inl");
    const auto cleanup=Read(root/"src/Loader/NpcCleanup.inl");
    const auto event=Read(root/"src/Loader/DragonWildsSpawnLoader.cpp");
    const auto catalog=Read(root/"src/Generator/AssetSearch.cpp");
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
    const auto scope=catalog.find("const auto metadataScope=json::array({generation,HelpySourceCache::ForClient().value(\"CatalogKey\",std::string{})}).dump()",metadata);
    const auto refresh=catalog.find("if(!mountedReady||mountedScope!=metadataScope)",scope);
    const auto reset=catalog.find("if(mountedScope!=metadataScope){mountedReady=false;mountedItems.clear();}",refresh);
    const auto ensure=catalog.find("if(!classMetadataReady&&CookedAssets::TryEnsure())",reset);
    const auto committed=catalog.find("mountedReady=true",ensure);
    Check(metadata!=std::string::npos&&scope!=std::string::npos&&refresh!=std::string::npos&&reset!=std::string::npos&&ensure!=std::string::npos&&committed!=std::string::npos,"An unready registry is cached as a completed item census or content/world changes fail to invalidate its scope");
    const auto classQuery=catalog.find("RecipeAssetCatalog::MountedPaths(TEXT(\"ItemData\"))");
    const auto emptyGuard=catalog.find("if(paths.empty())throw",classQuery),classCommit=catalog.find("mountedReady=classMetadataReady=true",classQuery);
    Check(classQuery!=std::string::npos&&emptyGuard<classCommit&&classCommit<ensure,"Targeted ItemData metadata must reject an empty/unready registry before committing");
    Check(catalog.find("cache.ReadRecovering(")!=std::string::npos,"Stale alphabetic cursors strand the item list instead of safely replacing the window");
    Check(catalog.find("ItemManifest.Restore(manifest,diskScope,basicCache,legacyScope)")!=std::string::npos&&catalog.find("ItemManifest.Persist(manifest,diskScope,cache)")!=std::string::npos,"Item browsing must restore and retain a verified source-scoped manifest");
    Check(catalog.find("PersistentItemCatalog::DiskScope(published,paths,language)")!=std::string::npos,"Item browsing must use the stable content catalogue fingerprint");
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
    const auto cached=thumbnail.substr(thumbnail.find("std::optional<nlohmann::json> Cached("),thumbnail.find("nlohmann::json Render(")-thumbnail.find("std::optional<nlohmann::json> Cached("));
    Check(cached.find("last_write_time")==std::string::npos&&cached.find("ActiveCapture")==std::string::npos&&cached.find("MaintainCache")==std::string::npos,"Read-only cache checks must not reset captures or rewrite cache timestamps");
    Check(thumbnail.find("recursive_directory_iterator")==std::string::npos&&thumbnail.find("remove_all")==std::string::npos,"Native icon rendering must not walk or clear the cache on the game thread");
    Check(thumbnail.find("DiskMaintenance.Post(")!=std::string::npos,"Icon maintenance must be delegated to its filesystem worker");
    Check(Read(root/"src/Loader/DragonWildsSpawnLoader.cpp").find("PS::ItemIconThumbnail::ShutdownDiskCache()")!=std::string::npos,"Icon disk worker must join before the DLL unloads");
    for(const auto* phase:{"helpy.icons.cache-read","helpy.icons.cache-write","helpy.icons.asset-load","helpy.icons.draw","helpy.icons.gpu-readback"})Check(thumbnail.find(phase)!=std::string::npos,"Slow native captures need stage attribution");
}
