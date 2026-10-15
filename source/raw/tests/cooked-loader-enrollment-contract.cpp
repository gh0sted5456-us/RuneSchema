#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
static void Require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
int main(int argc,char** argv) {
    Require(argc==6,"Expected main, recipe, journal, registry loader and registrar");
    const auto main=Read(argv[1]),recipe=Read(argv[2]),journal=Read(argv[3]),registry=Read(argv[4]),registrar=Read(argv[5]);
    const auto import=main.find("->ImportCookedDefinitions();");
    Require(import!=std::string::npos && import<main.find("PrefetchCookedAssets(plan)")
        && import<main.find("->PrepareReferences();"),"Cooked definitions must precede recipe reference consumers");
    Require(main.contains("required loader is disabled: ") && main.contains("RegistryEnrollment::Reject"),
        "Disabling a required loader must not make enabled cooked IDs appear orphaned");
    Require(recipe.contains("CookedPakRegistryManifest::Definitions(\"Recipes\")")
        && recipe.contains("existingPaths.contains(path)") && recipe.contains("ParsePlacements(definition.Body)"),
        "Cooked recipe metadata must use ordinary placement parsing with loose override precedence");
    Require(journal.contains("m_loreOnly?\"Lore\":\"Journals\"")
        && journal.contains("CookedPakRegistryManifest::Definitions(lane)") && journal.contains("QueueData(nlohmann::json{{definition.Path,definition.Body}}"),
        "Journal and lore must reuse native loader registration/unlock logic");
    Require(recipe.contains("RegistryEnrollment::Reject") && journal.contains("RegistryEnrollment::Reject"),
        "A rejected cooked definition must veto automatic character pruning");
    Require(registry.contains("if(metadataReady)for(auto& asset:assets)")
        && registry.find("for(const auto& owner:PS::MountedModRegistryOwners::Snapshot())")<registry.find("if(metadataReady)for(auto& asset:assets)"),
        "Conventional discovery must remain available without Asset Registry metadata");
    Require(registry.contains("CastField<FStrProperty>") && registry.contains("ReflectedContract::Fits(property,")
        && registry.contains("property->GetElementSize()!=sizeof(FString)")
        && registry.contains("PS::RegistryEnrollment::Reject(\"pak:\"+path,error.what())"),
        "Cooked strings and rejected declarations require guarded reflection and cleanup veto");
    Require(registrar.contains("if(lane.Name==\"Journals\"||lane.Name==\"Lore\")continue;")
        && registrar.contains("required native registry binding is unavailable:")
        && registrar.contains("unresolved or wrong-class "),
        "Journal ownership must remain in its loader; failed persistent native lanes must veto cleanup");
}
