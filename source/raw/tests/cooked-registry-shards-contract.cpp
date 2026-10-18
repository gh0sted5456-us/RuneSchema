#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing shard contract input");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==3,"Expected registry loader and manifest implementation");
    const auto loader=Read(argv[1]),manifest=Read(argv[2]);
    Check(loader.find("for(const auto& owner:PS::MountedModRegistryOwners::Snapshot())")<loader.find("if(metadataReady)for(auto& asset:assets)"),
        "Conventional root discovery must not depend on metadata or enumeration order");
    Check(loader.contains("if(!enabledOwners.contains(root.Owner))return;")&&loader.contains("ReservedShardPath(path)"),
        "Disabled roots and independently discovered children must not enroll content");
    Check(loader.contains("readAsset(ActorHelper::ResolveObject(PS::ToWideSafe(child.c_str())),child,root.Owner)"),
        "Shards must be loaded by exact object path with an explicit expected owner");
    const auto stage=loader.find("auto nextEntries=m_modEntries,nextAudit=m_audit"),publish=loader.find("PublishBatch(group.Manifests)");
    Check(stage!=std::string::npos&&stage<publish&&publish<loader.find("m_modEntries.swap(nextEntries)"),
        "Root presentations and native declarations must stage before publication");
    Check(loader.contains("m_keys.swap(previousKeys)")&&loader.contains("RegistryEnrollment::Reject(\"pak:\"+path,error.what())"),
        "Failed roots must roll back presentation keys and veto orphan cleanup");
    const auto cacheOwnership=loader.find("for(const auto& manifest:group.Manifests)PS::HelpyCacheSources::DeclareManifest(manifest)");
    Check(cacheOwnership!=std::string::npos&&cacheOwnership>loader.find("catch(...){m_keys.swap(previousKeys);throw;}"),
        "Sharded PAK cache ownership must publish after the complete registry transaction succeeds");
    Check(manifest.contains("auto next=Detail::Manifests")&&manifest.contains("MergeDefinitions(next,lane)")
        &&manifest.find("MergeDefinitions(next,lane)")<manifest.find("Detail::Manifests.swap(next)"),
        "Conflicting shard declarations must not partially publish");
}
