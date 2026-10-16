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
    const auto ensure=catalog.find("if(CookedAssets::TryEnsure())",catalog.find("static uint64_t mountedGeneration"));
    const auto committed=catalog.find("mountedReady=true",ensure);
    Check(ensure!=std::string::npos&&committed!=std::string::npos&&catalog.find("if(!mountedReady||mountedGeneration!=generation)")<ensure,"An unready registry is cached as a completed item census");
    for(const auto* proof:{"Waiting for mounted asset registry","F2Catalog::MountedCandidate(record.path,record.name,record.type)","IconReferences(asset)","DiscoveryFailed","Completed with failures"})
        Check(bulk.find(proof)!=std::string::npos,"Bulk icon discovery misses unloaded owners or hides partial failures");
}
