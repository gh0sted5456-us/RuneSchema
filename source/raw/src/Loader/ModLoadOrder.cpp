#include "Loader/ModLoadOrder.h"
#include "Loader/ModOrderPolicy.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/ModFolderLayout.h"

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
    std::string FoldName(const RC::StringType& name) {
        return PS::ModFolderLayout::FoldAscii(Narrow(name));
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
                PS::Log<RC::LogLevel::Warning>(STR("Invalid {} value for '{}'; expected 0 or 1. Disabled.\n"),
                    path.filename().native(), name);
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
        // Explicit runeschema.txt order is authoritative. Never alphabetize
        // discovery: newly found mods are appended in the order supplied by
        // the directory scan, while every existing user-arranged row stays in
        // exactly the same position.
        auto fallback = discovered;
        const auto path = GetOrderPath(mods);
        const bool existed = fs::exists(path);
        const auto& settings = PS::PSConfig::Get()->GetLoadOrderSettings();
        auto entries = existed ? Load(path, settings.strictValues)
                               : std::vector<ModOrderEntry>{};

        // runeschema.txt is an enablement authority even when its optional
        // ordering/reconciliation feature is disabled. A disabled row wins
        // over every duplicate spelling of the same Windows folder name.
        std::unordered_set<std::string> explicitlyDisabled;
        for (const auto& entry : entries)
            if (!entry.Enabled) explicitlyDisabled.insert(FoldName(entry.Name));
        const auto filterExplicitlyDisabled = [&](const std::vector<RC::StringType>& names) {
            std::vector<RC::StringType> allowed;
            allowed.reserve(names.size());
            for (const auto& name : names) {
                if (!explicitlyDisabled.contains(FoldName(name))) allowed.push_back(name);
                else PS::Log<RC::LogLevel::Normal>(STR(
                    "Skipping mod '{}' (disabled in RuneSchema/mods/runeschema.txt).\n"), name);
            }
            return allowed;
        };

        if (!settings.enabled) {
            return filterExplicitlyDisabled(
                settings.deterministicFallback ? fallback : discovered);
        }

        if (!existed && !settings.autoCreate) {
            return settings.deterministicFallback ? fallback : discovered;
        }

        std::unordered_map<std::string, RC::StringType> discoveredByName;
        discoveredByName.reserve(discovered.size());
        for (const auto& name : discovered)
            discoveredByName.try_emplace(FoldName(name), name);

        std::vector<ModOrderEntry> normalized;
        normalized.reserve(entries.size());
        std::unordered_map<std::string, std::size_t> normalizedIndex;
        bool changed = false;
        for (const auto& entry : entries) {
            const auto key = FoldName(entry.Name);
            const auto discoveredEntry = discoveredByName.find(key);
            if (discoveredEntry == discoveredByName.end()) {
                changed = true;
                continue;
            }
            const auto [position, inserted] = normalizedIndex.try_emplace(
                key, normalized.size());
            if (!inserted) {
                // Zero wins across duplicate rows, regardless of casing.
                normalized[position->second].Enabled =
                    normalized[position->second].Enabled && entry.Enabled;
                changed = true;
                continue;
            }
            normalized.push_back({discoveredEntry->second, entry.Enabled});
            changed = changed || entry.Name != discoveredEntry->second;
        }
        entries = std::move(normalized);

        std::unordered_set<std::string> known;
        for (const auto& entry : entries) known.insert(FoldName(entry.Name));

        // Unlisted ordinary/numeric mods are appended in discovery order.
        // Existing runeschema.txt rows never move.
        for (const auto& name : fallback) {
            if (!ModOrderPolicy::ShouldAutoPersist(name)
                || !known.insert(FoldName(name)).second) continue;
            entries.push_back({name, true});
            changed = true;
        }

        if (!existed || (settings.reconcileFolders && changed))
            settings.preserveComments && existed ? SavePreservingComments(path, entries) : Save(path, entries);

        // AA_/ZZ_ folders are implicit when omitted. Keep that convenience
        // without re-sorting explicit rows: omitted AA_ goes before the file,
        // omitted ZZ_ goes after it. If explicitly listed, its exact row wins.
        std::vector<ModOrderEntry> resolved;
        resolved.reserve(entries.size() + fallback.size());

        for (const auto& name : fallback) {
            if (!ModOrderPolicy::IsImplicit(name)
                || known.contains(FoldName(name))) continue;
            if (ModOrderPolicy::Priority(name) == 0)
                resolved.push_back({name, true});
        }

        resolved.insert(resolved.end(), entries.begin(), entries.end());

        for (const auto& name : fallback) {
            if (!ModOrderPolicy::IsImplicit(name)
                || known.contains(FoldName(name))) continue;
            if (ModOrderPolicy::Priority(name) != 0)
                resolved.push_back({name, true});
        }

        std::vector<RC::StringType> result;
        result.reserve(resolved.size());
        for (const auto& e : resolved) {
            if (e.Enabled) result.push_back(e.Name);
            else PS::Log<RC::LogLevel::Normal>(STR(
                "Skipping mod '{}' (disabled in RuneSchema/mods/runeschema.txt).\n"), e.Name);
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
            if (PS::ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))
                discovered.push_back(entry.path().filename().native());
        std::set<std::string> active;
        for (const auto& owner : Resolve(mods, discovered)) active.insert(RC::to_string(owner));
        return active;
    }
}
