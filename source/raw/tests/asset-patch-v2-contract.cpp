#include "nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error(std::string("Unable to read ") + path);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Expected schema, hair example, beard example, and loader source paths");
    const auto schema=nlohmann::json::parse(Read(argv[1]));
    const auto example=nlohmann::json::parse(Read(argv[2]));
    const auto beard=nlohmann::json::parse(Read(argv[3]));
    const auto source=Read(argv[4]);
    for(const auto* token:{"IsDirectDataAssetPatch", "DirectDataAssetOperations",
            "target.ends_with(\"_C\")", "DataHandle.RowName", "TemplateIndex",
            "$MergeWhere", "SelectorPath", "SelectorValues",
            "$MergeWhere selector must match exactly one array entry"})
        if(source.find(token)==std::string::npos)
            throw std::runtime_error(std::string("Direct DA_ field patch support is missing: ")+token);
    for(const auto* token:{"BodyTypeCompatability",
            "FaceTypeCompatibility", "EyeTypeCompatibility", "value[field] = -1"})
        if(source.find(token)==std::string::npos)
            throw std::runtime_error(std::string("Character customization normalization is missing: ")+token);
    for(const auto* token:{"AppendUnique post-commit identity verification failed",
            "[CHARACTER-OPTIONS][VERIFIED]", "[CHARACTER-OPTIONS][PROPAGATED]",
            "ValidateCharacterOptionHandle", "character option DataHandle row is unavailable",
            "CharacterMenuAssetPatchReplay", "ApplyObjectPatches(true)"})
        if(source.find(token)==std::string::npos)
            throw std::runtime_error(std::string("Character option refresh contract is missing: ")+token);
    if(schema.dump().find("runeschema.dev")!=std::string::npos
        || schema.dump().find("modId")!=std::string::npos)
        throw std::runtime_error("Authoring schema must not require a web identity or duplicated mod ID");
    if(example.contains("$schema") || example.contains("schema") || example.contains("modId"))
        throw std::runtime_error("Hair example must be identified by its DA_ target and owning mod folder");
    const auto target="/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C";
    const auto& body=example.at(target);
    const auto path="CharacterOptionData[ECharacterOptionType::HairPreset].OptionData";
    const auto& operation=body.at(path);
    if(!operation.contains("$AppendUnique")
        || operation.at("$AppendUnique").at(0).at("DataHandle").at("RowName")!="RS_ExampleHair")
        throw std::runtime_error("Hair example lost its stable unique append contract");
    if(beard.contains("$schema") || beard.contains("schema") || beard.contains("modId"))
        throw std::runtime_error("Beard example contains redundant envelope identity");
    const auto& grouped=beard.at(target).at("CharacterOptions[FacialHairPreset].OptionData").at("$MergeWhere");
    if(grouped.at("Field")!="DataHandle.RowName" || grouped.at("Values").size()!=40
        || grouped.at("Value").at("BodyTypeCompatability")!="both"
        || grouped.at("Value").at("FaceTypeCompatibility")!="all")
        throw std::runtime_error("Beard example lost its grouped forty-row merge contract");
    for(const auto* token:{"QueueObjectPatch", "ApplyObjectPatches", "ClassDefaultObject",
            "RuneSchema.AssetPatch.v2", "RegistryPatch::Schema", "AppendUnique"})
        if(source.find(token)==std::string::npos)
            throw std::runtime_error(std::string("Asset loader contract is missing: ")+token);
}
