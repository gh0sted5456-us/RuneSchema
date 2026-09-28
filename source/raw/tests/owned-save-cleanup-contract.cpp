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
    need(registrar.find("ScrubLocalCharacterFiles")==registrar.npos
        && registrar.find("ConfigFiles::Write")==registrar.npos,
        "automatic cleanup still rewrites stored Steam character files");
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
    need(preRegistration!=registrar.npos && registerAll!=registrar.npos,
        "pre-world native registration is missing");
    need(registrar.find("fingerprint != m_registryCandidateFingerprint")!=registrar.npos
        && registrar.find("PublishRegistry({});\n                return;")!=registrar.npos,
        "character cleanup can consume an unsettled registry snapshot");
    need(registrar.find("if (cleaned.Removed.empty()) {")!=registrar.npos,
        "an unchanged character is not a strict no-op");
}
