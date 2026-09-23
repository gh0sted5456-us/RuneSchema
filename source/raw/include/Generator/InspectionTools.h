#pragma once
namespace PS::InspectionTools {
    enum class Section { Inspector, Presets, Traces, Results, SavedItems, SavedWorld, AssetTemplates, Niagara };
    void Render(bool available, Section section, const char* loader = nullptr);
    // Focused create-new editor for the two related runtime contracts. Drafts
    // export to runtime/live/jobs/exports and do not install or activate mod files.
    void RenderPlayerNameplateBuilder(bool nameplateDefinition);
    void Tick();
    // Renders the shared bottom workflow history for the currently visible tool family.
    void RenderHistory();
    // Game-thread cleanup; preserves authoring controls and exported files.
    void Reset();
}
