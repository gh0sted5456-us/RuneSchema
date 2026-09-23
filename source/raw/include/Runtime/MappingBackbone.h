#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace PS::MappingBackbone {
namespace fs = std::filesystem;

struct Mapping {
    fs::path Path;
    std::uintmax_t Size{};
    std::string Fingerprint;
    bool Available{};
};

inline std::string Fingerprint(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    // Cache identity only; runtime reflection supplies type data.
    std::uint64_t hash = 14695981039346656037ull;
    std::array<char, 64 * 1024> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<size_t>(index)]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream text;
    text << std::hex << std::setfill('0') << std::setw(16) << hash;
    return text.str();
}

inline Mapping Discover(const fs::path& ue4ssRoot)
{
    const std::array fixedCandidates{
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"dlls" / L"mappings" / L"Mappings.usmap",
        ue4ssRoot / L"Mappings.usmap",
        ue4ssRoot / L"mappings" / L"Mappings.usmap",
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"mappings" / L"Mappings.usmap",
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"shared" / L"Mappings.usmap"
    };
    std::vector<fs::path> candidates(fixedCandidates.begin(), fixedCandidates.end());
    const std::array searchRoots{
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"dlls" / L"mappings",
        ue4ssRoot,
        ue4ssRoot / L"mappings",
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"mappings",
        ue4ssRoot / L"Mods" / L"RuneSchema" / L"shared"
    };
    std::error_code error;
    for (const auto& root : searchRoots) {
        if (!fs::is_directory(root, error)) { error.clear(); continue; }
        std::vector<fs::directory_entry> maps;
        for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, error)) {
            if (error) { error.clear(); break; }
            auto extension = entry.path().extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
            if (entry.is_regular_file(error) && extension == L".usmap") maps.push_back(entry);
            error.clear();
        }
        std::sort(maps.begin(), maps.end(), [](const auto& left, const auto& right) {
            std::error_code leftError, rightError;
            const auto leftTime = left.last_write_time(leftError);
            const auto rightTime = right.last_write_time(rightError);
            if (!leftError && !rightError && leftTime != rightTime) return leftTime > rightTime;
            return left.path().native() < right.path().native();
        });
        for (const auto& entry : maps) {
            if (std::find(candidates.begin(), candidates.end(), entry.path()) == candidates.end())
                candidates.push_back(entry.path());
        }
    }
    for (const auto& candidate : candidates) {
        if (!fs::is_regular_file(candidate, error)) { error.clear(); continue; }
        const auto size = fs::file_size(candidate, error);
        if (error || size < 8) { error.clear(); continue; }
        const auto fingerprint = Fingerprint(candidate);
        if (!fingerprint.empty()) {
            auto canonical = fs::weakly_canonical(candidate, error);
            if (error) canonical = candidate;
            return {canonical, size, fingerprint, true};
        }
    }
    return {};
}

inline const Mapping& Current(const fs::path& ue4ssRoot)
{
    static const Mapping mapping = Discover(ue4ssRoot);
    return mapping;
}
}
