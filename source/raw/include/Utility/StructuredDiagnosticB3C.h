#pragma once
// B3C catalog entry point: bounded, side-effect-free failure reporting.
// No UObject persistence; original loader failure semantics are preserved.
#include "Utility/StructuredDiagnosticB3B.h"
#include <string>
#include <string_view>

namespace PS::StructuredDiagnosticB3C {
inline bool Report(std::string code, std::string severity, std::string outcome,
    std::string source, std::string title, std::string message,
    std::string mod={}, std::string file={}, std::string target={},
    std::string phase="Validation", std::string property={}) noexcept {
    return StructuredDiagnosticB3B::Report(std::move(code),std::move(severity),std::move(outcome),
        std::move(source),std::move(title),std::move(message),
        std::move(mod),std::move(file),std::move(target),std::move(phase),std::move(property));
}
}
