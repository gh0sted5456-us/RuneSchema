#pragma once
// Reporting glue for loader sites. Never resolves or retains live UObject pointers.
#include "Utility/StructuredDiagnosticB2.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace PS::StructuredDiagnosticB3 {
inline StructuredDiagnosticB2::Provenance FromPath(const std::filesystem::path& original,
        std::string_view expectedSection={}) {
    const auto normalized=original.lexically_normal();
    StructuredDiagnosticB2::Provenance result;
    bool previousWasMods=false,foundMod=false;
    std::filesystem::path relative;
    for(const auto& part:normalized){
        const auto segment=StructuredDiagnosticB2::Utf8Path(part);
        if(previousWasMods){result.ModId=segment;foundMod=true;previousWasMods=false;continue;}
        if(foundMod){relative/=part;continue;}
        previousWasMods=StructuredDiagnosticB2::SameAsciiDirectory(segment,"mods");
    }
    if(foundMod && !relative.empty())result.File=StructuredDiagnosticB2::Utf8Path(relative);
    else if(!expectedSection.empty())result.File=StructuredDiagnosticB2::SourceWithinSection(normalized,expectedSection);
    else result.File=StructuredDiagnosticB2::Utf8Path(normalized.filename());
    return result;
}
inline bool Report(std::string code,std::string severity,std::string outcome,
        std::string source,std::string title,std::string message,
        StructuredDiagnosticB2::Provenance provenance,std::string phase,
        std::string property={}) noexcept {
    return StructuredDiagnosticB2::Report(std::move(code),std::move(severity),std::move(outcome),
        std::move(source),std::move(title),std::move(message),provenance,std::move(phase),std::move(property));
}
}
