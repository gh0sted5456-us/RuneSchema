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
}
