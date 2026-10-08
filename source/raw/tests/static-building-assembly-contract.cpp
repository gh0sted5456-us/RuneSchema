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
    if(argc!=3) throw std::runtime_error("Expected building and spawn loader sources");
    const auto buildings=Read(argv[1]),spawns=Read(argv[2]);
    const auto require=[](bool value,const char* what) {
        if(!value) throw std::runtime_error(std::string("Static assembly contract regression: ")+what);
    };
    require(buildings.find("NativeBuildingPieces")!=std::string::npos,
        "backward-compatible native import mode missing");
    require(buildings.find("StaticAssembly")!=std::string::npos,
        "static import mode missing");
    require(buildings.find("NativePieceIds")!=std::string::npos,
        "hybrid native-piece allowlist missing");
    require(buildings.find("{\"Location\",{{\"X\",destinationX}")!=std::string::npos,
        "assembly parent is not anchored at the requested center");
    require(spawns.find("HierarchicalInstancedStaticMeshComponent")!=std::string::npos,
        "HISM batching missing");
    require(spawns.find("ComposeAssemblyTransforms(")!=std::string::npos,
        "source component transforms are not preserved");
    require(spawns.find("BodySetup")!=std::string::npos,
        "cooked collision validation missing");
    require(spawns.find("ProcessClientStaticAssemblies")!=std::string::npos,
        "deterministic client reconstruction missing");
    require(spawns.find("SetIsReplicated")!=std::string::npos,
        "local assembly replication policy missing");
    require(spawns.find("[DEGRADED][BUILDING-ASSEMBLY]")!=std::string::npos,
        "per-piece failure isolation tag missing");
}
