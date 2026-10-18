#pragma once
#include "Core/SpawnCatalogSearch.h"
namespace PS::SpawnPresentation {
inline bool Enemy(const nlohmann::json& row){return SpawnCatalogSearch::Text(row,"Type")=="AI"&&row.value("SpawnableCharacter",false)&&row.value("Available",false)&&!row.value("NeedsInspection",false);}
}
