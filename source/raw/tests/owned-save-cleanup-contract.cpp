#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    if(argc!=5)throw std::runtime_error("registrar, provenance, quest service, and main loader sources required");
    const auto registrar=Read(argv[1]),provenance=Read(argv[2]),quests=Read(argv[3]),mainLoader=Read(argv[4]);
    const auto need=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    need(registrar.find("OwnedContent::CompareSnapshot")==registrar.npos
        && registrar.find("OwnedContent::CommitSnapshot")==registrar.npos,
        "automatic pruning still depends on an ownership ledger");
    need(mainLoader.find("OwnedContent::BeginSnapshot")==mainLoader.npos,
        "startup still creates an ownership ledger");
    need(registrar.find("ScrubLocalCharacterFiles")!=registrar.npos
        && registrar.find("ConfigFiles::Write")!=registrar.npos,
        "pre-selection Steam pruning is missing");
    need(registrar.find("SaveCleanup::Plan(")!=registrar.npos
        && registrar.find("registry.get(), false, true")!=registrar.npos,
        "pruning is not driven by the completed native registry");
    need(registrar.find("snapshot.Items")!=registrar.npos
        && registrar.find("snapshot.Recipes")!=registrar.npos
        && registrar.find("snapshot.Quests")!=registrar.npos
        && registrar.find("snapshot.Journals")!=registrar.npos,
        "one or more persistent identity registries are absent");
    need(registrar.find("ScrubCharacterJsonBeforeLoad")!=registrar.npos,
        "provider-backed character JSON preflight is missing");
    const auto preRegistration=registrar.find("RegisterInitGameStatePreCallback");
    const auto registerAll=registrar.find("RegisterAll();",preRegistration);
    const auto scrubFiles=registrar.find("ScrubLocalCharacterFiles();",registerAll);
    need(preRegistration!=registrar.npos && registerAll<scrubFiles,
        "pruning does not follow pre-world native registration");
    need(registrar.find("[SAVE-CLEANER][VERIFIED] Removed")!=registrar.npos,
        "compact verified pruning summary is missing");
}
