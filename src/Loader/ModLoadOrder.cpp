#include "Loader/ModLoadOrder.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>

#include "Utility/Logging.h"
#include "Utility/Config.h"

namespace fs = std::filesystem;

namespace {
    RC::StringType Trim(const RC::StringType& value)
    {
        auto start = value.find_first_not_of(STR(" \t\r\n"));
        if (start == RC::StringType::npos)
        {
            return RC::StringType{};
        }

        auto end = value.find_last_not_of(STR(" \t\r\n"));
        return value.substr(start, end - start + 1);
    }

    // mods.txt is plain ASCII on disk (mod folder names only need to make sense to
    // the filesystem and to .pak content), so this mirrors PS::ToWideSafe()'s
    // lossy-but-safe narrowing in the other direction.
    std::string NarrowFromWide(const RC::StringType& value)
    {
        std::string narrow;
        narrow.reserve(value.size());
        for (auto character : value)
        {
            narrow.push_back(character < 0x80 ? static_cast<char>(character) : '?');
        }
        return narrow;
    }
}

namespace DragonWilds {
    std::filesystem::path ModLoadOrder::GetModsTxtPath(const std::filesystem::path& modsFolderPath)
    {
        return modsFolderPath / "mods.txt";
    }

    std::vector<ModOrderEntry> ModLoadOrder::Load(
        const std::filesystem::path& modsTxtPath,
        bool strictValues)
    {
        std::vector<ModOrderEntry> entries;

        std::ifstream file(modsTxtPath);
        if (!file.is_open())
        {
            return entries;
        }

        std::string line;
        while (std::getline(file, line))
        {
            auto trimmed = Trim(PS::ToWideSafe(line.c_str()));
            if (trimmed.empty() || trimmed.front() == STR(';') || trimmed.front() == STR('#'))
            {
                continue;
            }

            auto separator = trimmed.find(STR(':'));
            if (separator == RC::StringType::npos)
            {
                continue;
            }

            auto name = Trim(trimmed.substr(0, separator));
            auto value = Trim(trimmed.substr(separator + 1));
            if (name.empty())
            {
                continue;
            }

            if (strictValues && value != STR("0") && value != STR("1"))
            {
                PS::Log<RC::LogLevel::Warning>(
                    STR("Invalid mods.txt value for '{}': expected 0 or 1; treating it as disabled.\n"), name);
                entries.push_back({ name, false });
                continue;
            }

            bool enabled = value != STR("0");
            entries.push_back({ name, enabled });
        }

        return entries;
    }

    void ModLoadOrder::SavePreservingComments(
        const std::filesystem::path& modsTxtPath,
        const std::vector<ModOrderEntry>& entries)
    {
        std::vector<std::string> outputLines;

        std::ifstream input(modsTxtPath);
        std::string line;
        while (std::getline(input, line))
        {
            auto trimmed = Trim(PS::ToWideSafe(line.c_str()));
            auto separator = trimmed.find(STR(':'));
            if (trimmed.empty() || trimmed.front() == STR(';') || trimmed.front() == STR('#')
                || separator == RC::StringType::npos)
            {
                outputLines.push_back(line);
                continue;
            }

            auto name = Trim(trimmed.substr(0, separator));
            if (name.empty())
            {
                outputLines.push_back(line);
                continue;
            }

            // Entry lines are rewritten below in the requested order. Comments,
            // blank lines, and unrelated text retain their relative order.
        }

        for (const auto& entry : entries)
        {
            outputLines.push_back(NarrowFromWide(entry.Name) + " : " + (entry.Enabled ? "1" : "0"));
        }

        std::ofstream output(modsTxtPath, std::ios::trunc);
        if (!output.is_open())
        {
            PS::Log<RC::LogLevel::Error>(STR("Failed to write mod load order to mods.txt\n"));
            return;
        }
        for (const auto& outputLine : outputLines)
        {
            output << outputLine << '\n';
        }
    }

