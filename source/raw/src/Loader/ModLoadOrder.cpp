#include "Loader/ModLoadOrder.h"
#include "Loader/ModOrderPolicy.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include "Utility/Config.h"
#include "Utility/Logging.h"

namespace fs = std::filesystem;
namespace {
    RC::StringType Trim(const RC::StringType& v) {
        const auto first = v.find_first_not_of(STR(" \t\r\n"));
        if (first == RC::StringType::npos) return {};
        return v.substr(first, v.find_last_not_of(STR(" \t\r\n")) - first + 1);
    }
    std::string Narrow(const RC::StringType& v) {
        std::string out; out.reserve(v.size());
        for (auto c : v) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        return out;
    }
}

namespace DragonWilds {
    fs::path ModLoadOrder::GetOrderPath(const fs::path& mods) { return mods / "runeschema.txt"; }
    std::vector<ModOrderEntry> ModLoadOrder::Load(const fs::path& path, bool strict) {
        std::vector<ModOrderEntry> entries; std::ifstream file(path); std::string line;
        while (std::getline(file, line)) {
            auto text = Trim(PS::ToWideSafe(line.c_str()));
            if (text.empty() || text.front() == STR(';') || text.front() == STR('#')) continue;
            const auto colon = text.find(STR(':')); if (colon == RC::StringType::npos) continue;
            auto name = Trim(text.substr(0, colon)); auto value = Trim(text.substr(colon + 1));
            if (name.empty()) continue;
            if (strict && value != STR("0") && value != STR("1")) {
                PS::Log<RC::LogLevel::Warning>(STR("Invalid runeschema.txt value for '{}'; expected 0 or 1. Disabled.\n"), name);
                entries.push_back({name, false}); continue;
            }
            entries.push_back({name, value != STR("0")});
        }
        return entries;
    }
    bool ModLoadOrder::Save(const fs::path& path, const std::vector<ModOrderEntry>& entries) {
        std::error_code directoryError;
        fs::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            PS::Log<RC::LogLevel::Warning>(STR("Load-order directory unavailable: {} ({}).\n"),
                path.parent_path().native(), PS::ToWideSafe(directoryError.message().c_str()));
            return false;
        }
        std::ofstream file(path, std::ios::trunc);
        if (!file) { PS::Log<RC::LogLevel::Warning>(STR("Load-order file is not writable: {}.\n"), path.native()); return false; }
        file << "; RuneSchema mod order - loaded top to bottom.\n"
                "; Use 1 to enable and 0 to disable.\n"
                "; AA_ and ZZ_ folders are enabled implicitly when omitted. Add one here only to override it.\n";
        for (const auto& e : entries) file << Narrow(e.Name) << " : " << (e.Enabled ? 1 : 0) << '\n';
        return static_cast<bool>(file);
    }
    bool ModLoadOrder::SavePreservingComments(const fs::path& path, const std::vector<ModOrderEntry>& entries) {
        std::vector<std::string> comments; std::ifstream input(path); std::string line;
        while (std::getline(input, line)) {
            auto text = Trim(PS::ToWideSafe(line.c_str()));
            if (text.empty() || text.front() == STR(';') || text.front() == STR('#') || text.find(STR(':')) == RC::StringType::npos)
                comments.push_back(line);
        }
        std::error_code directoryError;
        fs::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            PS::Log<RC::LogLevel::Warning>(STR("Load-order directory unavailable: {} ({}).\n"),
                path.parent_path().native(), PS::ToWideSafe(directoryError.message().c_str()));
            return false;
        }
        std::ofstream output(path, std::ios::trunc);
        if (!output) { PS::Log<RC::LogLevel::Warning>(STR("Load-order file is not writable: {}.\n"), path.native()); return false; }
        for (const auto& c : comments) output << c << '\n';
        for (const auto& e : entries) output << Narrow(e.Name) << " : " << (e.Enabled ? 1 : 0) << '\n';
        return static_cast<bool>(output);
    }
    std::vector<RC::StringType> ModLoadOrder::Resolve(const fs::path& mods, const std::vector<RC::StringType>& discovered) {
        auto sorted = discovered; std::sort(sorted.begin(), sorted.end());
        const auto& settings = PS::PSConfig::Get()->GetLoadOrderSettings();
        if (!settings.enabled) {
            auto result = settings.deterministicFallback ? sorted : discovered;
            ModOrderPolicy::Apply(result, [](const auto& name) -> const auto& { return name; });
            return result;
        }
        const auto path = GetOrderPath(mods); const bool existed = fs::exists(path);
        if (!existed && !settings.autoCreate) {
            ModOrderPolicy::Apply(sorted, [](const auto& name) -> const auto& { return name; });
            return sorted;
        }
        auto entries = Load(path, settings.strictValues);
        std::unordered_set<RC::StringType> present(discovered.begin(), discovered.end());
        const auto before = entries.size();
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& e) { return !present.contains(e.Name); }), entries.end());
        bool changed = entries.size() != before;
        std::unordered_set<RC::StringType> known; for (const auto& e : entries) known.insert(e.Name);
        for (const auto& name : sorted) {
            if (!ModOrderPolicy::ShouldAutoPersist(name) || !known.insert(name).second) continue;
            entries.push_back({name, true});
            changed = true;
        }
        changed = ModOrderPolicy::Apply(entries, [](const auto& e) -> const auto& { return e.Name; }) || changed;
        if (!existed || (settings.reconcileFolders && changed))
            settings.preserveComments && existed ? SavePreservingComments(path, entries) : Save(path, entries);
        auto resolved = entries;
        known.clear();
        for (const auto& entry : resolved) known.insert(entry.Name);
        for (const auto& name : sorted) {
            if (ModOrderPolicy::IsImplicit(name) && known.insert(name).second)
                resolved.push_back({name, true});
        }
        ModOrderPolicy::Apply(resolved, [](const auto& e) -> const auto& { return e.Name; });
        std::vector<RC::StringType> result;
        for (const auto& e : resolved) {
            if (e.Enabled) result.push_back(e.Name);
            else PS::Log<RC::LogLevel::Normal>(STR("Skipping mod '{}' (disabled in runeschema.txt).\n"), e.Name);
        }
        return result;
    }
    std::set<std::string> ModLoadOrder::ActiveOwners(const fs::path& mods) {
        std::error_code error;
        const bool exists=fs::exists(mods,error);
        if(error)throw std::runtime_error("Mod directory status unavailable; cleanup refused");
        if(!exists)return {};
        if(!fs::is_directory(mods,error) || error)throw std::runtime_error("Mod path is not a readable directory; cleanup refused");
        std::vector<RC::StringType> discovered;
        for (const auto& entry : fs::directory_iterator(mods))
            if (entry.is_directory()) discovered.push_back(entry.path().filename().native());
        std::set<std::string> active;
        for (const auto& owner : Resolve(mods, discovered)) active.insert(RC::to_string(owner));
        return active;
    }
}
