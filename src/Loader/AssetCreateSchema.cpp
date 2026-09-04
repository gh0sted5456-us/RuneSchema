#include "Loader/AssetCreateSchema.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace DragonWilds
{
    namespace
    {
        std::string SanitizePackageSegment(
            std::string_view value, std::string_view fallback)
        {
            std::string result;
            result.reserve(value.size());
            bool previousUnderscore = false;
            for (const auto character : value)
            {
                const auto byte = static_cast<unsigned char>(character);
                const bool accepted = std::isalnum(byte) || character == '_';
                const char normalized = accepted ? character : '_';
                if (normalized == '_' && previousUnderscore)
                    continue;
                result.push_back(normalized);
                previousUnderscore = normalized == '_';
            }
            while (!result.empty() && result.back() == '_') result.pop_back();
            while (!result.empty() && result.front() == '_') result.erase(result.begin());
            if (result.empty()) result.assign(fallback);
            return result;
        }
    }

    bool NormalizeAssetCreateDirective(nlohmann::json& properties)
    {
        const bool hasCreate = properties.contains("$Create");
        const bool hasClone = properties.contains("$Clone");
        if (properties.contains("$CloneFrom") || properties.contains("$clone"))
            throw std::runtime_error(
                "use the exact case-sensitive $Clone directive");
        if (hasCreate && hasClone)
            throw std::runtime_error("use either $Create or $Clone, not both");
        if (!hasCreate && !hasClone) return false;

        if (hasClone)
        {
            const auto& clone = properties.at("$Clone");
            if (!clone.is_string() || clone.get_ref<const std::string&>().empty())
                throw std::runtime_error(
                    "$Clone must be a non-empty cooked object path or item InternalName");
            return true;
        }

        const auto& create = properties.at("$Create");
        std::string classPath;
        if (create.is_string()) classPath = create.get<std::string>();
        else if (create.is_object())
        {
            for (const auto& [key, ignored] : create.items())
            {
                (void)ignored;
                if (key != "Class")
                    throw std::runtime_error(
                        "$Create supports only the Class field; reference cooked icons, meshes, appearance data, and other assets in the item fields");
            }
            classPath = create.value("Class", std::string{});
        }
        else
            throw std::runtime_error(
                "$Create must be a reflected class path string or an object containing Class");

        if (classPath.empty()
            || (!classPath.starts_with("/Script/")
                && classPath.front() != '/'))
            throw std::runtime_error(
                "$Create.Class must be a full reflected /Script/Module.Class path or mounted cooked class path");

        properties["$Create"] = nlohmann::json{{"Class", std::move(classPath)}};
        return true;
    }

    std::string BuildRuntimeItemObjectPath(
        const std::string& modName, const std::string& internalName)
    {
        const auto safeMod = SanitizePackageSegment(modName, "UnnamedMod");
        const auto safeItem = SanitizePackageSegment(internalName, "ITEM_RS_Unnamed");
        return "/Game/RuneSchema/" + safeMod + "/Items/" + safeItem
            + "." + safeItem;
    }
}

