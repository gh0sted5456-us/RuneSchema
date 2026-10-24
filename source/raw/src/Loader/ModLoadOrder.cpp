#include "Loader/ModLoadOrder.h"
#include "Loader/ModOrderPolicy.h"
#include <stdexcept>
#include <unordered_set>
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/ModFolderLayout.h"

namespace fs=std::filesystem;
namespace {
std::string FoldName(const RC::StringType& name) {
    return DragonWilds::ModOrderState::Fold(RC::to_string(name));
}
std::vector<DragonWilds::ModOrderState::Entry> ToEntries(const std::vector<DragonWilds::ModOrderEntry>& entries) {
    std::vector<DragonWilds::ModOrderState::Entry> result;
    for(const auto& entry:entries)result.push_back({RC::to_string(entry.Name),entry.Enabled});
    return result;
}
bool WriteOrder(const fs::path& path,const std::vector<DragonWilds::ModOrderEntry>& entries,
    bool preserveComments,const DragonWilds::ModOrderState::Snapshot* expected) {
    try {
        const auto baseline=expected?*expected:DragonWilds::ModOrderState::Read(path);
        const auto contents=DragonWilds::ModOrderState::Serialize(ToEntries(entries),baseline,preserveComments);
        // ConfigFiles creates the parent and replaces a complete staged file.
        // Never truncate runeschema.txt in place or overwrite a newer snapshot.
        if(!DragonWilds::ModOrderState::WriteExpected(path,baseline,contents)) {
            PS::LogStructuredFallback<RC::LogLevel::Warning>(
                PS::StructuredDiagnosticB3C::Report("RS-CORE-040","Warning","Skipped","LoaderCore",
                "Load-order save refused","The on-disk load order has changed; reload it before saving.",{},path.generic_string(),"ModLoadOrder","Validation"),
                STR("Load order changed on disk; save was not applied. Reload before saving.\n"));
            return false;
        }
        return true;
    } catch(const std::exception& error) {
        PS::LogStructuredFallback<RC::LogLevel::Warning>(
            PS::StructuredDiagnosticB3C::Report("RS-CORE-041","Error","Failed","LoaderCore",
                "Load-order save failed",error.what(),{},path.generic_string(),"ModLoadOrder","Finalization"),
            STR("Could not save load order: {}.\n"),PS::ToWideSafe(error.what()));
        return false;
    }
}
}
namespace DragonWilds {
fs::path ModLoadOrder::GetOrderPath(const fs::path& mods){return mods/"runeschema.txt";}
std::vector<ModOrderEntry> ModLoadOrder::Parse(std::string_view contents,bool /*strictValues*/) {
    std::vector<ModOrderEntry> result;
    for(const auto& entry:ModOrderState::Parse(contents))result.push_back({PS::ToWideSafe(entry.Name.c_str()),entry.Enabled});
    return result;
}
std::vector<ModOrderEntry> ModLoadOrder::Load(const fs::path& path,bool strictValues) {
    return Parse(ModOrderState::Read(path).Contents,strictValues);
}
bool ModLoadOrder::Save(const fs::path& path,const std::vector<ModOrderEntry>& entries,const ModOrderState::Snapshot* expected) {
    return WriteOrder(path,entries,false,expected);
}
bool ModLoadOrder::SavePreservingComments(const fs::path& path,const std::vector<ModOrderEntry>& entries,const ModOrderState::Snapshot* expected) {
    return WriteOrder(path,entries,true,expected);
}
std::vector<RC::StringType> ModLoadOrder::Resolve(const fs::path& mods,const std::vector<RC::StringType>& discovered) {
    // Explicit runeschema.txt order is authoritative. Missing folders retain
    // their rows so updates/removals never erase a user's disable choice.
    const auto path=GetOrderPath(mods);
    const auto snapshot=ModOrderState::Read(path); // Read failures are not an empty list.
    const auto& settings=PS::PSConfig::Get()->GetLoadOrderSettings();
    const auto entries=Parse(snapshot.Contents,settings.strictValues);
    // A disabled row wins across aliases in both the runtime and editor.
    // Zero wins across duplicate rows; the pure reconciliation plan is shared.
    std::unordered_set<std::string> disabled;
    for(const auto& entry:entries)if(!entry.Enabled)disabled.insert(FoldName(entry.Name));
    const auto logDisabled=[](const RC::StringType& name) {
        PS::Log<RC::LogLevel::Normal>(STR("Skipping mod '{}' (disabled in RuneSchema/mods/runeschema.txt).\n"),name);
    };
    if(!settings.enabled||(!snapshot.Exists&&!settings.autoCreate)) {
        std::vector<RC::StringType> result;
        for(const auto& name:discovered)if(!disabled.contains(FoldName(name)))result.push_back(name);else logDisabled(name);
        return result;
    }
    std::vector<std::string> names;
    for(const auto& name:discovered)names.push_back(RC::to_string(name));
    // Unlisted ordinary/numeric mods are appended in discovery order.
    const auto plan=ModOrderState::Reconcile(ToEntries(entries),names);
    if(!snapshot.Exists||(settings.reconcileFolders&&plan.Changed)) {
        std::vector<ModOrderEntry> normalized;
        for(const auto& entry:plan.Entries)normalized.push_back({PS::ToWideSafe(entry.Name.c_str()),entry.Enabled});
        const bool saved=settings.preserveComments&&snapshot.Exists
            ? SavePreservingComments(path,normalized,&snapshot):Save(path,normalized,&snapshot);
        if(!saved&&ModOrderState::Read(path)!=snapshot)
            throw std::runtime_error("Load order changed during discovery; refusing stale enablement decisions");
    }
    std::vector<RC::StringType> result;
    for(const auto& name:plan.Active)result.push_back(PS::ToWideSafe(name.c_str()));
    for(const auto& name:discovered)if(disabled.contains(FoldName(name)))logDisabled(name);
    return result;
}
std::set<std::string> ModLoadOrder::ActiveOwners(const fs::path& mods) {
    std::error_code error;
    const bool exists=fs::exists(mods,error);
    if(error)throw std::runtime_error("Mod directory status unavailable; cleanup refused");
    if(!exists)return {};
    if(!fs::is_directory(mods,error)||error)throw std::runtime_error("Mod path is not a readable directory; cleanup refused");
    std::vector<RC::StringType> discovered;
    for(const auto& entry:fs::directory_iterator(mods))
        if(PS::ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))discovered.push_back(entry.path().filename().native());
    std::set<std::string> active;
    for(const auto& owner:Resolve(mods,discovered))active.insert(RC::to_string(owner));
    return active;
}
}
