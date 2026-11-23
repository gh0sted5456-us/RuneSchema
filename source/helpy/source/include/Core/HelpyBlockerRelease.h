#pragma once
// Helpy's browser ad blocker: which uBlock Origin Lite file to download from its official GitHub
// release, and the checksum it must match. Nothing is downloaded until the player turns ad blocking
// on. Engine-free and tested on its own.
#include <nlohmann/json.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PS::HelpyBlockerRelease {
using Json = nlohmann::json;
inline constexpr std::string_view LatestApi = "https://api.github.com/repos/uBlockOrigin/uBOL-home/releases/latest";
inline constexpr std::string_view DownloadPrefix = "https://github.com/uBlockOrigin/uBOL-home/releases/download/";
inline constexpr std::string_view Project = "https://github.com/uBlockOrigin/uBOL-home";
inline constexpr uint64_t MaxBytes = 32ull * 1024 * 1024;

struct Release { std::string Tag, Asset, Url, Sha256; uint64_t Size = 0; };

inline bool Tag(std::string_view tag) {
    if (tag.empty() || tag.size() > 40) return false;
    for (const char c : tag) if (!((c >= '0' && c <= '9') || c == '.')) return false;
    return tag.front() != '.' && tag.back() != '.';
}
inline bool Hex64(std::string_view text) {
    if (text.size() != 64) return false;
    for (const char c : text) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
// The Edge build (WebView2 is Edge), else the Chromium build, from GitHub's "latest release" JSON.
// Refuses anything without a sha256 digest, outside the project's own download URLs, or oversized.
inline Release Parse(const Json& release) {
    if (!release.is_object() || !release.contains("tag_name") || !release["tag_name"].is_string())
        throw std::runtime_error("The uBlock Origin Lite release has no version.");
    Release result; result.Tag = release["tag_name"].get<std::string>();
    if (!Tag(result.Tag)) throw std::runtime_error("The uBlock Origin Lite release version is not recognised.");
    const auto assets = release.find("assets");
    if (assets == release.end() || !assets->is_array()) throw std::runtime_error("The uBlock Origin Lite release has no files.");
    for (const auto* build : {".edge.zip", ".chromium.zip"}) {
        const auto wanted = "uBOLite_" + result.Tag + build;
        for (const auto& asset : *assets) {
            if (!asset.is_object() || asset.value("name", std::string{}) != wanted) continue;
            const auto url = asset.value("browser_download_url", std::string{});
            const auto digest = asset.value("digest", std::string{});
            const auto field = asset.find("size");
            const uint64_t size = field != asset.end() && field->is_number_integer() && field->get<int64_t>() > 0 ? field->get<uint64_t>() : 0;
            if (url != std::string(DownloadPrefix) + result.Tag + "/" + wanted) throw std::runtime_error("The uBlock Origin Lite download is not from its official release.");
            if (!digest.starts_with("sha256:") || !Hex64(std::string_view(digest).substr(7))) throw std::runtime_error("The uBlock Origin Lite download has no checksum to verify.");
            if (size == 0 || size > MaxBytes) throw std::runtime_error("The uBlock Origin Lite download has an unexpected size.");
            result.Asset = wanted; result.Url = url; result.Sha256 = digest.substr(7); result.Size = size;
            return result;
        }
    }
    throw std::runtime_error("The latest uBlock Origin Lite release has no Edge or Chromium build.");
}
}
