#pragma once
#include "Core/BugReport.h"
#include "Core/HelpyPreviewHooks.h"
#include "Generator/HelpyImGuiTheme.h"
#include <shellapi.h>

namespace PS::BugReportPanel {
inline std::string Status;
#if defined(HELPY_PREVIEW)
inline std::string OpenedUrl;
#endif
inline bool Open(){
#if defined(HELPY_PREVIEW)
    OpenedUrl=BugReport::NexusPage;const bool opened=true;
#else
    const bool opened=reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr,"open",BugReport::NexusPage,nullptr,nullptr,SW_SHOWNORMAL))>32;
#endif
    Status=opened?"Nexus bugs page opened.":"Could not open the browser. Visit the Nexus bugs page below.";return opened;
}
inline void Render(){
    ImGui::TextWrapped("Report issues directly on RuneSchema's Nexus Bugs page. A Nexus account is required.");
    if(HelpyImGuiTheme::SoftButton("Open Nexus Bugs"))Open();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BugSubmit=HelpyPreviewHooks::LastRect();
#endif
    ImGui::TextWrapped("%s",BugReport::NexusPage);
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
}
}
