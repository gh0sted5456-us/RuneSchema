#pragma once
#include <nlohmann/json.hpp>
namespace RC::Unreal { class UObject; }
namespace PS::SaveViewer {
    nlohmann::json Capture(RC::Unreal::UObject* controller, bool world);
    void RenderCleanup();
    void RenderNpcExport();
    void PumpNpcExport();
}
