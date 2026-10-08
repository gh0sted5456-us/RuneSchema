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
    require(spawn.find("const bool questOwnedBuilding = spawn.bBuildingProp && !spawn.QuestCompleted.empty()")!=std::string::npos,
        "quest-owned building classification is missing");
    require(spawn.find("spawn.Time!=TimeOfDay::Requirement::Any || questOwnedBuilding")!=std::string::npos
        && spawn.find("spawned->SetFlags(RF_Transient)")!=std::string::npos
        && spawn.find("TEXT(\"bSkipSpudStore\")")!=std::string::npos,
        "quest-owned buildings can still enter native persistence");
    require(spawn.find("!existing->HasAnyFlags(RF_Transient)")!=std::string::npos
        && spawn.find("if (spawn.bBuildingProp) RetireTimedActor(existing)")!=std::string::npos,
        "legacy persistent quest buildings are not safely retired");
    require(schema.find("Quest-linked BuildingProp actors are transient")!=std::string::npos,
        "author-facing lifecycle behavior is undocumented in the generated schema");
}
