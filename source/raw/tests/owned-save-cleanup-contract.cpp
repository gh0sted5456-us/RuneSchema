#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    if(argc!=7)throw std::runtime_error("registrar, provenance, quest service, main loader, pruner, and asset loader sources required");
    const auto registrar=Read(argv[1]),provenance=Read(argv[2]),quests=Read(argv[3]),mainLoader=Read(argv[4]),pruner=Read(argv[5]),assetLoader=Read(argv[6]);
    const auto need=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    need(registrar.find("RegisterProcessEventPostCallback")!=registrar.npos
        &&registrar.find("if(!gameplayPayload)")!=registrar.npos
        &&registrar.find("parameters!=m_nativePolicyParameters")!=registrar.npos
        &&registrar.find("GetCurrentThreadId()!=m_registrationThread")!=registrar.npos
        &&registrar.find("NativePersistenceRecovery::EnsureSessionPolicy")!=registrar.npos
        &&registrar.find("NativePersistencePolicy::Release")==registrar.npos,
        "native load audit must match its payload without restoring the session loading policy");
    const auto beforeLoad=registrar.find("void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad");
    need(pruner.find("if(prepared)*prepared=false;")!=pruner.npos
        &&pruner.find("jsonProperty->SetPropertyValue(jsonAddress, value);")<pruner.find("if(prepared)CharacterLoadAudit::Prepared();")
        &&pruner.find("if(prepared)*prepared=true;")!=pruner.npos,
        "automatic bypass eligibility must follow successful gameplay preflight and native payload writeback, never mere JSON discovery");
    need(registrar.find("m_prepareCharacterRegistries(context)",beforeLoad)<registrar.find("m_pruner.PruneBeforeCharacterLoad",beforeLoad)
        &&mainLoader.find("SetCharacterRegistryPreparation")!=mainLoader.npos
        &&quests.find("RegistryEnrollment::Await(\"quest:\"")!=quests.npos
        &&quests.find("RegistryEnrollment::Complete(\"quest:\"")!=quests.npos
        &&assetLoader.find("RegistryEnrollment::Await(\"clone:\"")!=assetLoader.npos
        &&assetLoader.find("RegistryEnrollment::Complete(\"clone:\"")!=assetLoader.npos,
        "world-dependent registration is not completed before character cleanup");
    need(registrar.find("OwnedContent::CompareSnapshot")==registrar.npos
        && registrar.find("OwnedContent::CommitSnapshot")==registrar.npos,
        "automatic pruning still depends on an ownership ledger");
    need(mainLoader.find("OwnedContent::BeginSnapshot")==mainLoader.npos,
        "startup still creates an ownership ledger");
    const auto fallbackHook=mainLoader.find("HookName=TEXT(\"CoreStartupFallback\")");
    const auto fallbackPhase=mainLoader.find(
        "SetupGameInstanceInitLoadersOnce();",fallbackHook);
    const auto fallbackRemoval=mainLoader.find("iteration.RemoveSelf();",fallbackHook);
    need(fallbackHook!=mainLoader.npos && fallbackPhase!=mainLoader.npos
        && fallbackRemoval!=mainLoader.npos && fallbackPhase<fallbackRemoval,
        "game-thread core fallback can retire before item registration and save pruning start");
    need(registrar.find("CleanLocalCharacterSavesOnce")!=registrar.npos
        && registrar.find("ConfigFiles::Write(path, encoded)")!=registrar.npos
        && registrar.find("BackupCharacterSave(path, original)")!=registrar.npos
        && registrar.find("failed post-write verification")!=registrar.npos,
        "startup cleanup is missing verified backup-first atomic replacement");
    need(registrar.find("CharacterBackupRetention = 5")!=registrar.npos
        && registrar.find("OwnedCharacterBackups(source)")!=registrar.npos
        && registrar.find("[PERSISTENCE-BACKUP][REUSED]")!=registrar.npos
        && registrar.find("PruneCharacterBackups(path)")!=registrar.npos
        && registrar.find("[PERSISTENCE-BACKUP][PRUNED]")!=registrar.npos,
        "RuneSchema character backups are not deduplicated and retention bounded");
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
    need(pruner.find("s_cleanupConsumedForProcess")==pruner.npos
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
    const auto preflightEnd=registrar.find(
        "void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad",
        mandatoryPreflight);
    need(mandatoryPreflight!=registrar.npos && auxiliaryHooks==registrar.npos
        && preflightEnd!=registrar.npos
        && registrar.find("RegisterAll();",mandatoryPreflight)==registrar.npos,
        "character preflight can refresh registries during a world transition");
    const auto registerAllDefinition=registrar.find(
        "void DragonWildsDataRegistrar::RegisterAll()");
    need(registerAllDefinition!=registrar.npos
        && registrar.find("EnsureCharacterJsonPreflightHook")==registrar.npos
        && registrar.find("ForEachUObject")==registrar.npos,
        "registry refresh still performs repeated global native-hook discovery");
    const auto readyGate=pruner.find("if (!registry || !registry->Ready())");
    const auto plan=pruner.find("SaveCleanup::Plan(",readyGate);
    need(readyGate!=pruner.npos && plan!=pruner.npos && readyGate<plan
        && pruner.find("Every incoming gameplay payload is checked")!=pruner.npos,
        "every character load must be checked after certified registry readiness, without a process-wide one-shot");
    need(pruner.find("FString replacement(")<pruner.find("characterJson=std::move(replacement)")
        && pruner.find("verified != serialized")<pruner.find("characterJson=std::move(replacement)"),
        "native cleanup writes before verifying the replacement payload");
    need(pruner.find("m_checkedCharacters")==pruner.npos
        && pruner.find("Save cleanup | missing character IDs removed=")!=pruner.npos
        && pruner.find("[PERSISTENCE-PRUNER][ORPHAN-REMOVED]")!=pruner.npos,
        "cleanup is still per-character or no longer reports removed orphan IDs");
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
    need(registrar.find("RegisterInitGameStatePreCallback")==registrar.npos
        && registrar.find("SaveLoadHookPaths")==registrar.npos
        && registrar.find("[REGISTRY][LIFECYCLE][SEALED]")!=registrar.npos,
        "registry mutation is not sealed to the one-time startup boundary");
    need(registrar.find("fingerprint != m_registryCandidateFingerprint")!=registrar.npos
        && registrar.find("PublishRegistry({});\n                return;")!=registrar.npos,
        "character cleanup can consume an unsettled registry snapshot");
    need(registrar.find("const auto registered = RegisterMissing")!=registrar.npos
        && registrar.find("if (binding.CleanupAuthority)")!=registrar.npos
        && registrar.find("registrationsComplete = registered && registrationsComplete")!=registrar.npos
        && registrar.find("itemsReady && recipesReady && registrationsComplete")!=registrar.npos
        && registrar.find("primary persistence registry rejected the asset")!=registrar.npos
        && registrar.find("network registry rejected the asset")!=registrar.npos
        && registrar.find("does not round-trip to one live data asset")!=registrar.npos
        && registrar.find("duplicate PersistenceID resolves to multiple live assets")!=registrar.npos,
        "cleanup readiness ignores a loaded asset that failed identity round-trip, uniqueness, primary, or network registration");
    need(registrar.find("CombatSpellDataSubsystem")!=registrar.npos
        && registrar.find("UtilitySpellDataSubsystem")!=registrar.npos
        && registrar.find("HeldEquipmentEffectDataSubsystem")!=registrar.npos
        && registrar.find("false,")!=registrar.npos,
        "combat persistence registries are missing or can become cleanup authority");
    need(registrar.find("[REGISTRY][{}][ADDED]")!=registrar.npos
        && registrar.find("[REGISTRY][{}][SOURCE-SUMMARY]")!=registrar.npos
        && registrar.find("TEXT(\"ITEM\")")!=registrar.npos
        && registrar.find("TEXT(\"RECIPE\")")!=registrar.npos
        && registrar.find("TEXT(\"QUEST\")")!=registrar.npos
        && registrar.find("TEXT(\"COMBAT-SPELL\")")!=registrar.npos
        && registrar.find("TEXT(\"UTILITY-SPELL\")")!=registrar.npos
        && registrar.find("TEXT(\"EQUIPMENT-EFFECT\")")!=registrar.npos
        && registrar.find("source='{}' added={} existing={}")!=registrar.npos
        && registrar.find("ordered_fingerprint={}")!=registrar.npos
        && registrar.find("[COMBAT-REGISTRY][{}][{}]")!=registrar.npos
        && registrar.find("TEXT(\"MELEE-ATTACK\")")!=registrar.npos
        && registrar.find("TEXT(\"RANGED-EQUIPMENT\")")!=registrar.npos,
        "combat registry counts, content sources and identity fingerprint are not reported without per-asset spam");
    need(provenance.find("AnnounceRuneSchemaItem")!=provenance.npos
        && provenance.find("RuneSchemaItems")!=provenance.npos
        && assetLoader.find("RegistryProvenance::AnnounceRuneSchemaItem")!=assetLoader.npos
        && assetLoader.find("source=runeschema")!=assetLoader.npos
        && registrar.find("[REGISTRY][ITEM][PROVENANCE]")!=registrar.npos
        && registrar.find("[REGISTRY][ITEM][UNRESOLVED-RUNESCHEMA]")!=registrar.npos
        && registrar.find("cooked_or_pak_added")!=registrar.npos,
        "item registration does not verify or report RuneSchema versus cooked/PAK provenance");
    need(assetLoader.find("networkId=deferred-to-settled-registrar")!=assetLoader.npos
        && assetLoader.find("const bool networkDeferred = PS::NetworkIdentityState::DeferItemEnrollment()")!=assetLoader.npos
        && registrar.find("PS::NetworkIdentityState::FinishStartupBatch();")!=registrar.npos
        && assetLoader.find("auto* stored = map.FindValue(&key)")!=assetLoader.npos
        && assetLoader.find("reverse.Rehash()") == assetLoader.npos,
        "startup peers still differ in early clone network enrollment or map insertion is not round-trip verified");
    need(registrar.find("[REGISTRY][LIFECYCLE][SEALED]")!=registrar.npos
        && registrar.find("world transitions are read-only")!=registrar.npos,
        "one-time registry lifecycle is not announced");
    const auto pakPreload=registrar.find("PreloadMountedPersistenceAssets();");
    const auto firstRegistration=registrar.find("RegisterAll();",pakPreload);
    need(pakPreload!=registrar.npos && firstRegistration!=registrar.npos
        && pakPreload<firstRegistration
        && registrar.find("MaxPersistencePreloads = 32768")!=registrar.npos
        && registrar.find("PersistenceLoadTranche = 512")!=registrar.npos
        && registrar.find("UAssetRegistryHelpers::GetAsset(asset)")!=registrar.npos
        && registrar.find("[REGISTRY][PAK-DISCOVERY][SUMMARY]")!=registrar.npos
        && registrar.find("bFAssetDataAvailable")==registrar.npos
        && registrar.find("Live Asset Registry query failed")!=registrar.npos
        && registrar.find("asset.AssetClass().ToString()")!=registrar.npos
        && registrar.find("[REGISTRY][PAK-DISCOVERY][NO-CANDIDATES]")!=registrar.npos
        && registrar.find("mounted_records={}")!=registrar.npos
        && registrar.find("FailureDetailLimit = 12")!=registrar.npos
        && registrar.find("m_pakDiscoveryComplete")==registrar.npos
        && registrar.find("registrationsComplete = true")!=registrar.npos,
        "mounted persistence assets are not bounded, condensed, and loaded before registry sealing");
    need(registrar.find("[LIFECYCLE][COMBAT-COMPONENT][READY]")!=registrar.npos
        && registrar.find("Hook::RegisterProcessEventPostCallback")!=registrar.npos
        && registrar.find("IsCombatComponentReadyFunction")!=registrar.npos
        && registrar.find("component->GetWorld() != world")!=registrar.npos
        && registrar.find("m_ownedAdditionalWeaponAttackRoots")!=registrar.npos
        && registrar.find("attack->SetRootSet()")!=registrar.npos
        && registrar.find("attack->ClearRootSet()")!=registrar.npos
        && registrar.find("m_initialCombatFallbackAttempted")!=registrar.npos
        && registrar.find("INITIAL-WORLD-FALLBACK")!=registrar.npos
        && registrar.find("combat.enabled && combat.initialWorldMutation")!=registrar.npos
        && registrar.find("m_initialCombatFallbackAttempted = true")!=registrar.npos
        && registrar.find("GetClassDefaultObject().Get()")!=registrar.npos
        && registrar.find("const bool defaultTemplate = world == nullptr")!=registrar.npos
        && registrar.find("defaultTemplate\n                || allowInitialWorldFallback")!=registrar.npos
        && registrar.find("declared->IsChildOf(expectedAttackClass)")!=registrar.npos
        && registrar.find("runtime mutation is intentionally disabled")!=registrar.npos
        && registrar.find("QuickAttackData")!=registrar.npos
        && registrar.find("FullAttackData")!=registrar.npos,
        "custom melee and item-owned ranged attacks are not installed once on component defaults with read-only world validation");
    need(pruner.find("if (cleaned.Removed.empty() && restored.empty()) {")!=pruner.npos,
        "an unchanged character is not a strict no-op");
    need(pruner.find("SaveSnapshotRestore")==pruner.npos
        && pruner.find("defaultRecovery")!=pruner.npos
        && pruner.find("MergeBaseline")!=pruner.npos,
        "baseline recovery is missing or old snapshot replacement machinery returned");
    need(registrar.find("m_pruner.PruneBeforeCharacterLoad")!=registrar.npos
        && pruner.find("PruneCharacterJson(value,&prepared)")!=pruner.npos
        && registrar.find("SaveCleanup::Plan(source")!=registrar.npos
        && registrar.find("startupRegistry.QuestsComplete = false")!=registrar.npos
        && registrar.find("startupRegistry.JournalsComplete = false")!=registrar.npos
        && registrar.find("&startupRegistry, false, true, true")!=registrar.npos,
        "startup and provider-boundary pruning do not share the unresolved-only plan");
    const auto reflectedPreflight=pruner.find("bool PersistencePruner::PruneBeforeCharacterLoad");
    const auto directPreflight=pruner.find("std::size_t PersistencePruner::PruneCharacterJson");
    const auto registryGuard=pruner.find("if (!registry || !registry->Ready())",reflectedPreflight);
    need(registryGuard!=pruner.npos&&registryGuard<pruner.find("ReflectedContract::Frame(function)",reflectedPreflight),
        "uncertified cleanup walks native parameters before allowing the original load through");
    need(pruner.find("if (!registry || !registry->Ready())",directPreflight)<pruner.find("json::parse(utf8",directPreflight),
        "uncertified cleanup parses a character payload even though it cannot modify it");
    need(registrar.find("Non-blocking: native loading continues")!=registrar.npos
        &&pruner.find("Non-blocking: native character loading continues")!=pruner.npos,
        "deferred cleanup diagnostics must distinguish a skipped optional cleanup from a rejected character");
}
