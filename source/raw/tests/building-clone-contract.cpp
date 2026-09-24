#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if(!file) throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=3) throw std::runtime_error("Expected building loader source and loader schema");
    const auto loader=Read(argv[1]),schema=Read(argv[2]);
    const auto require=[](bool value,const char* what) {
        if(!value) throw std::runtime_error(std::string("Building clone contract regression: ")+what);
    };
    require(loader.find("FindSourcePlacements(source)")!=std::string::npos,
        "omitted AddTo no longer inherits source catalogue membership");
    require(loader.find("SameSoftObject(*soft, source)")!=std::string::npos,
        "source catalogue matching is not object-path based");
    require(loader.find("must resolve to a concrete BP_BaseBuilding_BaseActor child")!=std::string::npos,
        "cooked actor ancestry guard missing");
    require(loader.find("custom cooked actor could not select ManagedActor representation")!=std::string::npos,
        "lightweight derived-mesh safety missing");
    require(loader.find("BuildableActor replacement requires '$Clone'")!=std::string::npos,
        "native source mutation is not rejected");
    require(loader.find("PersistenceID\", \"InternalName\", \"BuildingPieceDataIndex\"")!=std::string::npos,
        "managed identity fields are not protected");
    require(loader.find("requirement.at(\"Amount\").get<int64_t>() <= 0")!=std::string::npos,
        "non-positive costs are not rejected");
    require(loader.find("RefreshBuildingReferencesForWorld()")!=std::string::npos,
        "direct assets are not refreshed before world registration");
    require(loader.find("GetValidBuilding(identity)")!=std::string::npos,
        "registry protection can bypass serial-validated building handles");
    require(loader.find("object=LoadObject(definition.AssetPath)")!=std::string::npos,
        "direct assets are not re-resolved from their stable configured path");
    require(loader.find("definition.Clone?TEXT(\"clone\"):TEXT(\"direct\")")!=std::string::npos,
        "direct and clone lifetime diagnostics are no longer distinct");
    require(loader.find("cooked identity changed")!=std::string::npos,
        "world-boundary baked identity verification is missing");
    require(loader.find("RefreshBuildingCatalogueForWorld()")!=std::string::npos,
        "fresh world catalogue does not replay custom menu placements");
    require(loader.find("[BUILDING-CATALOGUE][VERIFIED]")!=std::string::npos,
        "world catalogue replay has no acceptance diagnostic");
    require(schema.find("appends the clone to every page/collection containing its $Clone source")!=std::string::npos,
        "inherited menu behavior is undocumented in schema");
    require(schema.find("Complete replacement build cost")!=std::string::npos,
        "replacement cost behavior is undocumented in schema");
}
