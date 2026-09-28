#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("Journal loader source required");
    std::ifstream file(argv[1]);
    if (!file) throw std::runtime_error("Cannot read journal loader source");
    const std::string source{std::istreambuf_iterator<char>(file), {}};
#define require(ok) do { if (!(ok)) { std::cerr << "Journal lifecycle regression at line " << __LINE__ << '\n'; throw std::runtime_error("Journal lifecycle regression"); } } while (0)
    require(source.find("m_initialJournalApplied = result.ErrorCount == 0") == std::string::npos);
    require(source.find("m_initialJournalApplied = true;") != std::string::npos);
    const auto hook = source.find("PS::RegisterNativePostHook(function");
    const auto hookEnd = source.find("m_hooksActive = true", hook);
    require(hook != std::string::npos && hookEnd != std::string::npos);
    require(source.substr(hook, hookEnd-hook).find("ApplyAll()") == std::string::npos);
    require(source.substr(hook, hookEnd-hook).find("if(!hookId)") != std::string::npos);
    require(source.substr(hook, hookEnd-hook).find("catch(...)") != std::string::npos);
    require(source.find("parameterCount!=2") < hook);
    require(source.find("/Script/Dominion.DominionDataAssetNetId") < hook);
    const auto apply = source.find("DragonWildsJournalModLoader::LoadResult DragonWildsJournalModLoader::ApplyAll()");
    require(source.find("PS::JournalPlacement::Parse", apply) < source.find("ResolveOrCreate(def)", apply));
    require(source.find("if (m_rejectedEntries.contains(def.Key)) continue;", apply) != std::string::npos);
    require(source.find("m_rejectedEntries.insert(def.Key)", apply) != std::string::npos);
    require(source.find("auto* entry = found->second.Get();") != std::string::npos);
    require(source.find("UObject* entry = found->second.Get();") != std::string::npos);
    require(source.find("m_entries.emplace(def.Key, FWeakObjectPtr(entry))") == std::string::npos);
    require(source.find("m_entries.emplace(def.Key, EntryHandle(entry))") != std::string::npos);
    require(source.find("Serial!=0 && Serial!=current") != std::string::npos);
    require(source.find("slot->IsRootSet()") != std::string::npos);
    require(source.find("requested entries unavailable") != std::string::npos);
    require(source.find("JournalPlayerAccess::EnsureUnlocked(journalComponent,entry)") != std::string::npos);
    require(source.find("ensureInArray") == std::string::npos);
    require(source.find("m_knownIds") == std::string::npos);
    require(source.find("InstallNativePersistence();") != std::string::npos);
    require(source.find("if (!m_ownedIds.empty())") != std::string::npos);
    require(source.find("!m_nativePersistenceReady") != std::string::npos);
    require(source.find("[FEATURE:journal-save-cleanup][UNAVAILABLE]") != std::string::npos);
    const auto persistence = source.find("InstallNativePersistence();", apply);
    const auto hooks = source.find("RegisterHooks();", persistence);
    require(persistence != std::string::npos && hooks != std::string::npos);
    require(source.substr(persistence, hooks-persistence).find("catch (const std::exception& error)") != std::string::npos);
    require(source.find("JournalPersistence::Install(this,") != std::string::npos);
    require(source.find("if (!m_nativePersistenceReady)") != std::string::npos);
    require(source.find("JournalPlayerAccess::EnsureUnlocked(journalComponent,entry)") != std::string::npos);
    require(source.find("StripUnusableIdsFromCharacterSave") == std::string::npos);
    require(source.find("SaveCharacters") == std::string::npos);
    const auto registered=source.find("RegisterEntry(entry,def.Owner);",apply);
    const auto placed=source.find("Place(entry, def)",registered);
    const auto owned=source.find("TrackOwnedId(entry,persistence,def.Owner,def.Declared);",placed);
    require(registered<placed && placed<owned);
    require(source.find("found->Owner!=modName")!=std::string::npos);
    require(source.find("for (const auto* fieldName : {TEXT(\"PersistenceID\"), TEXT(\"InternalName\")})")!=std::string::npos);
    require(source.find("std::memcpy(reverse.GetValuePtr") == std::string::npos);
    std::cout << "PASS: journal source guards for preflight, failure isolation, rooted cache and no character-load retry.\n";
}
