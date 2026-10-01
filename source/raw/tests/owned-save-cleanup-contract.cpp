#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    if(argc!=6)throw std::runtime_error("registrar, provenance, quest service, main loader, and pruner sources required");
    const auto registrar=Read(argv[1]),provenance=Read(argv[2]),quests=Read(argv[3]),mainLoader=Read(argv[4]),pruner=Read(argv[5]);
    const auto need=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    need(registrar.find("OwnedContent::CompareSnapshot")==registrar.npos
        && registrar.find("OwnedContent::CommitSnapshot")==registrar.npos,
        "automatic pruning still depends on an ownership ledger");
    need(mainLoader.find("OwnedContent::BeginSnapshot")==mainLoader.npos,
        "startup still creates an ownership ledger");
    need(registrar.find("ScrubLocalCharacterFiles")==registrar.npos
        && registrar.find("ConfigFiles::Write")==registrar.npos,
        "automatic cleanup still rewrites stored Steam character files");
    need(pruner.find("SaveCleanup::Plan(")!=pruner.npos
        && pruner.find("registry.get(), false, true, true")!=pruner.npos,
        "pruning is not driven by the completed native registry");
    need(registrar.find("snapshot.Items")!=registrar.npos
        && registrar.find("snapshot.Recipes")!=registrar.npos
        && registrar.find("snapshot.Quests")!=registrar.npos
        && registrar.find("snapshot.Journals")!=registrar.npos,
        "one or more persistent identity registries are absent");
    need(registrar.find("ScrubCharacterJsonBeforeLoad")!=registrar.npos,
        "provider-backed character JSON preflight is missing");
    need(registrar.find(
            "/Script/Dominion.PersistenceSubsystem:OnPersistentStoreLoadPlayerResult")
            !=registrar.npos
        && registrar.find("OnPersistentStoreLoadPlayerResult")!=registrar.npos,
        "character preflight does not bind the native persistent-store player JSON result");
    need(pruner.find("s_cleanupConsumedForProcess")!=pruner.npos
        && registrar.find("EnsureCharacterJsonPreflightHook")!=registrar.npos
        && registrar.find("[PERSISTENCE-PRUNER][BOUNDARY-READY]")!=registrar.npos,
        "automatic cleanup cannot late-bind the first eligible character load");
    const auto registerAllDefinition=registrar.find(
        "void DragonWildsDataRegistrar::RegisterAll()");
    need(registerAllDefinition!=registrar.npos
        && registrar.find("EnsureCharacterJsonPreflightHook();",
            registerAllDefinition)!=registrar.npos,
        "native character preflight is not retried after Dominion loads");
    const auto readyGate=pruner.find("if (!registry || !registry->Ready())");
    const auto consume=pruner.find("s_cleanupConsumedForProcess.exchange(",readyGate);
    const auto plan=pruner.find("SaveCleanup::Plan(",consume);
    need(readyGate!=pruner.npos && consume!=pruner.npos
        && plan!=pruner.npos && readyGate<consume && consume<plan,
        "startup cleanup is not globally consumed after registry readiness and before mutation");
    need(pruner.find("m_checkedCharacters")==pruner.npos
        && pruner.find("[PERSISTENCE-PRUNER][ORPHANS-REMOVED]")!=pruner.npos
        && pruner.find("[PERSISTENCE-PRUNER][ORPHAN-REMOVED]")!=pruner.npos,
        "cleanup is still per-character or no longer warns when orphaned IDs are removed");
    need(pruner.find("[PERSISTENCE-PRUNER][RESOLVED]")!=pruner.npos
        && pruner.find("Origin is irrelevant")!=pruner.npos,
        "loaded-pak item IDs are not visibly retained from the live registry");
    need(pruner.find("PersistenceDiagnosticLedger")==pruner.npos,
        "cleanup depends on the optional diagnostic ledger");
    const auto fallback=registrar.find(
        "m_characterJsonHook = Hook::RegisterProcessEventPreCallback");
    const auto fallbackGuard=registrar.find("if (!parameters",fallback);
    const auto preGuardDiscovery=registrar.find(
        "EnsureCharacterJsonPreflightHook();",fallback);
    need(fallback!=registrar.npos && fallbackGuard!=registrar.npos
        && (preGuardDiscovery==registrar.npos || preGuardDiscovery>fallbackGuard),
        "global ProcessEvent fallback performs native-hook discovery before filtering the event");
    const auto preRegistration=registrar.find("RegisterInitGameStatePreCallback");
    const auto registerAll=registrar.find("RegisterAll();",preRegistration);
    need(preRegistration!=registrar.npos && registerAll!=registrar.npos,
        "pre-world native registration is missing");
    need(registrar.find("fingerprint != m_registryCandidateFingerprint")!=registrar.npos
        && registrar.find("PublishRegistry({});\n                return;")!=registrar.npos,
        "character cleanup can consume an unsettled registry snapshot");
    need(registrar.find("registrationsComplete = RegisterMissing")!=registrar.npos
        && registrar.find("itemsReady && recipesReady && registrationsComplete")!=registrar.npos
        && registrar.find("primary persistence registry rejected the asset")!=registrar.npos
        && registrar.find("network registry rejected the asset")!=registrar.npos
        && registrar.find("does not round-trip to one live data asset")!=registrar.npos
        && registrar.find("duplicate PersistenceID resolves to multiple live assets")!=registrar.npos,
        "cleanup readiness ignores a loaded asset that failed identity round-trip, uniqueness, primary, or network registration");
    need(pruner.find("if (cleaned.Removed.empty()) {")!=pruner.npos,
        "an unchanged character is not a strict no-op");
    need(pruner.find("SaveSnapshotRestore")==pruner.npos
        && pruner.find("Default.json")==pruner.npos,
        "automatic pruning still contains snapshot restoration machinery");
    need(registrar.find("m_pruner.PruneBeforeCharacterLoad")!=registrar.npos
        && registrar.find("SaveCleanup::Plan(")==registrar.npos,
        "persistence pruning is not isolated from live-registry registration");
}
