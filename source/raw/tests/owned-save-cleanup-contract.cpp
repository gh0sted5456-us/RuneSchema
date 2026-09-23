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
    need(registrar.find("OwnedContent::Absent(records, active)")!=registrar.npos,"disabled folders are not treated as absent owners");
    need(registrar.find("DiscoverDisabledDefinitions")!=registrar.npos,"disabled item definitions are not migrated without activation");
    need(registrar.find("ReadCloneManifest")!=registrar.npos,"prior clone manifest migration is missing");
    need(registrar.find("PrepareRetiredContent();")<registrar.find("InstallHooks();"),"tombstones are not prepared before save hooks");
    need(registrar.find("RegisterNativePreHook")<registrar.find("RegisterNativePostHook"),"registry restore must precede verified post-load scrub");
    need(registrar.find("GetNumItemsByData")!=registrar.npos && registrar.find("RemoveItemByData")!=registrar.npos,"native inventory scrub missing");
    need(registrar.find("verification.Result<int32>() != 0")!=registrar.npos,"item removal is not read-back verified");
    need(registrar.find("OWNED-ONLY")!=registrar.npos,"owned-only operator tag missing");
    need(registrar.find("RecipesUnlocked")!=registrar.npos && registrar.find("FScriptSetHelper")!=registrar.npos,"retired recipe unlock cleanup is missing");
    need(registrar.find("IsActiveDeclarationPath")!=registrar.npos,"active cooked declarations are not admitted to native registration");
}
