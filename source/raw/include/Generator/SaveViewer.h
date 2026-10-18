#pragma once
#include <nlohmann/json.hpp>
namespace RC::Unreal { class UObject; }
namespace PS::SaveViewer {
    nlohmann::json Capture(RC::Unreal::UObject* controller, bool world);
    void RenderCleanup();
    void RenderNpcExport();
    void PumpNpcExport();
    // Game-thread-only authoring facade. Returns owned JSON, never engine objects.
    nlohmann::json Authoring(const std::string& operation,const nlohmann::json& arguments);
    nlohmann::json SaveEditorOperation(const std::string& operation,const nlohmann::json& arguments);
    void ResetAuthoring();
}
