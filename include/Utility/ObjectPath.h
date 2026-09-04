#pragma once

#include "Unreal/NameTypes.hpp"

namespace PS::ObjectPath {
    // Normalizes Unreal/FModel object paths. Numeric FModel export suffixes such
    // as `.0` are treated as the package's asset name (or objectName when given).
    RC::StringType Normalize(
        const RC::StringType& path,
        const RC::StringType& objectName = {},
        bool classReference = false);
}
