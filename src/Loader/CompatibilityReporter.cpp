#include "Loader/CompatibilityReporter.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "Helpers/String.hpp"
#include "nlohmann/json.hpp"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/ObjectPath.h"

namespace fs = std::filesystem;

namespace {
    struct WriteSite {
        RC::StringType ModName;
        fs::path File;
        bool ReplacesArray = false;
    };

    std::string Narrow(const RC::StringType& value)
    {
        std::string result;
        result.reserve(value.size());
        for (auto character : value)
        {
            result.push_back(character < 0x80 ? static_cast<char>(character) : '?');
        }
        return result;
    }

    std::string SiteText(const WriteSite& site)
    {
        return Narrow(site.ModName) + " (" + site.File.generic_string() + ")";
    }
}

namespace DragonWilds {
    void CompatibilityReporter::Generate(
        const std::filesystem::path& modsFolderPath,
        const std::vector<RC::StringType>& orderedModNames)
    {
        auto* config = PS::PSConfig::Get();
        const auto& settings = config->GetCompatibilityReportSettings();
        if (!config->IsToolingEnabled() || !settings.enabled)
        {
            return;
        }

        std::map<std::string, std::vector<WriteSite>> targetWrites;
        std::map<std::string, std::vector<WriteSite>> propertyWrites;

        for (const auto& modName : orderedModNames)
        {
            auto modPath = modsFolderPath / modName;
            for (const auto& loaderEntry : fs::directory_iterator(modPath))
            {
                if (!loaderEntry.is_directory())
                {
                    continue;
                }

                auto loaderName = loaderEntry.path().filename().string();
                if (loaderName == "config" || loaderName == "paks")
                {
                    continue;
                }

                for (const auto& fileEntry : fs::directory_iterator(loaderEntry.path()))
                {
                    auto extension = fileEntry.path().extension().string();
                    if (!fileEntry.is_regular_file() || (extension != ".json" && extension != ".jsonc"))
                    {
                        continue;
                    }

                    try
                    {
                        std::ifstream input(fileEntry.path());
                        auto data = nlohmann::json::parse(input, nullptr, true, true);
                        if (!data.is_object())
                        {
                            continue;
                        }

                        for (const auto& [rawTarget, properties] : data.items())
                        {
                            if (rawTarget.starts_with("$") || !properties.is_object())
                            {
                                continue;
                            }

                            auto normalizedTarget = rawTarget;
                            if (loaderName == "assets" && rawTarget.starts_with("/"))
                            {
                                normalizedTarget = Narrow(PS::ObjectPath::Normalize(
                                    RC::to_generic_string(rawTarget)));
                            }

                            auto relativeFile = fs::relative(fileEntry.path(), modsFolderPath);
                            auto targetKey = loaderName + "|" + normalizedTarget;
                            targetWrites[targetKey].push_back({ modName, relativeFile, false });

                            for (const auto& [propertyName, propertyValue] : properties.items())
                            {
                                auto propertyKey = targetKey + "|" + propertyName;
                                propertyWrites[propertyKey].push_back({
                                    modName,
                                    relativeFile,
                                    propertyValue.is_array()
                                });
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        PS::Log<RC::LogLevel::Warning>(STR("Compatibility report skipped '{}': {}\n"),
                            fileEntry.path().native(), PS::ToWideSafe(e.what()));
                    }
                }
            }
        }

        std::ostringstream report;
        report << "RuneSchema compatibility report\n";
        report << "Load order: ";
        for (std::size_t index = 0; index < orderedModNames.size(); ++index)
        {
            if (index > 0) report << " -> ";
            report << Narrow(orderedModNames[index]);
        }
        report << "\n\n";

        std::size_t warningCount = 0;
        auto differentMods = [](const std::vector<WriteSite>& sites) {
            std::set<RC::StringType> mods;
            for (const auto& site : sites) mods.insert(site.ModName);
            return mods.size() > 1;
        };

        if (settings.warnSameTarget)
        {
            for (const auto& [key, sites] : targetWrites)
            {
                if (!differentMods(sites)) continue;
                ++warningCount;
                report << "[TARGET] " << key << '\n';
                for (const auto& site : sites) report << "  - " << SiteText(site) << '\n';
            }
        }

        if (settings.warnSameProperty || settings.warnArrayReplacement)
        {
            for (const auto& [key, sites] : propertyWrites)
            {
                if (!differentMods(sites)) continue;
                auto arrayReplacement = false;
                for (const auto& site : sites) arrayReplacement = arrayReplacement || site.ReplacesArray;
                if ((!arrayReplacement && !settings.warnSameProperty)
                    || (arrayReplacement && !settings.warnSameProperty && !settings.warnArrayReplacement))
                {
                    continue;
                }

                ++warningCount;
                report << (arrayReplacement && settings.warnArrayReplacement ? "[ARRAY] " : "[PROPERTY] ") << key << '\n';
                for (const auto& site : sites) report << "  - " << SiteText(site) << '\n';
            }
        }

        if (warningCount == 0)
        {
            report << "No cross-mod target or property conflicts found.\n";
        }

        if (settings.writeFile)
        {
            auto configPath = modsFolderPath.parent_path() / "config";
            fs::create_directories(configPath);
            std::ofstream output(configPath / "compatibility_report.txt", std::ios::trunc);
            output << report.str();
        }

        PS::Log<RC::LogLevel::Normal>(STR("RuneSchema compatibility report completed with {} warning(s).\n"), warningCount);
    }
}
