#include "Loader/ModLoadOrder.h"
#include "Loader/ModOrderPolicy.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <nlohmann/json.hpp>
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
    bool ValidModId(const std::string& value) {
        return !value.empty() && value.size() <= 255
            && value.find('/') == std::string::npos
            && value.find('\\') == std::string::npos;
    }
    std::vector<DragonWilds::ModOrderEntry> LoadLegacyOrder(
        const fs::path& path, bool strict)
    {
        std::vector<DragonWilds::ModOrderEntry> entries;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            auto text = Trim(PS::ToWideSafe(line.c_str()));
            if (text.empty() || text.front() == STR(';') || text.front() == STR('#')) continue;
            const auto colon = text.find(STR(':'));
            if (colon == RC::StringType::npos) continue;
            auto name = Trim(text.substr(0, colon));
            auto value = Trim(text.substr(colon + 1));
            if (name.empty()) continue;
            if (strict && value != STR("0") && value != STR("1")) {
                PS::Log<RC::LogLevel::Warning>(
                    STR("Invalid legacy runeschema.txt value for '{}'; expected 0 or 1. Disabled.\n"),
                    name);
                entries.push_back({name, false});
                continue;
            }
            entries.push_back({name, value != STR("0")});
        }
        return entries;
    }
    std::vector<DragonWilds::ModOrderEntry> LoadJsoncOrder(
        const fs::path& path, bool strict)
    {
        if (!fs::is_regular_file(path)) return {};
        if (fs::file_size(path) > 256 * 1024)
            throw std::runtime_error("runeschema.jsonc exceeds 256 KiB");

        std::ifstream file(path);
        auto data = nlohmann::json::parse(file, nullptr, true, true);
        if (!data.is_object() || !data.contains("Mods") || !data.at("Mods").is_array())
            throw std::runtime_error("runeschema.jsonc requires a Mods array");
        if (data.at("Mods").size() > 4096)
            throw std::runtime_error("runeschema.jsonc exceeds the 4096-entry safety limit");

        std::vector<DragonWilds::ModOrderEntry> entries;
        for (const auto& item : data.at("Mods")) {
            if (!item.is_object() || !item.contains("Id") || !item.at("Id").is_string())
                throw std::runtime_error("runeschema.jsonc entries require a string Id");
            const auto id = item.at("Id").get<std::string>();
            if (!ValidModId(id))
                throw std::runtime_error("runeschema.jsonc contains an invalid mod Id");

            bool enabled = true;
            if (item.contains("Enabled")) {
                const auto& value = item.at("Enabled");
                if (value.is_boolean()) enabled = value.get<bool>();
                else if (!strict && value.is_number_integer()
                    && (value.get<int64_t>() == 0 || value.get<int64_t>() == 1))
                    enabled = value.get<int64_t>() != 0;
                else {
                    PS::Log<RC::LogLevel::Warning>(
                        STR("Invalid runeschema.jsonc Enabled value for '{}'; expected true or false. Disabled.\n"),
                        PS::ToWideSafe(id.c_str()));
                    enabled = false;
                }
            }
            entries.push_back({PS::ToWideSafe(id.c_str()), enabled});
        }
        return entries;
    }
    std::vector<std::string> StandaloneJsonComments(const fs::path& path) {
        std::vector<std::string> comments;
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            const auto first = line.find_first_not_of(" \t");
            if (first != std::string::npos && line.compare(first, 2, "//") == 0)
                comments.push_back(line);
        }
        return comments;
    }
    bool WriteJsoncOrder(const fs::path& path,
        const std::vector<DragonWilds::ModOrderEntry>& entries,
        const std::vector<std::string>& preservedComments = {})
    {
        std::error_code directoryError;
        fs::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            PS::Log<RC::LogLevel::Warning>(
                STR("Load-order directory unavailable: {} ({}).\n"),
                path.parent_path().native(),
                PS::ToWideSafe(directoryError.message().c_str()));
            return false;
        }

        nlohmann::ordered_json data;
        data["Mods"] = nlohmann::ordered_json::array();
        for (const auto& entry : entries)
            data["Mods"].push_back({
                {"Id", Narrow(entry.Name)},
                {"Enabled", entry.Enabled}
            });

        std::ofstream output(path, std::ios::trunc);
        if (!output) {
            PS::Log<RC::LogLevel::Warning>(
                STR("Load-order file is not writable: {}.\n"), path.native());
            return false;
        }
        output << "// RuneSchema mod order. Entries are loaded top to bottom.\n"
                  "// Set Enabled to false to disable a mod. JSONC comments are allowed.\n"
                  "// AA_ and ZZ_ folders are enabled implicitly when omitted.\n";
        for (const auto& comment : preservedComments) {
            if (comment.find("RuneSchema mod order") != std::string::npos
                || comment.find("Set Enabled") != std::string::npos
                || comment.find("AA_ and ZZ_") != std::string::npos)
                continue;
            output << comment << '\n';
        }
        output << data.dump(2) << '\n';
        return static_cast<bool>(output);
    }
    void RemoveLegacyOrder(const fs::path& legacy) {
        std::error_code error;
        fs::remove(legacy, error);
        if (error)
            PS::Log<RC::LogLevel::Warning>(
                STR("Could not remove migrated legacy load-order file {}: {}.\n"),
                legacy.native(), PS::ToWideSafe(error.message().c_str()));
    }
}

