#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    std::ostringstream text;text<<file.rdbuf();return text.str();
}

int main(int argc,char** argv) {
    assert(argc==4);
    const auto provenance=Read(argv[1]);
    const auto loader=Read(argv[2]);
    const auto recipes=Read(argv[3]);
    assert(provenance.find("FWeakObjectPtr")==std::string::npos);
    assert(provenance.find("PS::WeakObjectHandle handle")!=std::string::npos);
    assert(provenance.find("entry.handle.Get()!=object")!=std::string::npos);
    assert(provenance.find("created.handle.Assign(object)")!=std::string::npos);
    assert(provenance.find("FUObjectArray::IndexToObject")!=std::string::npos);
    assert(provenance.find("slot->GetUObject()!=object")!=std::string::npos);
    // Ownership is established at construction, then updated with the actual
    // registration result. A failed clone must not acquire successful provenance.
    assert(loader.find("PS::AssetProvenance::Record(created,pendingAsset.Properties")!=std::string::npos);
    assert(loader.find("RC::to_string(pendingAsset.ModName),true,false,0,pendingAsset.Properties)")!=std::string::npos);
    assert(loader.find("if (isClone) PS::AssetProvenance::Record(object,it->Properties")!=std::string::npos);
    assert(loader.find("RC::to_string(it->ModName),createdNow,registered,result.ErrorCount,it->Properties)")!=std::string::npos);
    assert(recipes.find("soft->WeakPtr.Get()") == std::string::npos);
    assert(recipes.find("soft->WeakPtr = PS::WeakObject(recipe)") == std::string::npos);
    assert(recipes.find("soft->ObjectID = recipeID") != std::string::npos);
    assert(loader.find("$DominionSpheres") != std::string::npos);
    assert(loader.find("/Script/Dominion.DominionShape_Sphere") != std::string::npos);
    assert(loader.find("candidate->GetOuterPrivate() != current") != std::string::npos);
    assert(loader.find("DominionShape_Sphere Radius did not match after update") != std::string::npos);
    assert(loader.find("[SERVER][ASSET-CLONING]") != std::string::npos);
    assert(loader.find("if (!PS::Storefront::IsDedicatedServer())") != std::string::npos);
}
