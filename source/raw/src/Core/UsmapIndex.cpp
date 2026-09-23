#include "Core/UsmapIndex.h"

#include "Core/ConfigFiles.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace PS::Usmap {
namespace {
namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr uint16_t FileMagic = 0x30C4;
constexpr uint32_t ExtensionMagic = 0x54584543;
constexpr uint32_t PathExtension = 0x48545050;
constexpr size_t MaximumFileBytes = 128ull * 1024 * 1024;
constexpr size_t MaximumPayloadBytes = 192ull * 1024 * 1024;
constexpr size_t MaximumCacheBytes = 2ull * 1024 * 1024;
constexpr size_t MaximumCachedTypes = 128;
constexpr auto ParseBudget = std::chrono::milliseconds(2500);

class Reader {
public:
    Reader(const uint8_t* data, size_t size, std::chrono::steady_clock::time_point deadline)
        : m_data(data), m_size(size), m_deadline(deadline) {}

    template <class T> T Read()
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (sizeof(T) > Remaining()) throw std::runtime_error("USMAP ended unexpectedly");
        T value{};
        std::memcpy(&value, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return value;
    }

    std::string String(size_t size)
    {
        if (size > Remaining()) throw std::runtime_error("USMAP string exceeds payload");
        std::string value(reinterpret_cast<const char*>(m_data + m_offset), size);
        m_offset += size;
        return value;
    }

    void Skip(size_t size)
    {
        if (size > Remaining()) throw std::runtime_error("USMAP extension exceeds payload");
        m_offset += size;
    }

    size_t Position() const { return m_offset; }
    size_t Remaining() const { return m_size - m_offset; }
    void Seek(size_t value)
    {
        if (value > m_size) throw std::runtime_error("USMAP seek exceeds payload");
        m_offset = value;
    }
    void CheckBudget() const
    {
        if (std::chrono::steady_clock::now() > m_deadline)
            throw std::runtime_error("USMAP parsing exceeded 2500 ms");
    }

private:
    const uint8_t* m_data{};
    size_t m_size{};
    size_t m_offset{};
    std::chrono::steady_clock::time_point m_deadline;
};

std::vector<uint8_t> ReadFile(const fs::path& path)
{
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error || size < 12 || size > MaximumFileBytes)
        throw std::runtime_error("USMAP file size is outside the 12-byte to 128-MiB limit");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("USMAP file could not be opened");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("USMAP file could not be read");
    return bytes;
}

std::string NameAt(const std::vector<std::string>& names, int32_t index, bool nullable = false)
{
    if (nullable && index == -1) return {};
    if (index < 0 || static_cast<size_t>(index) >= names.size())
        throw std::runtime_error("USMAP name index is invalid");
    return names[static_cast<size_t>(index)];
}

