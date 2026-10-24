#pragma once
#include "Utility/StructuredDiagnosticB3C.h"
#include <string>
#include <utility>

namespace PS::StructuredDiagnosticB3D {
inline bool Report(std::string code, std::string severity, std::string outcome,
    std::string title, std::string message, std::string target={},
    std::string phase="Application", std::string mod={}, std::string file={}) noexcept {
    return StructuredDiagnosticB3C::Report(std::move(code),std::move(severity),std::move(outcome),
        "LoaderCore",std::move(title),std::move(message),
        std::move(mod),std::move(file),std::move(target),std::move(phase));
}
}
