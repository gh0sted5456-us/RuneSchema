#pragma once
#include "Core/JsonDocument.h"

namespace RC::Unreal {
    struct FRotator;
    struct FVector;
    class FName;
}

namespace PS::JsonHelpers {
    void ParseRotator(const nlohmann::json& value, const std::string& fieldName, RC::Unreal::FRotator& outValue);
    void ParseVector(const nlohmann::json& value, const std::string& fieldName, RC::Unreal::FVector& outValue);
    void ParseFName(const nlohmann::json& value, const std::string& fieldName, RC::Unreal::FName& outValue);
}
