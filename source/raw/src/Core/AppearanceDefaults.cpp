#include "Core/AppearanceDefaults.h"

#include <array>
#include <stdexcept>
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"

namespace PS::AppearanceDefaults {
namespace {
using Json = nlohmann::json;

constexpr std::array<const char*, 8> RequiredFields{
    "BodyType", "Head", "HairPreset", "FacialHairPreset",
    "SkinTone", "HairColor", "EyeColor", "EyebrowColor"
};

Json Handle(const char* table, const char* row)
{
    return {{"dataTable", table}, {"rowName", row}};
}

const Json& Fields(const Json& document)
{
    const auto& customization = document.at("Customization");
    if (!customization.is_object())
        throw std::runtime_error("Customization must be an object");
    const auto& fields = customization.at("CustomizationData");
    if (!fields.is_object())
        throw std::runtime_error("Customization.CustomizationData must be an object");
    return fields;
}

void Validate(const Json& document)
{
    const auto& fields = Fields(document);
    if (fields.size() != RequiredFields.size())
        throw std::runtime_error("the override must contain exactly eight canonical appearance fields");
    for (const auto* field : RequiredFields) {
        const auto& value = fields.at(field);
        if (!value.is_object() || value.size() != 2
            || !value.contains("dataTable") || !value.at("dataTable").is_string()
            || !value.contains("rowName") || !value.at("rowName").is_string()
            || value.at("dataTable").get_ref<const std::string&>().empty()
            || value.at("rowName").get_ref<const std::string&>().empty())
            throw std::runtime_error(std::string("invalid appearance field: ") + field);
    }
}
}

const nlohmann::json& BuiltIn()
{
    static const Json document = {
        {"meta_data", Json::object()},
        {"Customization", {{"CustomizationData", {
            {"BodyType", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_BodyType.DT_Customization_BodyType", "male_A_01")},
            {"Head", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_FaceType.DT_Customization_FaceType", "male_A_01")},
            {"HairPreset", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets", "Preset_None")},
            {"FacialHairPreset", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_FacialHairPresets.DT_Customization_FacialHairPresets", "M_A_PresetNone")},
            {"SkinTone", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_SkinTone.DT_Customization_SkinTone", "SkinTone1")},
            {"HairColor", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_HairColor.DT_Customization_HairColor", "Color1")},
            {"EyeColor", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_EyeColor.DT_Customization_EyeColor", "Color1")},
            {"EyebrowColor", Handle("/Game/Gameplay/Character/Player/Customization/DT_Customization_EyebrowColor.DT_Customization_EyebrowColor", "Color1")}
        }}}}
    };
    return document;
}

std::filesystem::path OverridePath()
{
    return HostServices::SettingsDirectory() / "defaults" / "Default.json";
}

Selection Load(bool allowExternal)
{
    if (!allowExternal) return {BuiltIn(), false, {}};
    const auto path = OverridePath();
    if (!std::filesystem::is_regular_file(path)) return {BuiltIn(), false,
        "settings/defaults/Default.json was not found"};
    try {
        auto document = Json::parse(ConfigFiles::Read(path, 256 * 1024));
        if (!document.contains("meta_data")) document["meta_data"] = Json::object();
        Validate(document);
        return {std::move(document), true, {}};
    } catch (const std::exception& error) {
        return {BuiltIn(), false,
            "settings/defaults/Default.json was ignored: " + std::string(error.what())};
    }
}

bool ReadField(const nlohmann::json& document, const std::string& field,
    std::string& dataTablePath, std::string& rowName, std::string& error)
{
    try {
        const auto& value = Fields(document).at(field);
        dataTablePath = value.at("dataTable").get<std::string>();
        rowName = value.at("rowName").get<std::string>();
        if (dataTablePath.empty() || rowName.empty())
            throw std::runtime_error("appearance field was empty");
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
}
