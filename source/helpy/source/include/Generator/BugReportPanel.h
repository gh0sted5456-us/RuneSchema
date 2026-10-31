#pragma once
#include "Core/BugReport.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/HelpyPreviewHooks.h"
#include "Generator/HelpyImGuiTheme.h"
#include <shellapi.h>

namespace PS::BugReportPanel {
inline char Title[161]{},Details[2049]{},Steps[2049]{},Expected[1025]{},Additional[2049]{};
inline int Category=0;
inline bool IncludeEnvironment=true;
inline std::string Status;
inline void* NexusTexture=nullptr;inline void* NexusAccent=nullptr;
#if defined(HELPY_PREVIEW)
inline std::string OpenedUrl,CopiedBody;
inline bool ScrollToSubmit=false;
#endif
inline void Copy(const std::string& text){
#if defined(HELPY_PREVIEW)
    CopiedBody=text;
#else
    ImGui::SetClipboardText(text.c_str());
#endif
}
inline void Render(){
    const auto& theme=HelpyImGuiTheme::Current;
    ImGui::TextWrapped("Fill out the form, choose Copy bug report, then Submit to Nexus Page. Sign in with a Nexus account, create a bug report and paste the copied text. Helpy opens the page; you submit it there.");
    ImGui::TextWrapped("Do not include passwords, account IDs, personal information or private save data. No logs or files are attached automatically.");
    ImGui::Spacing();ImGui::TextUnformatted("Title *");ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##bug-title","Short summary of the problem",Title,sizeof(Title));
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BugFields["Title"]=HelpyPreviewHooks::LastRect();
#endif
    ImGui::TextUnformatted("Category");ImGui::SetNextItemWidth(-1);ImGui::Combo("##bug-category",&Category,BugReport::Categories.data(),int(BugReport::Categories.size()));
    const auto field=[&](const char* label,const char* id,char* value,size_t size,float height){ImGui::TextUnformatted(label);ImGui::InputTextMultiline(id,value,size,{-1,theme.Y(height)});
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BugFields[label]=HelpyPreviewHooks::LastRect();
#endif
    };
    field("What happened? *","##bug-details",Details,sizeof(Details),90);
    field("Steps to reproduce","##bug-steps",Steps,sizeof(Steps),80);
    field("Expected behavior","##bug-expected",Expected,sizeof(Expected),60);
    field("Additional details / log excerpt (optional)","##bug-additional",Additional,sizeof(Additional),80);
    ImGui::Checkbox("Include platform and version details (review below)",&IncludeEnvironment);
    std::vector<std::pair<std::string,std::string>> environment;
    if(IncludeEnvironment){
        const auto build=HelpyEnvironment::UE4SS.find("Build");
        environment={{"Platform",HelpyEnvironment::Label()},{"RuneSchema",HelpyEnvironment::RuneSchemaVersion},{"Helpy",HelpyBranding::Version},{"Game build",HelpyEnvironment::GameVersion},{"UE4SS",build!=HelpyEnvironment::UE4SS.end()&&build->is_string()?build->get<std::string>():std::string{}}};
    }
    BugReport::Draft draft{Title,Details,Steps,Expected,Additional,static_cast<size_t>(Category)};std::string body,error;
    try{body=BugReport::Body(draft,environment);}catch(const std::exception& e){error=e.what();}
    if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
    ImGui::BeginDisabled(!error.empty());
#if defined(HELPY_PREVIEW)
    if(ScrollToSubmit)ImGui::SetScrollHereY(.85f);
#endif
    if(HelpyImGuiTheme::SoftButton("Copy bug report")){Copy(body);Status="Plain-text report copied. Open the Nexus bugs page, sign in, and paste it into a new bug report.";}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BugCopy=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::GetContentRegionAvail().x>theme.X(270))ImGui::SameLine();
    if(NexusTexture){
        const float size=ImGui::GetFrameHeight();const auto at=ImGui::GetCursorScreenPos();ImGui::Image(ImTextureRef(NexusTexture),{size,size});
        if(NexusAccent)ImGui::GetWindowDrawList()->AddImage(ImTextureRef(NexusAccent),at,{at.x+size,at.y+size},{0,0},{1,1},ImGui::GetColorU32(theme.accent));ImGui::SameLine();
    }
    if(HelpyImGuiTheme::SoftButton("Submit to Nexus Page")){
#if defined(HELPY_PREVIEW)
        OpenedUrl=BugReport::NexusPage;const bool opened=true;
#else
        const auto result=ShellExecuteA(nullptr,"open",BugReport::NexusPage,nullptr,nullptr,SW_SHOWNORMAL);const bool opened=reinterpret_cast<INT_PTR>(result)>32;
#endif
        Status=opened?"Nexus bugs page opened. Sign in, create a bug report and paste your copied report. Helpy has not submitted it.":"Could not open the browser. Copy the report and visit RuneSchema's Nexus bugs page manually.";
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BugSubmit=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(!body.empty()&&ImGui::CollapsingHeader("Preview the exact report"))ImGui::TextWrapped("%s",body.c_str());
}
}
