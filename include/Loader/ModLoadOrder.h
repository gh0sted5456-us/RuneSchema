#pragma once

#include <filesystem>
#include <vector>
#include "Unreal/NameTypes.hpp"

namespace DragonWilds {
    // One line of a RuneSchema `mods.txt` load-order file. Mirrors the format UE4SS
    // itself uses for its own Mods/mods.txt: `<ModName> : <1|0>`, where 1 means the
    // mod is enabled and 0 means it is skipped entirely without removing its folder.
    struct ModOrderEntry {
        RC::StringType Name;
        bool Enabled = true;
    };

    // Reads (or creates) `mods.txt` inside a RuneSchema `mods` folder, reconciles it
    // against the mod folders that actually exist on disk - folders discovered on
    // disk but missing from the file are appended as enabled, in discovery order;
    // entries whose folder no longer exists are dropped - writes the reconciled file
    // back out when anything changed, and returns the folder names that should be
    // loaded, in load order, with disabled entries already filtered out.
    class ModLoadOrder {
    public:
        static std::vector<RC::StringType> Resolve(
            const std::filesystem::path& modsFolderPath,
            const std::vector<RC::StringType>& discoveredModNames);

        static std::vector<ModOrderEntry> ReadEntries(
            const std::filesystem::path& modsFolderPath,
            bool strictValues);

        static void WriteEntries(
            const std::filesystem::path& modsFolderPath,
            const std::vector<ModOrderEntry>& entries,
            bool preserveComments);

    private:
        static std::filesystem::path GetModsTxtPath(const std::filesystem::path& modsFolderPath);

        static std::vector<ModOrderEntry> Load(
            const std::filesystem::path& modsTxtPath,
            bool strictValues);

        static void Save(const std::filesystem::path& modsTxtPath, const std::vector<ModOrderEntry>& entries);

        static void SavePreservingComments(
            const std::filesystem::path& modsTxtPath,
            const std::vector<ModOrderEntry>& entries);
    };
}
