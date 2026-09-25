#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    if(argc!=3)throw std::runtime_error("registrar and provenance sources required");
    const auto registrar=Read(argv[1]),provenance=Read(argv[2]);
    const auto need=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    need(provenance.find("OwnedContent::Merge")!=provenance.npos,"registered clones are not persisted to ownership ledger");
    need(registrar.find("OwnedContent::CompareSnapshot(path)")!=registrar.npos,"previous/current ownership snapshot is not compared");
    need(registrar.find("OwnedContent::CommitSnapshot(path)")!=registrar.npos,"successful current ownership snapshot is not committed");
    need(registrar.find("accumulating")!=registrar.npos,"ownership state is not documented as a bounded snapshot");
    need(registrar.find("PrepareRetiredContent();")<registrar.find("InstallHooks();"),"tombstones are not prepared before save hooks");
    need(registrar.find("CleanRetiredCharacterSaves(retired)")!=registrar.npos,"owned content is not cleaned before character deserialization");
    need(registrar.find("runeschema-before-clean")!=registrar.npos,"pre-load save cleanup has no original backup");
    need(registrar.find("PlanOwned")!=registrar.npos,"pre-load cleanup is not constrained by the ownership ledger");
    need(registrar.find("RegisterNativePreHook")<registrar.find("RegisterNativePostHook"),"registry restore must precede verified post-load scrub");
    need(registrar.find("GetNumItemsByData")!=registrar.npos && registrar.find("RemoveItemByData")!=registrar.npos,"native inventory scrub missing");
    need(registrar.find("verification.Result<int32>() != 0")!=registrar.npos,"item removal is not read-back verified");
    need(registrar.find("OWNED-ONLY")!=registrar.npos,"owned-only operator tag missing");
    need(registrar.find("RecipesUnlocked")!=registrar.npos && registrar.find("FScriptSetHelper")!=registrar.npos,"retired recipe unlock cleanup is missing");
    need(registrar.find("IsActiveDeclarationPath")!=registrar.npos,"active cooked declarations are not admitted to native registration");
    const auto compare=registrar.find("OwnedContent::CompareSnapshot(path)");
    const auto pending=registrar.find("m_pendingProviderSnapshot = path",compare);
    const auto providerCommit=registrar.find("OwnedContent::CommitSnapshot(m_pendingProviderSnapshot)",pending);
    need(compare< pending && pending<providerCommit,
        "Game Pass ledger is committed before provider-backed cleanup verification");
    need(registrar.find("[SAVE-CLEANER][PROVIDER][VERIFIED]")!=registrar.npos,
        "verified Game Pass provider cleanup has no explicit completion state");
    need(registrar.find("m_pendingProviderUnsupportedKinds")!=registrar.npos,
        "unsupported Game Pass save categories do not fail closed");
    need(registrar.find("[SAVE-CLEANER][PROVIDER][VERIFIED-PARTIAL]")!=registrar.npos,
        "supported Game Pass identities are not cleaned when another save category remains pending");
    const auto partialWarning=registrar.find("[SAVE-CLEANER][PROVIDER][PARTIAL]");
    const auto supportedCleanup=registrar.find("const bool hasItems",partialWarning);
    need(partialWarning<supportedCleanup,
        "unsupported Game Pass categories still block supported item/recipe cleanup");
}
