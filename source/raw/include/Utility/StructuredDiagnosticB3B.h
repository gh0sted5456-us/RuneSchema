#pragma once
// Source-grounded reports for building and spawn definitions; no UObject lifetime dependency.
#include "Utility/StructuredDiagnosticB3.h"
#include <string>
#include <utility>

namespace PS::StructuredDiagnosticB3B {
inline bool Report(std::string code,std::string severity,std::string outcome,
        std::string source,std::string title,std::string message,
        std::string mod,std::string file={},std::string target={},
        std::string phase="Validation",std::string property={}) noexcept {
    StructuredDiagnosticB2::Provenance provenance;
    provenance.ModId=std::move(mod);
    provenance.File=std::move(file);
    provenance.Target=std::move(target);
    return StructuredDiagnosticB3::Report(std::move(code),std::move(severity),std::move(outcome),
        std::move(source),std::move(title),std::move(message),
        std::move(provenance),std::move(phase),std::move(property));
}
}
