#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::Usmap {

class Index {
public:
    static Index& Shared();

    nlohmann::json Handle(const std::filesystem::path& mappingPath,
        const std::string& fingerprint, const std::filesystem::path& cacheDirectory,
        const nlohmann::json& request);
    nlohmann::json Snapshot() const;
    void ResetForTests();

private:
    struct Property {
        uint16_t Index{};
        uint8_t ArrayDim{};
        std::string Name;
        std::string Type;
    };
    struct Type {
        std::string Name;
        std::string Path;
        std::string Super;
        uint16_t PropertySlots{};
        std::vector<Property> Properties;
    };

    mutable std::mutex m_gate;
    std::filesystem::path m_mappingPath;
    std::filesystem::path m_cacheDirectory;
    std::string m_fingerprint;
    std::string m_error;
    std::vector<Type> m_types;
    std::unordered_map<std::string, std::vector<size_t>> m_lookup;
    nlohmann::json m_cachedTypes = nlohmann::json::object();
    bool m_cacheLoaded{};
    bool m_parsed{};
    uint64_t m_parseMilliseconds{};

    void SelectSource(const std::filesystem::path& mappingPath,
        const std::string& fingerprint, const std::filesystem::path& cacheDirectory);
    void LoadCache();
    void SaveCache();
    void Parse();
    nlohmann::json Describe(const std::string& query);
    nlohmann::json Search(const std::string& query, size_t limit);
    static std::string Normalize(std::string value);
};

}
