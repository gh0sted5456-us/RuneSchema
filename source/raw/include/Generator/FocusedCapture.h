#pragma once
#include <nlohmann/json.hpp>
#include "Generator/DiagnosticPreset.h"
#include <unordered_set>
namespace RC::Unreal { class UObject; }
namespace PS::InspectionTools {
struct CaptureBudget {
    unsigned remaining = 2048;
    unsigned maxDepth = DefaultCaptureDepth;
    unsigned maxEntries = 64;
    unsigned maxSparseSlots = 4096;
    bool followReferences = false;
    std::unordered_set<RC::Unreal::UObject*> visited;
};
nlohmann::json CaptureProperty(RC::Unreal::UObject* root, const nlohmann::json& path, CaptureBudget& budget);
}
