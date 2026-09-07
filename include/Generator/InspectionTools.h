#pragma once
namespace PS::InspectionTools {
    enum class Section { Inspector, Presets, Traces, Results };
    void Render(bool available, Section section);
    void Tick();
}
