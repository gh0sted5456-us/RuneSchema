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
    Require(argc==7,"Expected main, recipe, journal, registry loader, registrar and building loader");
    const auto main=Read(argv[1]),recipe=Read(argv[2]),journal=Read(argv[3]),registry=Read(argv[4]),registrar=Read(argv[5]);
    const auto building=Read(argv[6]);
    Require(main.contains("static_cast<DragonWildsBuildingModLoader*>(loader.get())->ImportCookedDefinitions()")
        &&main.contains("static_cast<DragonWildsBuildingModLoader*>(loader.get())->RejectCleanup"),
        "Cooked buildings must import before application and preserve disabled-loader cleanup guards");
    Require(building.contains("CookedPakRegistryManifest::Definitions(\"Buildings\")")
        &&building.contains("definition.DeclaredPersistenceID=id")&&building.contains("definition.DeclaredInternalName=name")
        &&building.contains("if(!definition.UnlockAuthored)")&&building.contains("definition.InheritSourcePlacement&&!targets.empty()")
        &&building.contains("if(phase==EEngineLifecyclePhase::GameInstanceInit)ApplyDefinitions()")
        &&building.contains("EnsureStabilityProfile(building,!definition.Declared)")
        &&registrar.contains("if(lane.Name==\"Buildings\")continue;"),
        "Cooked buildings must retain identity, overlay only authored loose settings and use the world registrar");
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
    Require(registrar.contains("if(lane.Name==\"Journals\"||lane.Name==\"Lore\"){")
        && registrar.contains("QuestRegistry::NativeRegistry::ResolvesPersistenceIdentity(subsystem,entry,true)")
        && registrar.contains("schema journal/lore identity is not registered:")
        && registrar.contains("required native registry binding is unavailable:")
        && registrar.contains("unresolved or wrong-class "),
        "Journal ownership must remain in its loader; failed persistent native lanes must veto cleanup");
    Require(!registry.contains("accepted only from a cooked RSREG_")
        && registry.contains("PS::RegistryEnrollment::Reject(owner+\":\"+source,error.what())"),
        "Loose native declarations must use the same parser and fail-closed cleanup enrollment");
    Require(registrar.contains("if(lane.Name==\"Skills\"||lane.Name==\"SkillPerks\"){")
        &&registrar.contains("no skill/perk registry or progress was modified")
        &&registrar.contains("/Script/Engine.UserDefinedEnum")&&registrar.contains("CookedEnumContract::Fingerprint")
        &&registrar.contains("NetworkIdentityState::PublishOwner(key,manifest.Owner,fingerprint)"),
        "Unverified skills/perks must stay blocked; cooked enum verification must preserve owned client/server symbols");
}
