#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path) {
    std::ifstream file(path);if(!file)throw std::runtime_error("Summon attribution source unavailable");
    return {std::istreambuf_iterator<char>(file),{}};
}
int main(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("Expected resolver, quest credit, native credit, NPC loader sources");
    const auto resolver=Read(argv[1]),quest=Read(argv[2]),native=Read(argv[3]),loader=Read(argv[4]);
    const std::string nativePath=argv[3];
    const auto notifications=Read((nativePath.substr(0,nativePath.find_last_of("/\\")+1)+"ConfirmedKillNotifications.h").c_str());
    const auto require=[](bool value){if(!value)throw std::runtime_error("RuneSchema summon attribution integration regression");};
    // Generic native enrollment, including subclasses; no named addon is required.
    require(resolver.find("/Game/Mods/")==std::string::npos);
    require(resolver.find("AllegianceCertified")==std::string::npos);
    require(resolver.find("/Script/Dominion.FollowerComponent")!=std::string::npos);
    require(resolver.find("GetComponentsByClass(type)")!=std::string::npos);
    require(resolver.find("component->GetOuterPrivate()!=caster")!=std::string::npos);
    require(resolver.find("matches==1 && CertifiedAllegiance")!=std::string::npos);
    require(resolver.find("OwnsGameplay(PS::Network::Detect(player).Mode)")!=std::string::npos);
    require(resolver.find("Authority(attacker)")!=std::string::npos);
    require(resolver.find("Authority(controller)")!=std::string::npos);
    require(resolver.find("IsConfused")!=std::string::npos && resolver.find("ShouldFollowConfuser")!=std::string::npos);
    require(quest.find("Summons::CreditedPlayer")!=std::string::npos);
    require(quest.find("Summons::DeadVictim")!=std::string::npos);
    // Reject non-AI destructibles before reading their nonexistent AI field.
    const auto aiOnly=quest.find("!Summons::DeadVictim(victim,player))return {};");
    const auto aiComponent=quest.find("ActorHelper::GetObjectRef(victim,TEXT(\"AiDamageComponent\"))");
    require(aiOnly!=std::string::npos && aiComponent!=std::string::npos && aiOnly<aiComponent);
    require(resolver.find("!victim->IsA(ai)")!=std::string::npos);
    require(native.find("ProgressComponent:OnAIKilled")!=std::string::npos);
    require(native.find("source->GetOuterPrivate()!=controller")!=std::string::npos);
    require(native.find("GetObjectRef(controller,TEXT(\"ProgressComponent\"))!=source")!=std::string::npos);
    require(native.find("PreventOriginalFunctionCall")!=std::string::npos);
    require(native.find("frame.Original")!=std::string::npos);
    require(native.find("m_dispatches.Cancel")!=std::string::npos);
    require(native.find("frame.Reserved && !dispatch.OriginalFunctionCallPrevented()")!=std::string::npos);
    require(native.find("NotifyConfirmedKill(player,victim)")!=std::string::npos);
    require(native.find("m_frames.erase(m_frames.begin()+index-1)")<native.find("NotifyConfirmedKill(player,victim)"));
    require(notifications.find("bRuneSchemaConfirmedKillListener")!=std::string::npos);
    require(notifications.find("RuneSchema_OnConfirmedKill")!=std::string::npos);
    require(notifications.find("PlayerController(player)")!=std::string::npos);
    require(notifications.find("component->GetOuterPrivate()!=player")!=std::string::npos);
    require(notifications.find("PS::WeakObjectHandle")!=std::string::npos);
    require(notifications.find("HasAnyFunctionFlags(FUNC_Net)")!=std::string::npos);
    require(notifications.find("count!=2")!=std::string::npos);
    require(notifications.find("listeners.size()>=64")!=std::string::npos);
    require(notifications.find("OnAIKilled")==std::string::npos);
    // The native callback is adapted in place. Never synthesize or replay rewards.
    require(native.find("ProcessEvent(")==std::string::npos);
    require(native.find("FunctionCall ")==std::string::npos);
    require(loader.find("m_summonNativeCredit->Before(dispatch")!=std::string::npos);
    require(loader.find("m_summonNativeCredit->After(dispatch")!=std::string::npos);
    require(loader.find("UnregisterCallback(m_summonCreditCallbackId)")!=std::string::npos);
}
