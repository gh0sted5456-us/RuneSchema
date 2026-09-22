#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Quest ownership source unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Gameplay context, NPC loader, presentation and quest action sources required");
    const auto context = Read(argv[1]);
    const auto loader = Read(argv[2]);
    const auto presentation = Read(argv[3]);
    const auto actions = Read(argv[4]);
    const auto require = [](bool value) {
        if (!value) throw std::runtime_error("Quest gameplay-controller ownership regression");
    };
    require(context.find("/Script/Dominion.DominionPlayerController") != std::string::npos);
    require(context.find("/Script/Dominion.DominionPlayerCharacter") != std::string::npos);
    require(context.find("GetObjectRef(controller, TEXT(\"Pawn\"))") != std::string::npos);
    require(context.find("GetObjectRef(pawn, TEXT(\"Controller\")) != controller") != std::string::npos);
    require(context.find("component->GetOuterPrivate() == controller") != std::string::npos);
    require(context.find("IsGameplayQuestController(controller, static_cast<AActor*>(object))") != std::string::npos);
    require(loader.find("if(!IsGameplayQuestController(owner))return;") != std::string::npos);
    require(loader.find("if(!IsGameplayQuestController(controller))return;") != std::string::npos);
    require(loader.find("PumpQuestRefreshes();") != std::string::npos);
    const auto damageStart=loader.find("void DragonWildsNpcLoader::OnQuestDamageCredit");
    const auto damageEnd=loader.find("void DragonWildsNpcLoader::QueueQuestRefresh",damageStart);
    require(damageStart!=std::string::npos && damageEnd!=std::string::npos);
    const auto damage=loader.substr(damageStart,damageEnd-damageStart);
    require(damage.find("QueueQuestRefresh(controller,key)")!=std::string::npos);
    // Multiplayer proved the native whole-registry owning-client refresh. Do
    // not regress to synthesizing the version-sensitive per-objective struct.
    require(loader.find("native.NotifyRecovery()") != std::string::npos);
    require(loader.find("native.NotifyCounterChanged") == std::string::npos);
    require(loader.find("QueueQuestLocationRefresh(owner);") != std::string::npos);
    require(loader.find("PumpQuestLocationRefreshes(deltaSeconds);") != std::string::npos);
    require(loader.find("m_pendingQuestLocationRefreshes.push_back({PS::WeakObject(controller),0,60})") != std::string::npos);
    require(loader.find("if(entry->Elapsed<0.5)") != std::string::npos);
    require(loader.find("if(m_questLocationChecks>=300)return;") != std::string::npos);
    const auto acceptance=actions.find("return current() && native.IsInitialized() && state()==\"Given\";");
    require(acceptance!=std::string::npos);
    require(actions.find("QueueQuestRefresh(controller,quest.Key);",acceptance)==std::string::npos);
    require(actions.find("action.QuestAction==\"Abandon\"")!=std::string::npos);
    require(actions.find("native.CancelForRecovery()")!=std::string::npos);
    require(presentation.find("if(!IsGameplayQuestController(controller)") != std::string::npos);
}
