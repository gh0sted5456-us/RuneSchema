#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>
namespace PS::HelpyDownloads {
struct View {bool NeedsArtwork=false,HintDismissed=false;std::string Status;};
const View& Current();
bool Busy();
unsigned Completed();
unsigned Total();
void Initialize(); // One filesystem-only check; never starts a network request.
bool StartArtwork(); // Explicit user action only.
bool StartBaseCache(const std::map<std::string,std::string>& sources,const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& gameVersion);
bool Poll(); // True once after new artwork is committed.
std::filesystem::path TakeBaseCache();
bool ReleaseImportedPack(const std::filesystem::path& file) noexcept; // Managed staging only; after verified import.
void DismissHint();
void Cancel() noexcept;
void Shutdown() noexcept;
#if defined(HELPY_ASSET_TEST)
void TestArtwork(const std::filesystem::path& source,const std::filesystem::path& destination);
#endif
}
