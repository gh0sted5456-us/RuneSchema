#pragma once
#include "Generator/ToolRequest.h"
#include "Generator/HelpyImGuiTheme.h"
namespace PS::CatalogProgressView {
inline void Render(const nlohmann::json& snapshot) {
    if(!snapshot.value("_CatalogIndexing",false))return;
    const auto done=snapshot.value("IndexDone",size_t{}),total=snapshot.value("IndexTotal",size_t{});
    const bool known=snapshot.value("IndexHasTotal",false)&&total>0;
    const auto label=snapshot.value("IndexStage",std::string("Discovering catalogue"));
    const float fraction=known?std::min(1.0f,float(done)/float(total)):-static_cast<float>(ImGui::GetTime());
    ImGui::ProgressBar(fraction,{-1,0},label.c_str());
    if(known)ImGui::Text("%zu / %zu entries",std::min(done,total),total);else ImGui::Text("%zu entries discovered; total not known yet",done);
    const auto detail=snapshot.value("IndexDetail",std::string{});if(!detail.empty())ImGui::TextWrapped("%s",detail.c_str());
    ImGui::BeginDisabled(SpawnToolRequests::IsWaiting());
    if(ImGui::SmallButton("Cancel full scan"))SpawnToolRequests::TrySubmit({{"Action","CancelCatalogIndex"},{"_Source","HelpyCatalogProgress"}});
    ImGui::EndDisabled();
}
}
