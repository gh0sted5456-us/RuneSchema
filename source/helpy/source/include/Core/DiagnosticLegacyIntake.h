#pragma once
#include "Core/LiveLog.h"
#include "Core/StructuredDiagnosticLegacyMirror.h"

namespace PS::DiagnosticLegacyIntake {
inline std::string MirrorReference(const LiveLog::Entry& entry) {
    if(entry.Text.find("[RuneSchema] [error] [RS-")==std::string::npos&&
       entry.Text.find("[RuneSchema] [warning] [RS-")==std::string::npos)return {};
    return StructuredDiagnosticLegacyMirror::EventReference(entry.Text);
}
inline bool Eligible(const LiveLog::Entry& entry) noexcept {
    return entry.Severity==LiveLog::Level::Error ||
           entry.Severity==LiveLog::Level::Warning;
}
inline bool HasEmittedCode(const LiveLog::Entry& entry) noexcept {
    return !entry.ReportedCode.empty() && LiveLog::CodeToken(entry.ReportedCode);
}
inline std::string SuggestedReference(const LiveLog::Entry& entry) {
    if(HasEmittedCode(entry)||!entry.Suggested||entry.Help<0||
       entry.Help>=static_cast<int>(LiveLog::Guides.size()))return {};
    const auto code=LiveLog::Guides[entry.Help].Code;
    if(code=="RS-LOG-000"||code=="RS-GAME-000")return {};
    return std::string(code);
}
}
