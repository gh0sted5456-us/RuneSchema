#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Time-of-day contract source unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

int main(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("Runtime, NPC, spawn and schema sources required");
    const auto runtime=Read(argv[1]);
    const auto npc=Read(argv[2]);
    const auto spawn=Read(argv[3]);
    const auto schema=Read(argv[4]);
    const auto require=[](bool value){if(!value)throw std::runtime_error("Time-of-day release contract regression");};

    require(runtime.find("CachedTimeOfDayState")!=std::string::npos);
    require(runtime.find("/Script/Dominion.InGameTimeActor:GetTimeOfDay")!=std::string::npos);
    require(runtime.find("/Script/Dominion.InGameTimeActor:GetTimeOfDawn")!=std::string::npos);
    require(runtime.find("/Script/Dominion.InGameTimeActor:GetTimeOfDusk")!=std::string::npos);
    require(runtime.find("StoredTime")!=std::string::npos);
    require(npc.find("OnChangeTimeOfDayState")!=std::string::npos);
    require(npc.find("OnEnterTimeFrameDynamic_Event")!=std::string::npos);
    require(npc.find("OnExitTimeFrameDynamic_Event")!=std::string::npos);
    require(spawn.find("OnEnterTimeFrameDynamic_Event")!=std::string::npos);
    require(spawn.find("m_buildingTimeElapsed=1.0")!=std::string::npos);

    for(const auto* loader:{"spawns","vendors","quests","events"})
        require(schema.find(std::string("result[\"")+loader+"\"]")!=std::string::npos);
    require(schema.find("WhenTimeOfDay")!=std::string::npos);
    require(schema.find("DespawnTimeOfDay")!=std::string::npos);
    require(schema.find("TimeOfDay")!=std::string::npos);
}
