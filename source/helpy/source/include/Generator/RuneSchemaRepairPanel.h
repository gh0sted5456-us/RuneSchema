#pragma once
#include "Runtime/HelpyModCacheBuilder.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyEnvironment.h"
#include "Generator/HelpyImGuiTheme.h"
namespace PS::RuneSchemaRepairPanel {
inline void Render(){
    const auto& job=HelpyModCacheBuilder::Current();
    ImGui::TextWrapped("Repair RuneSchema's required base files and four bundled plugins. Close the assigned game or server first. Mods, settings, saves, caches and third-party plugins are preserved.");
    ImGui::TextWrapped("Downloads the published package matching the installed version and verifies its SHA-256 checksum. Recent RuneSchema and UE4SS log warnings are shown as context. Previous files are backed up before replacement.");
    ImGui::BeginDisabled(job.Running);
    if(HelpyImGuiTheme::SoftButton("Repair",{HelpyImGuiTheme::Current.X(160),0}))HelpyModCacheBuilder::StartRepair(HostServices::ModDirectory(),HelpyEnvironment::RuneSchemaVersion);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RepairButton=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    if(job.Repair){
        ImGui::TextWrapped("%s: %s",job.State.c_str(),job.Stage.c_str());ImGui::ProgressBar(job.Percent/100.f,{-1,0});
        if(!job.Job.empty()&&HelpyImGuiTheme::SoftButton("Open repair report & backups"))ShellExecuteW(nullptr,L"open",job.Job.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    }else if(job.Running)ImGui::TextWrapped("Finish the active authoring job before starting a repair.");
    ImGui::SeparatorText("Console");
    if(ImGui::BeginChild("##repair-console",{0,0},true,ImGuiWindowFlags_HorizontalScrollbar)){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::RepairConsole=true;
#endif
        const bool following=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-4;
        const auto* text=job.Repair&&!job.Console.empty()?job.Console.c_str():"Ready. Press Repair to read recent logs and scan the base files.";
        ImGui::TextUnformatted(text);if(following)ImGui::SetScrollHereY(1.f);
    }ImGui::EndChild();
}
}
