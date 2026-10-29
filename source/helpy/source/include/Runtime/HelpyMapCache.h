#pragma once
#include "Core/MapFilters.h"
#include <memory>
#include <filesystem>
namespace PS::HelpyMapCache {
struct View {std::shared_ptr<const MapFilters::Dataset> Data;MapFilters::Selection Filters;std::string Status,CachedBuild;bool Ready=false;uint64_t Revision=0;};
View& Current();
void Initialize(); // Disk only; no request before opt-in or a confirmed patch.
void Poll();
bool Busy();
bool StartInitial(); // Explicit initial-assets opt-in.
bool RefreshEligible();
bool Refresh(bool manual=false); // Automatic checks require a confirmed build change.
void Cancel()noexcept;
void SaveFilters();
void Shutdown()noexcept;
#if defined(HELPY_MAP_CACHE_TEST)
void Test(const std::filesystem::path& folder,const std::string& bytes);
#endif
}
