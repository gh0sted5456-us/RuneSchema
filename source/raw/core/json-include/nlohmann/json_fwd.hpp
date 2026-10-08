#pragma once
// Standalone verification compatibility wrapper. The production build uses
// the package-provided nlohmann/json_fwd.hpp; this pinned test tree carries
// the single-header distribution.
#include "json.hpp"
