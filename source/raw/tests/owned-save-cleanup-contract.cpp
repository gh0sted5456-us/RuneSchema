#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    if(argc!=5)throw std::runtime_error("registrar, provenance, quest service, and main loader sources required");
    const auto registrar=Read(argv[1]),provenance=Read(argv[2]),quests=Read(argv[3]),mainLoader=Read(argv[4]);
    const auto need=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    need(provenance.find("OwnedContent::Merge")!=provenance.npos,"registered clones are not added to the ownership vector");
    need(registrar.find("OwnedContent::CompareSnapshot(path)")!=registrar.npos,"previous/current ownership vector is not compared");
    need(registrar.find("OwnedContent::CommitSnapshot(path)")!=registrar.npos,"unchanged ownership vector is not committed");
    need(registrar.find("CharacterSaveDirectory")==registrar.npos && registrar.find("ConfigFiles::Write")==registrar.npos,"registrar must not rewrite character files directly");
    need(registrar.find("ScrubUnknownCharacterSaves")==registrar.npos,"broad registry-unknown cleanup is still active");
    const auto questLoad=quests.find("void Load(const std::string& mod,const Json& data)");
    const auto questPrepare=quests.find("void Prepare(RC::Unreal::UObject* context)");
    const auto questOwnership=quests.find("OwnedContent::Merge",questLoad);
    need(questLoad<questOwnership && questOwnership<questPrepare,"quests are not owned before retired-ID comparison");
    need(registrar.find("retiredQuests.emplace(retired.PersistenceID,retired.Owner)")!=registrar.npos,"quest cleanup is not constrained to exact retired identities");
    need(registrar.find("RegisterNativePreHook")<registrar.find("RegisterNativePostHook"),"registration must precede post-load cleanup");
    need(registrar.find("GetNumItemsByData")!=registrar.npos && registrar.find("RemoveItemByData")!=registrar.npos,"native inventory cleanup missing");
    need(registrar.find("verification.Result<int32>() != 0")!=registrar.npos,"item cleanup is not read-back verified");
    need(registrar.find("RecipesUnlocked")!=registrar.npos && registrar.find("FScriptSetHelper")!=registrar.npos,"recipe cleanup missing");
    need(registrar.find("QuestProgressComponent:OnQuestsUpdated")!=registrar.npos,"quest cleanup has no ready-state retry");
    need(registrar.find("m_questsVerified=result.Ready")!=registrar.npos,"vector can commit before quest state is ready");
    need(registrar.find("ScrubRetiredJournal")!=registrar.npos
        && registrar.find("Client_HandleJournalEntriesLoadedFromPersistence")!=registrar.npos,
        "journal/lore cleanup is not attached to hydrated live state");
    need(registrar.find("m_journalVerified=true")!=registrar.npos
        && registrar.find("!m_journalVerified")!=registrar.npos,
        "ownership vector can commit before journal/lore cleanup verifies");
    const auto compare=registrar.find("OwnedContent::CompareSnapshot(path)");
    const auto pending=registrar.find("m_pendingSnapshot = path",compare);
    const auto commit=registrar.find("OwnedContent::CommitSnapshot(m_pendingSnapshot)",pending);
    need(compare<pending && pending<commit,"ownership vector commits before verified live cleanup");
    const auto initialize=mainLoader.find("m_dataRegistrar.Initialize()");
    const auto prepare=mainLoader.find("m_dataRegistrar.PrepareRetiredContent()",initialize);
    need(initialize<prepare,"retired comparison occurs before native registration");
    const auto partial=registrar.find("[SAVE-CLEANER][PARTIAL]");
    need(partial<registrar.find("const bool hasItems",partial),"unsupported categories block supported cleanup");
}
