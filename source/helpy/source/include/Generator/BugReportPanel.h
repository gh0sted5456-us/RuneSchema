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
    ImGui::TextWrapped("Write a report, then review and submit it on GitHub. A GitHub account is required. Nothing is posted automatically.");
    ImGui::TextColored(theme.warning,"GitHub issues are public.");
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
    BugReport::Draft draft{Title,Details,Steps,Expected,Additional,static_cast<size_t>(Category)};std::string body,error;BugReport::Submission submission;
    try{body=BugReport::Body(draft,environment);submission=BugReport::Prepare(draft,body);}catch(const std::exception& e){error=e.what();}
    if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
    ImGui::BeginDisabled(!error.empty());
#if defined(HELPY_PREVIEW)
    if(ScrollToSubmit)ImGui::SetScrollHereY(.85f);
#endif
    if(HelpyImGuiTheme::SoftButton(submission.CopyBody?"Copy report & open GitHub":"Review & submit on GitHub")){
        if(submission.CopyBody)Copy(body);
#if defined(HELPY_PREVIEW)
        OpenedUrl=submission.Url;const bool opened=true;
#else
        const auto result=ShellExecuteA(nullptr,"open",submission.Url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);const bool opened=reinterpret_cast<INT_PTR>(result)>32;
#endif
        Status=opened?(submission.CopyBody?"Report copied. Paste it into GitHub's issue body, review, then click Submit new issue.":"GitHub opened with your draft. Review it and click Submit new issue there; Helpy has not submitted it."):"The browser could not be opened. Nothing was submitted; use Copy report and open GitHub manually.";
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BugSubmit=HelpyPreviewHooks::LastRect();
#endif
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Copy report")){Copy(body);Status="Report copied. Nothing was submitted.";}ImGui::EndDisabled();
    if(submission.CopyBody)ImGui::TextWrapped("This report is too long for a reliable prefilled link. The button copies the complete report without truncation; paste it on GitHub.");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(!body.empty()&&ImGui::CollapsingHeader("Preview the exact report"))ImGui::TextWrapped("%s",body.c_str());
}
}
