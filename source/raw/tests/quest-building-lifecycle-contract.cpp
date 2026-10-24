#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("Expected spawn loader and schema sources");
    const auto spawn=Read(argv[1]),schema=Read(argv[2]);
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(std::string("Quest building lifecycle regression: ")+message);
    };
    require(spawn.find("if(spawn.bBuildingProp)RememberBuildingOwnership()")!=std::string::npos,
        "managed building ownership is not committed before creation");
    require(spawn.find("spawn.Time!=TimeOfDay::Requirement::Any || spawn.bBuildingProp || spawn.bTemporaryResource")!=std::string::npos
        && spawn.find("spawned->SetFlags(RF_Transient)")!=std::string::npos
        && spawn.find("TEXT(\"bSkipSpudStore\")")!=std::string::npos,
        "quest-owned buildings can still enter native persistence");
    require(spawn.find("!existing->HasAnyFlags(RF_Transient)")!=std::string::npos
        && spawn.find("if (spawn.bBuildingProp){RetireTimedActor(existing)")!=std::string::npos,
        "legacy persistent quest buildings are not safely retired");
    require(schema.find("Quest-linked BuildingProp actors are transient")!=std::string::npos,
        "author-facing lifecycle behavior is undocumented in the generated schema");
    require(spawn.find("type == \"BuildingProp\" || type == \"BuildingPiece\"")!=std::string::npos
        &&spawn.find("spawn.bBuildingProp=!spawn.bNativeBuilding")!=std::string::npos,
        "native BuildingPiece must remain separate from transient BuildingProp");
    require(spawn.find("spawn.bNativeBuilding&&HasEquivalentActorNear(world,spawn)")<spawn.find("ResolveGroundedLocation(world, spawn);",spawn.find("void DragonWildsSpawnLoader::ProcessSpawns")),
        "saved building occupancy must be checked before tracing its roof");
    const auto native=spawn.find("// Never rewrite or demolish an existing native/player-owned building.");
    require(native!=std::string::npos&&spawn.find("CreateActor(world,spawn);return;",native)<spawn.find("bool needsRecreate",native),
        "native placements must not enter managed building replacement logic");
    require(spawn.find("spawn.bNativeBuilding || spawn.Type == ESpawnEntryType::RemoveActor")!=std::string::npos
        &&spawn.find("if(spawn.bNativeBuilding||spawn.Type==ESpawnEntryType::RemoveActor")!=std::string::npos,
        "native buildings must not enter timed cleanup or recurring scale changes");
}
