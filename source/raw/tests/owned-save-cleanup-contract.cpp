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
    need(registrar.find("CleanLocalCharacterSavesOnce")!=registrar.npos
        && registrar.find("ConfigFiles::Write(path, encoded)")!=registrar.npos
        && registrar.find("BackupCharacterSave(path)")!=registrar.npos
        && registrar.find("failed post-write verification")!=registrar.npos,
        "startup cleanup is missing verified backup-first atomic replacement");
    need(pruner.find("SaveCleanup::Plan(")!=pruner.npos
        && pruner.find("registry.get(), false, true, true")!=pruner.npos,
        "pruning is not driven by the completed native registry");
    need(registrar.find("snapshot.Items")!=registrar.npos
        && registrar.find("snapshot.Recipes")!=registrar.npos
        && registrar.find("snapshot.Quests")!=registrar.npos
        && registrar.find("snapshot.Journals")!=registrar.npos,
        "one or more persistent identity registries are absent");
    const auto firstCapture=registrar.find("RegisterAll();");
    const auto secondCapture=registrar.find("RegisterAll();",firstCapture+1);
    const auto startupCleanup=registrar.find("CleanLocalCharacterSavesOnce();",
        secondCapture);
    need(firstCapture!=registrar.npos && secondCapture!=registrar.npos
        && startupCleanup!=registrar.npos && firstCapture<secondCapture
        && secondCapture<startupCleanup,
        "startup file cleanup is not gated by two stable registry captures");
    need(registrar.find("GamePassNative")!=registrar.npos
        && registrar.find("PROVIDER-DEFERRED")!=registrar.npos,
        "startup cleaner can rewrite Xbox WGS provider storage");
    need(registrar.find("ScrubCharacterJsonBeforeLoad")!=registrar.npos,
        "provider-backed character JSON preflight is missing");
    need(quests.find("ResolvesPersistenceIdentity")!=quests.npos
        && quests.find("[QUEST-REGISTRY][REENTRY]")!=quests.npos
        && quests.find("preparedInstance==instance")!=quests.npos,
        "same-instance world reentry can skip validation of live quest persistence identities");
    need(registrar.find("ProcessPlayerStateLoad")!=registrar.npos
        && registrar.find("OnPersistentStoreLoadPlayerResult")!=registrar.npos
        && registrar.find("LoadStateFromJson")!=registrar.npos,
        "character preflight does not filter the known player JSON boundaries");
    need(pruner.find("s_cleanupConsumedForProcess")!=pruner.npos
        && registrar.find("Hook::RegisterProcessEventPreCallback")!=registrar.npos
        && registrar.find("[PERSISTENCE-PRUNER][REFLECTED-BOUNDARY-READY]")!=registrar.npos,
        "automatic cleanup cannot attach to an eligible reflected character load");
    need(registrar.find("InstallInlineHook")==registrar.npos
        && registrar.find("s_playerStateLoadHook")==registrar.npos
        && registrar.find("ProcessPlayerStateLoadPreflight")==registrar.npos,
        "mandatory pruning reintroduced an unsafe executable inline detour");
    const auto mandatoryPreflight=registrar.find(
        "m_characterJsonHook = Hook::RegisterProcessEventPreCallback");
    const auto auxiliaryHooks=registrar.find(
        "for (auto* hookPath : SaveLoadHookPaths)");
    need(mandatoryPreflight!=registrar.npos && auxiliaryHooks!=registrar.npos
        && mandatoryPreflight<auxiliaryHooks
        && registrar.find("was isolated after registration failed",
            auxiliaryHooks)!=registrar.npos,
        "an auxiliary registry hook can prevent mandatory character preflight");
    const auto registerAllDefinition=registrar.find(
        "void DragonWildsDataRegistrar::RegisterAll()");
    need(registerAllDefinition!=registrar.npos
        && registrar.find("EnsureCharacterJsonPreflightHook")==registrar.npos
        && registrar.find("ForEachUObject")==registrar.npos,
        "registry refresh still performs repeated global native-hook discovery");
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
    need(fallback!=registrar.npos && fallbackGuard!=registrar.npos
        && registrar.find("ForEachUObject")==registrar.npos,
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
    need(pruner.find("if (cleaned.Removed.empty() && restored.empty()) {")!=pruner.npos,
        "an unchanged character is not a strict no-op");
    need(pruner.find("SaveSnapshotRestore")==pruner.npos
        && pruner.find("defaultRecovery")!=pruner.npos
        && pruner.find("MergeBaseline")!=pruner.npos,
        "baseline recovery is missing or old snapshot replacement machinery returned");
    need(registrar.find("m_pruner.PruneBeforeCharacterLoad")!=registrar.npos
        && pruner.find("PruneCharacterJson(value)")!=pruner.npos
        && registrar.find("SaveCleanup::Plan(source")!=registrar.npos
        && registrar.find("startupRegistry.QuestsComplete = false")!=registrar.npos
        && registrar.find("startupRegistry.JournalsComplete = false")!=registrar.npos
        && registrar.find("&startupRegistry, false, true, true")!=registrar.npos,
        "startup and provider-boundary pruning do not share the unresolved-only plan");
}
