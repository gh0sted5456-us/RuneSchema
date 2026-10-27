#pragma once
#include <filesystem>
#include <cstdint>
#include <map>
#include <set>
#include <string>
namespace PS::HelpyDownloads {
struct View {bool NeedsArtwork=false,HintDismissed=false;std::string Status;std::filesystem::path AuthorTools;};
const View& Current();
bool Busy();
unsigned Completed();
unsigned Total();
uint64_t BytesCompleted();
uint64_t BytesTotal();
void CacheApplied(const std::string& status);
void Initialize(); // One filesystem-only check; never starts a network request.
bool StartArtwork(); // Explicit user action only.
bool StartAuthorTools(); // Downloads an inert ZIP; never extracts or runs it.
std::filesystem::path AuthorToolsFolder();
bool StartBaseCache(const std::map<std::string,std::string>& sources,const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& gameVersion);
bool Poll(); // True once after new artwork is committed.
std::filesystem::path TakeBaseCache();
bool ReleaseImportedPack(const std::filesystem::path& file) noexcept; // Managed staging only; after verified import.
void DismissHint();
void Cancel() noexcept;
void Shutdown() noexcept;
#if defined(HELPY_ASSET_TEST)
void TestArtwork(const std::filesystem::path& source,const std::filesystem::path& destination);
void TestAuthorTools(const std::filesystem::path& source,const std::filesystem::path& destination,bool cancelBeforeCommit=false);
#endif
}