namespace DragonWilds {
    fs::path ModLoadOrder::GetOrderPath(const fs::path& mods) {
        return mods / "runeschema.jsonc";
    }

    std::vector<ModOrderEntry> ModLoadOrder::Load(const fs::path& path, bool strict) {
        if (path.extension() == ".txt")
            return LoadLegacyOrder(path, strict);

        const auto legacy = path.parent_path() / "runeschema.txt";
        if (!fs::is_regular_file(path) && fs::is_regular_file(legacy)) {
            auto legacyEntries = LoadLegacyOrder(legacy, strict);
            if (Save(path, legacyEntries)) {
                try {
                    auto migrated = LoadJsoncOrder(path, strict);
                    RemoveLegacyOrder(legacy);
                    PS::Log<RC::LogLevel::Normal>(
                        STR("Migrated runeschema.txt to runeschema.jsonc.\n"));
                    return migrated;
                } catch (...) {
                    std::error_code removeError;
                    fs::remove(path, removeError);
                }
            }
            return legacyEntries;
        }

        auto entries = LoadJsoncOrder(path, strict);
        if (fs::is_regular_file(legacy)) {
            RemoveLegacyOrder(legacy);
            PS::Log<RC::LogLevel::Normal>(
                STR("Removed legacy runeschema.txt; runeschema.jsonc is authoritative.\n"));
        }
        return entries;
    }

    bool ModLoadOrder::Save(const fs::path& path,
        const std::vector<ModOrderEntry>& entries)
    {
        return WriteJsoncOrder(path, entries);
    }

    bool ModLoadOrder::SavePreservingComments(const fs::path& path,
        const std::vector<ModOrderEntry>& entries)
    {
        return WriteJsoncOrder(path, entries, StandaloneJsonComments(path));
    }

    std::vector<RC::StringType> ModLoadOrder::Resolve(
        const fs::path& mods, const std::vector<RC::StringType>& discovered)
    {
        auto sorted = discovered;
        std::sort(sorted.begin(), sorted.end());
        const auto& settings = PS::PSConfig::Get()->GetLoadOrderSettings();
        if (!settings.enabled) {
            auto result = settings.deterministicFallback ? sorted : discovered;
            ModOrderPolicy::Apply(result,
                [](const auto& name) -> const auto& { return name; });
            return result;
        }

        const auto path = GetOrderPath(mods);
        const auto legacy = mods / "runeschema.txt";
        const bool hadOrderFile = fs::exists(path) || fs::exists(legacy);
        if (!hadOrderFile && !settings.autoCreate) {
            ModOrderPolicy::Apply(sorted,
                [](const auto& name) -> const auto& { return name; });
            return sorted;
        }

        auto entries = Load(path, settings.strictValues);
        const bool existed = fs::exists(path);
        std::unordered_set<RC::StringType> present(
            discovered.begin(), discovered.end());
        const auto before = entries.size();
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&](const auto& entry) { return !present.contains(entry.Name); }),
            entries.end());
        bool changed = entries.size() != before;

        std::unordered_set<RC::StringType> known;
        for (const auto& entry : entries) known.insert(entry.Name);
        for (const auto& name : sorted) {
            if (!ModOrderPolicy::ShouldAutoPersist(name)
                || !known.insert(name).second)
                continue;
            entries.push_back({name, true});
            changed = true;
        }

        changed = ModOrderPolicy::Apply(entries,
            [](const auto& entry) -> const auto& { return entry.Name; }) || changed;
        if (!existed || (settings.reconcileFolders && changed))
            settings.preserveComments && existed
                ? SavePreservingComments(path, entries)
                : Save(path, entries);

        auto resolved = entries;
        known.clear();
        for (const auto& entry : resolved) known.insert(entry.Name);
        for (const auto& name : sorted) {
            if (ModOrderPolicy::IsImplicit(name) && known.insert(name).second)
                resolved.push_back({name, true});
        }
        ModOrderPolicy::Apply(resolved,
            [](const auto& entry) -> const auto& { return entry.Name; });

        std::vector<RC::StringType> result;
        for (const auto& entry : resolved) {
            if (entry.Enabled) result.push_back(entry.Name);
            else PS::Log<RC::LogLevel::Normal>(
                STR("Skipping mod '{}' (disabled in runeschema.jsonc).\n"),
                entry.Name);
        }
        return result;
    }

    std::set<std::string> ModLoadOrder::ActiveOwners(const fs::path& mods) {
        std::error_code error;
        const bool exists = fs::exists(mods, error);
        if (error) throw std::runtime_error(
            "Mod directory status unavailable; cleanup refused");
        if (!exists) return {};
        if (!fs::is_directory(mods, error) || error)
            throw std::runtime_error(
                "Mod path is not a readable directory; cleanup refused");

        std::vector<RC::StringType> discovered;
        for (const auto& entry : fs::directory_iterator(mods))
            if (PS::ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))
                discovered.push_back(entry.path().filename().native());

        std::set<std::string> active;
        for (const auto& owner : Resolve(mods, discovered))
            active.insert(RC::to_string(owner));
        return active;
    }
}
