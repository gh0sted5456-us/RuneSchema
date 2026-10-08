#pragma once
#include "Core/PatchConflicts.h"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"

namespace DragonWilds {
    inline void WarnPatchConflicts(PatchConflicts& tracker, const std::string& record,
        const nlohmann::json& changes, const std::string& source, bool keyedArrays = true) {
        for (const auto& conflict : tracker.Record(record, changes, source, keyedArrays)) {
            PS::Log<RC::LogLevel::Warning>(STR("$Patch conflict: record '{}', field '{}': {} overrides {}. LAST PATCH WINS in load order; review both patches.\n"),
                RC::to_generic_string(record), RC::to_generic_string(conflict.Field),
                RC::to_generic_string(conflict.Later), RC::to_generic_string(conflict.Earlier));
        }
    }
}