    void ModLoadOrder::Save(const std::filesystem::path& modsTxtPath, const std::vector<ModOrderEntry>& entries)
    {
        std::ofstream file(modsTxtPath, std::ios::trunc);
        if (!file.is_open())
        {
            PS::Log<RC::LogLevel::Error>(STR("Failed to write mod load order to mods.txt\n"));
            return;
        }

        file << "; RuneSchema mod load order - mods are loaded top to bottom.\n";
        file << "; Set the value to 0 to disable a mod without removing its folder.\n";

        for (const auto& entry : entries)
        {
            file << NarrowFromWide(entry.Name) << " : " << (entry.Enabled ? 1 : 0) << "\n";
        }
    }

    std::vector<ModOrderEntry> ModLoadOrder::ReadEntries(
        const std::filesystem::path& modsFolderPath,
        bool strictValues)
    {
        return Load(GetModsTxtPath(modsFolderPath), strictValues);
    }

    void ModLoadOrder::WriteEntries(
        const std::filesystem::path& modsFolderPath,
        const std::vector<ModOrderEntry>& entries,
        bool preserveComments)
    {
        auto modsTxtPath = GetModsTxtPath(modsFolderPath);
        if (preserveComments && fs::exists(modsTxtPath))
        {
            SavePreservingComments(modsTxtPath, entries);
        }
        else
        {
            Save(modsTxtPath, entries);
        }
    }

    std::vector<RC::StringType> ModLoadOrder::Resolve(
        const std::filesystem::path& modsFolderPath,
        const std::vector<RC::StringType>& discoveredModNames)
    {
        auto discoveredSorted = discoveredModNames;
        std::sort(discoveredSorted.begin(), discoveredSorted.end());

        auto* config = PS::PSConfig::Get();
        const auto& settings = config->GetModsTxtSettings();
        if (!config->IsToolingEnabled() || !settings.enabled)
        {
            return discoveredSorted;
        }

        auto modsTxtPath = GetModsTxtPath(modsFolderPath);
        auto fileExisted = fs::exists(modsTxtPath);

        if (!fileExisted && !settings.autoCreate)
        {
            return discoveredSorted;
        }

        auto entries = Load(modsTxtPath, settings.strictValues);

        std::unordered_set<RC::StringType> discovered(discoveredModNames.begin(), discoveredModNames.end());

        // Drop entries for folders that no longer exist on disk.
        auto countBeforeCleanup = entries.size();
        // Missing folders are always removed from the in-memory plan so a stale
        // line can never make loaders walk a nonexistent path. reconcileFolders
        // controls whether that cleanup is persisted back to mods.txt.
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&](const ModOrderEntry& entry) { return discovered.find(entry.Name) == discovered.end(); }),
            entries.end());
        bool droppedStaleEntries = entries.size() != countBeforeCleanup;

        // Append any mod folders mods.txt doesn't know about yet, enabled by
        // default, in the order they were discovered on disk.
        std::unordered_set<RC::StringType> known;
        known.reserve(entries.size());
        for (const auto& entry : entries)
        {
            known.insert(entry.Name);
        }

        bool appendedNewEntries = false;
        for (const auto& name : discoveredSorted)
        {
            if (known.insert(name).second)
            {
                entries.push_back({ name, true });
                appendedNewEntries = settings.reconcileFolders;
            }
        }

        if (!fileExisted || (settings.reconcileFolders && (droppedStaleEntries || appendedNewEntries)))
        {
            if (fileExisted && settings.preserveComments)
            {
                SavePreservingComments(modsTxtPath, entries);
            }
            else
            {
                Save(modsTxtPath, entries);
            }
        }

        std::vector<RC::StringType> orderedModNames;
        orderedModNames.reserve(entries.size());
        for (const auto& entry : entries)
        {
            if (!entry.Enabled)
            {
                PS::Log<RC::LogLevel::Normal>(STR("Skipping mod '{}' (disabled in mods.txt)\n"), entry.Name);
                continue;
            }

            orderedModNames.push_back(entry.Name);
        }

        return orderedModNames;
    }
}