std::string PropertyType(Reader& reader, const std::vector<std::string>& names, unsigned depth = 0)
{
    if (depth > 16) throw std::runtime_error("USMAP property nesting exceeds 16 levels");
    static constexpr std::array<const char*, 31> labels{{
        "Byte", "Bool", "Int", "Float", "Object", "Name", "Delegate", "Double",
        "Array", "Struct", "String", "Text", "Interface", "MulticastDelegate", "WeakObject",
        "LazyObject", "AssetObject", "SoftObject", "UInt64", "UInt32", "UInt16", "Int64",
        "Int16", "Int8", "Map", "Set", "Enum", "FieldPath", "Optional", "Utf8String", "AnsiString"
    }};
    const auto kind = reader.Read<uint8_t>();
    if (kind == 0xFF) return "Unknown";
    if (kind >= labels.size()) throw std::runtime_error("USMAP property kind is invalid");
    if (kind == 26) {
        const auto inner = PropertyType(reader, names, depth + 1);
        return "Enum<" + NameAt(names, reader.Read<int32_t>()) + ":" + inner + ">";
    }
    if (kind == 9) return "Struct<" + NameAt(names, reader.Read<int32_t>()) + ">";
    if (kind == 8 || kind == 25 || kind == 28) {
        const auto inner = PropertyType(reader, names, depth + 1);
        return std::string(labels[kind]) + "<" + inner + ">";
    }
    if (kind == 24) {
        const auto key = PropertyType(reader, names, depth + 1);
        const auto value = PropertyType(reader, names, depth + 1);
        return "Map<" + key + "," + value + ">";
    }
    return labels[kind];
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

Index& Index::Shared()
{
    static Index value;
    return value;
}

std::string Index::Normalize(std::string value)
{
    return Lower(std::move(value));
}

void Index::ResetForTests()
{
    std::scoped_lock lock(m_gate);
    m_mappingPath.clear();
    m_cacheDirectory.clear();
    m_fingerprint.clear();
    m_error.clear();
    m_types.clear();
    m_lookup.clear();
    m_cachedTypes = json::object();
    m_cacheLoaded = false;
    m_parsed = false;
    m_parseMilliseconds = 0;
}

void Index::SelectSource(const fs::path& mappingPath, const std::string& fingerprint,
    const fs::path& cacheDirectory)
{
    if (mappingPath == m_mappingPath && fingerprint == m_fingerprint && cacheDirectory == m_cacheDirectory) return;
    m_mappingPath = mappingPath;
    m_cacheDirectory = cacheDirectory;
    m_fingerprint = fingerprint;
    m_error.clear();
    m_types.clear();
    m_lookup.clear();
    m_cachedTypes = json::object();
    m_cacheLoaded = false;
    m_parsed = false;
    m_parseMilliseconds = 0;
}

void Index::LoadCache()
{
    if (m_cacheLoaded) return;
    m_cacheLoaded = true;
    if (m_fingerprint.empty() || m_cacheDirectory.empty()) return;
    const auto path = m_cacheDirectory / ("usmap-" + m_fingerprint + ".json");
    try {
        if (!fs::is_regular_file(path) || fs::file_size(path) > MaximumCacheBytes) return;
        const auto value = json::parse(ConfigFiles::Read(path, MaximumCacheBytes));
        if (value.value("SchemaVersion", 0) != 1 || value.value("Fingerprint", std::string{}) != m_fingerprint
            || !value.contains("Types") || !value["Types"].is_object()
            || value["Types"].size() > MaximumCachedTypes) return;
        m_cachedTypes = value["Types"];
    } catch (...) {
        m_cachedTypes = json::object();
    }
}

void Index::SaveCache()
{
    if (m_fingerprint.empty() || m_cacheDirectory.empty() || m_cachedTypes.size() > MaximumCachedTypes) return;
    const json value{{"SchemaVersion", 1}, {"Fingerprint", m_fingerprint}, {"Types", m_cachedTypes}};
    const auto text = value.dump();
    if (text.size() > MaximumCacheBytes) return;
    ConfigFiles::Write(m_cacheDirectory / ("usmap-" + m_fingerprint + ".json"), text);
}

void Index::Parse()
{
    if (m_parsed) return;
    if (!m_error.empty()) throw std::runtime_error(m_error);
    const auto started = std::chrono::steady_clock::now();
    try {
        const auto file = ReadFile(m_mappingPath);
        Reader header(file.data(), file.size(), started + ParseBudget);
        if (header.Read<uint16_t>() != FileMagic) throw std::runtime_error("USMAP magic is invalid");
        const auto version = header.Read<uint8_t>();
        if (version > 4) throw std::runtime_error("USMAP version is newer than supported version 4");
        if (version >= 1) {
            const auto versioned = header.Read<int32_t>();
            if (versioned != 0) throw std::runtime_error("Versioned USMAP headers are not supported");
        }
        const auto compression = header.Read<uint8_t>();
        const auto compressedSize = header.Read<uint32_t>();
        const auto decompressedSize = header.Read<uint32_t>();
        if (compression != 0) throw std::runtime_error("Compressed USMAP files are not supported; use the UE4SS dump");
        if (compressedSize != decompressedSize || decompressedSize > MaximumPayloadBytes
            || compressedSize != header.Remaining())
            throw std::runtime_error("USMAP payload sizes are invalid");

        Reader reader(file.data() + header.Position(), decompressedSize, started + ParseBudget);
        const auto nameCount = reader.Read<uint32_t>();
        if (!nameCount || nameCount > 2'000'000) throw std::runtime_error("USMAP name count is outside limits");
        std::vector<std::string> names;
        names.reserve(nameCount);
        for (uint32_t index = 0; index < nameCount; ++index) {
            if ((index & 1023u) == 0) reader.CheckBudget();
            const auto length = version >= 2 ? reader.Read<uint16_t>() : reader.Read<uint8_t>();
            names.push_back(reader.String(length));
        }

        const auto enumCount = reader.Read<uint32_t>();
        if (enumCount > 500'000) throw std::runtime_error("USMAP enum count is outside limits");
        uint64_t enumMembers = 0;
        for (uint32_t index = 0; index < enumCount; ++index) {
            if ((index & 1023u) == 0) reader.CheckBudget();
            (void)NameAt(names, reader.Read<int32_t>());
            const auto count = version >= 3 ? reader.Read<uint16_t>() : reader.Read<uint8_t>();
            enumMembers += count;
            if (enumMembers > 4'000'000) throw std::runtime_error("USMAP enum members exceed limits");
            for (uint32_t member = 0; member < count; ++member) {
                if (version >= 4) (void)reader.Read<uint64_t>();
                (void)NameAt(names, reader.Read<int32_t>());
            }
        }

        const auto structCount = reader.Read<uint32_t>();
        if (structCount > 1'000'000) throw std::runtime_error("USMAP type count is outside limits");
        m_types.clear();
        m_types.reserve(structCount);
        uint64_t properties = 0;
        for (uint32_t index = 0; index < structCount; ++index) {
            if ((index & 511u) == 0) reader.CheckBudget();
            Type type;
            type.Name = NameAt(names, reader.Read<int32_t>());
            type.Super = NameAt(names, reader.Read<int32_t>(), true);
            type.PropertySlots = reader.Read<uint16_t>();
            const auto serializable = reader.Read<uint16_t>();
            properties += serializable;
            if (properties > 8'000'000) throw std::runtime_error("USMAP properties exceed limits");
            type.Properties.reserve(serializable);
            for (uint32_t propertyIndex = 0; propertyIndex < serializable; ++propertyIndex) {
                Property property;
                property.Index = reader.Read<uint16_t>();
                property.ArrayDim = reader.Read<uint8_t>();
                property.Name = NameAt(names, reader.Read<int32_t>());
                property.Type = PropertyType(reader, names);
                type.Properties.push_back(std::move(property));
            }
            m_types.push_back(std::move(type));
        }

        if (reader.Remaining() >= 9 && reader.Read<uint32_t>() == ExtensionMagic) {
            (void)reader.Read<uint8_t>();
            const auto count = reader.Read<uint32_t>();
            if (count > 64) throw std::runtime_error("USMAP extension count is outside limits");
            for (uint32_t extension = 0; extension < count; ++extension) {
                const auto id = reader.Read<uint32_t>();
                const auto size = reader.Read<uint32_t>();
                const auto end = reader.Position() + size;
                if (end < reader.Position() || size > reader.Remaining())
                    throw std::runtime_error("USMAP extension size is invalid");
                if (id == PathExtension && size >= 1) {
                    (void)reader.Read<uint8_t>();
                    const auto pathsForEnums = reader.Read<uint32_t>();
                    if (pathsForEnums != enumCount) throw std::runtime_error("USMAP enum path count does not match");
                    for (uint32_t path = 0; path < pathsForEnums; ++path) (void)NameAt(names, reader.Read<int32_t>());
                    const auto pathsForTypes = reader.Read<uint32_t>();
                    if (pathsForTypes != structCount) throw std::runtime_error("USMAP type path count does not match");
                    for (uint32_t path = 0; path < pathsForTypes; ++path)
                        m_types[path].Path = NameAt(names, reader.Read<int32_t>());
                }
                reader.Seek(end);
            }
        }

        m_lookup.clear();
        for (size_t index = 0; index < m_types.size(); ++index) {
            m_lookup[Normalize(m_types[index].Name)].push_back(index);
            if (!m_types[index].Path.empty()) m_lookup[Normalize(m_types[index].Path)].push_back(index);
        }
        m_parsed = true;
        m_parseMilliseconds = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
    } catch (const std::exception& error) {
        m_types.clear();
        m_lookup.clear();
        m_error = error.what();
        throw;
    }
}

json Index::Describe(const std::string& query)
{
    LoadCache();
    const auto key = Normalize(query);
    if (const auto cached = m_cachedTypes.find(key); cached != m_cachedTypes.end()) {
        auto response = *cached;
        response["Source"] = "cache";
        return response;
    }
    Parse();
    json response{{"Found", false}, {"Ambiguous", false}, {"Fingerprint", m_fingerprint}};
    const auto found = m_lookup.find(key);
    if (found != m_lookup.end() && found->second.size() > 1) {
        response["Ambiguous"] = true;
        response["Candidates"] = json::array();
        for (const auto index : found->second)
            response["Candidates"].push_back(m_types[index].Path.empty() ? m_types[index].Name : m_types[index].Path);
    } else if (found != m_lookup.end()) {
        const auto& type = m_types[found->second.front()];
        json properties = json::array();
        for (const auto& property : type.Properties) properties.push_back({
            {"Index", property.Index}, {"ArrayDim", property.ArrayDim},
            {"Name", property.Name}, {"Type", property.Type}});
        response = {{"Found", true}, {"Ambiguous", false}, {"Fingerprint", m_fingerprint},
            {"Type", {{"Name", type.Name}, {"Path", type.Path}, {"Super", type.Super},
                {"PropertySlots", type.PropertySlots}, {"Properties", std::move(properties)}}}};
    }
    response["Source"] = "usmap";
    if (m_cachedTypes.size() < MaximumCachedTypes) {
        auto stored = response;
        stored.erase("Source");
        m_cachedTypes[key] = std::move(stored);
        try { SaveCache(); } catch (...) {}
    }
    return response;
}

json Index::Search(const std::string& query, size_t limit)
{
    Parse();
    const auto needle = Normalize(query);
    json rows = json::array();
    for (const auto& type : m_types) {
        const auto candidate = Normalize(type.Path.empty() ? type.Name : type.Path);
        if (!needle.empty() && candidate.find(needle) == std::string::npos) continue;
        rows.push_back({{"Name", type.Name}, {"Path", type.Path}, {"Super", type.Super},
            {"Properties", type.Properties.size()}});
        if (rows.size() >= limit) break;
    }
    return {{"Fingerprint", m_fingerprint}, {"Results", std::move(rows)}, {"Limit", limit}};
}

json Index::Snapshot() const
{
    std::scoped_lock lock(m_gate);
    return {{"Parsed", m_parsed}, {"TypeCount", m_types.size()},
        {"CachedTypeCount", m_cachedTypes.size()}, {"ParseMilliseconds", m_parseMilliseconds},
        {"Error", m_error}};
}

json Index::Handle(const fs::path& mappingPath, const std::string& fingerprint,
    const fs::path& cacheDirectory, const json& request)
{
    std::scoped_lock lock(m_gate);
    if (!request.is_object()) throw std::runtime_error("Mapping request must be an object");
    SelectSource(mappingPath, fingerprint, cacheDirectory);
    if (m_mappingPath.empty() || m_fingerprint.empty())
        return {{"Ok", false}, {"Available", false}, {"Error", "Mappings.usmap was not found"}};
    const auto action = request.value("Action", std::string("Status"));
    if (action == "Status") {
        LoadCache();
        return {{"Ok", true}, {"Available", true}, {"Path", m_mappingPath.string()},
            {"Fingerprint", m_fingerprint}, {"Parsed", m_parsed}, {"TypeCount", m_types.size()},
            {"CachedTypeCount", m_cachedTypes.size()}, {"ParseMilliseconds", m_parseMilliseconds},
            {"Error", m_error}};
    }
    if (action == "DescribeType") {
        const auto type = request.value("Type", std::string{});
        if (type.empty() || type.size() > 1024) throw std::runtime_error("DescribeType requires Type");
        auto result = Describe(type);
        result["Ok"] = true;
        return result;
    }
    if (action == "HasProperty") {
        const auto type = request.value("Type", std::string{});
        const auto property = request.value("Property", std::string{});
        if (type.empty() || type.size() > 1024 || property.empty() || property.size() > 256)
            throw std::runtime_error("HasProperty requires Type and Property");
        auto result = Describe(type);
        bool present = false;
        if (result.value("Found", false) && result.contains("Type"))
            for (const auto& item : result["Type"]["Properties"])
                if (Normalize(item.value("Name", std::string{})) == Normalize(property)) { present = true; break; }
        return {{"Ok", true}, {"Found", result.value("Found", false)},
            {"Ambiguous", result.value("Ambiguous", false)}, {"HasProperty", present},
            {"Fingerprint", m_fingerprint}};
    }
    if (action == "SearchTypes") {
        const auto query = request.value("Query", std::string{});
        const auto limit = std::clamp<size_t>(request.value("Limit", 25u), 1, 100);
        auto result = Search(query, limit);
        result["Ok"] = true;
        return result;
    }
    if (action == "Warm") {
        Parse();
        return {{"Ok", true}, {"Fingerprint", m_fingerprint}, {"Parsed", true},
            {"TypeCount", m_types.size()}, {"ParseMilliseconds", m_parseMilliseconds}};
    }
    throw std::runtime_error("Unknown mapping action");
}

}
