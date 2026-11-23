#pragma once
// Extracts a downloaded ZIP (the browser's ad blocker) into a fresh folder, refusing entries that
// would land outside it. Uses miniz.
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace PS::HelpyZip {
struct Limits { size_t MaxFiles = 4000; uint64_t MaxBytes = 64ull * 1024 * 1024; };
// A relative path made of plain segments: no root, drive, backslash, "..", empty segment or control character.
bool SafeEntry(std::string_view name);
// Extracts every entry of zip into folder, which must not exist yet. Throws on any refused entry,
// limit or read error, leaving nothing behind.
void Extract(const std::filesystem::path& zip, const std::filesystem::path& folder, Limits limits = {});
}
