#pragma once
#include <string>

namespace PS::JsonSchemaGenerator {
    std::string GenerateSchemaFiles(bool includeLoadedTables = false, const std::string& exportName = {});
}
