#include "Core/AppearanceDefaults.h"

#include <stdexcept>

namespace PS::AppearanceDefaults {
namespace {
using Json = nlohmann::json;

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
