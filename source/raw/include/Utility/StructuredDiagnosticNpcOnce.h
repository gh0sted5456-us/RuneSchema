#pragma once
#include <string>
#include <utility>

namespace PS::StructuredDiagnosticNpcOnce {
template<class KeySet,class StructuredSink,class LegacySink>
bool ReportOnce(KeySet& seen, const std::string& key, bool preserveLegacy,
                StructuredSink&& structured, LegacySink&& legacy, bool structuredMirrorsLegacy=false) {
    if (!seen.insert(key).second) return false;
    bool accepted = false;
    try { accepted = static_cast<bool>(std::forward<StructuredSink>(structured)()); }
    catch (...) { accepted = false; }
    if (!accepted || (preserveLegacy && !structuredMirrorsLegacy)) std::forward<LegacySink>(legacy)();
    return true;
}
}
