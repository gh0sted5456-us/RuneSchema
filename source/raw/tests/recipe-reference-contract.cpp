#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Recipe loader source is required");
    std::ifstream file(argv[1],std::ios::binary);
    if(!file)throw std::runtime_error("Recipe loader source is unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    require(source.find("static RC::StringType RecipeIdentity")!=std::string::npos,
        "Recipe runtime maps are not namespaced by ModID");
    require(source.find("if(!match && !qualified)")!=std::string::npos,
        "Unique global authored recipe fallback is missing");
    require(source.find("if(match)return nullptr;")!=std::string::npos,
        "Ambiguous recipe names are not rejected");
    require(source.find("wide.starts_with(TEXT(\"/\"))")!=std::string::npos,
        "Cooked RecipeData path resolution is missing");
    require(source.find("static std::string RecipePersistenceId")!=std::string::npos
        && source.find("DialogueSave::PersistenceIdForSeed(\"Recipe/\"")!=std::string::npos,
        "Authored recipes do not receive a deterministic canonical persistence identity");
    require(source.find("OwnedContent::") == std::string::npos,
        "Recipe persistence still depends on an ownership manifest");
    require(source.find("recipeDiagnostics.largeRecipeWorkloadWarning")!=std::string::npos
        && source.find("recipeDiagnostics.largeRecipeWarningThreshold")!=std::string::npos
        && source.find("m_recipeDefs.size() > static_cast<std::size_t>(warningThreshold)")!=std::string::npos
        && source.find("Loading will continue normally without limits")!=std::string::npos,
        "Large recipe workloads are not warned without gatekeeping");
    const auto storeBegin=source.find("DragonWildsRecipeModLoader::PrepareStoreForPlayer");
    const auto storeEnd=source.find("void DragonWildsRecipeModLoader::OnLoad",storeBegin);
    require(storeBegin!=std::string::npos && storeEnd!=std::string::npos,
        "Vendor recipe availability implementation is missing");
    const auto store=source.substr(storeBegin,storeEnd-storeBegin);
    require(store.find("registry->Recipes.contains(")==std::string::npos,
        "Transient vendor recipes still depend on the persistence registry snapshot");
    require(store.find("RC::to_string(RC::StringType(*id))!=expectedIdentity")!=std::string::npos,
        "Transient vendor recipes are not checked against their owned deterministic identity");
    require(store.find("for(const auto& [vendorKey,_]:m_vendorRecipeOwners)")!=std::string::npos
        && store.find("helper.Remove(&vendorRecipe)")!=std::string::npos,
        "Previously opened vendor recipes can bleed into another store");
    const auto vendorBegin=source.find("DragonWildsRecipeModLoader::EnsureVendorRecipe");
    require(vendorBegin!=std::string::npos && vendorBegin<storeBegin,
        "Vendor recipe creation implementation is missing");
    const auto vendor=source.substr(vendorBegin,storeBegin-vendorBegin);
    require(vendor.find("ApplyUnlocksToAllProgressComponents()") == std::string::npos
        && vendor.find("m_unlock.insert(key)") == std::string::npos,
        "Opening a vendor must not replay all recipe unlocks for every player");
    require(source.find("items.Num()>32768")==std::string::npos
        &&source.find("Loaded ItemData roster exceeds recipe routing limit")==std::string::npos,
        "Legitimate large ItemData catalogues are rejected before recipe routing");
    require(source.find("if(m_itemRoutes.empty())")==std::string::npos
        &&source.find("m_itemRoutes.Ensure(")!=std::string::npos
        &&source.find("m_itemRoutes.Reset();")!=std::string::npos,
        "Empty/failed ItemData routes may rescan per offer or never refresh on a new batch");
    require(vendor.find("RefreshItemRoutes();")==std::string::npos,
        "Full-path vendor offers are gated by a global ItemData scan");
    require(source.find("PS::RecipeItemRouting::Route(")!=std::string::npos
        &&source.find("PersistenceID no longer matches its live asset")!=std::string::npos,
        "Recipe routing bypasses shared lookup behavior or live identity validation");
    require(source.find("m_referenceBatch->Remember(recipe->GetPathName()")!=std::string::npos
        &&source.find("m_referenceBatch->Resolve(def.Key,load,valid")!=std::string::npos
        &&source.find("RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization")!=std::string::npos,
        "Cooked reference preparation lacks its scoped live-object snapshot or ready/type validation");
    require(source.find("~BatchScope(){Active=Before;}")!=std::string::npos
        &&source.find("[PERF][RECIPES][REFERENCES]")!=std::string::npos,
        "Recipe preparation snapshot can leak across batches or lacks measured load counters");
}
