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
    assert(provenance.find("PS::WeakObject")==std::string::npos);
    assert(provenance.find("FUObjectArray::IndexToObject")!=std::string::npos);
    assert(provenance.find("slot->GetUObject()==object")!=std::string::npos);
    const auto call="PS::AssetProvenance::Record(";
    const auto first=loader.find(call);
    assert(first!=std::string::npos && loader.find(call,first+1)==std::string::npos);
    assert(recipes.find("soft->WeakPtr.Get()") == std::string::npos);
    assert(recipes.find("soft->WeakPtr = PS::WeakObject(recipe)") == std::string::npos);
    assert(recipes.find("soft->ObjectID = recipeID") != std::string::npos);
    assert(loader.find("$DominionSpheres") != std::string::npos);
    assert(loader.find("/Script/Dominion.DominionShape_Sphere") != std::string::npos);
    assert(loader.find("candidate->GetOuterPrivate() != current") != std::string::npos);
    assert(loader.find("DominionShape_Sphere Radius did not match after update") != std::string::npos);
}
