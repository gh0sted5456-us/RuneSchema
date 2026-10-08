#include "Core/UsmapIndex.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

template <class T> static void Append(std::vector<uint8_t>& bytes, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* start = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), start, start + sizeof(T));
}

static void AppendName(std::vector<uint8_t>& bytes, const std::string& value)
{
    Append<uint16_t>(bytes, static_cast<uint16_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

static std::vector<uint8_t> MakeMap()
{
    std::vector<uint8_t> payload;
    Append<uint32_t>(payload, 3);
    AppendName(payload, "TestType");
    AppendName(payload, "Health");
    AppendName(payload, "/Script/Test.TestType");
    Append<uint32_t>(payload, 0); // enums
    Append<uint32_t>(payload, 1); // structs
    Append<int32_t>(payload, 0);  // TestType
    Append<int32_t>(payload, -1); // no superclass
    Append<uint16_t>(payload, 1); // property slots
    Append<uint16_t>(payload, 1); // serializable properties
    Append<uint16_t>(payload, 0); // property index
    Append<uint8_t>(payload, 1);  // array dimension
    Append<int32_t>(payload, 1);  // Health
    Append<uint8_t>(payload, 2);  // Int

    Append<uint32_t>(payload, 0x54584543); // CEXT
    Append<uint8_t>(payload, 0);
    Append<uint32_t>(payload, 1);
    Append<uint32_t>(payload, 0x48545050); // PPTH
    Append<uint32_t>(payload, 13);
    Append<uint8_t>(payload, 0);
    Append<uint32_t>(payload, 0); // enum paths
    Append<uint32_t>(payload, 1); // struct paths
    Append<int32_t>(payload, 2);

    std::vector<uint8_t> file;
    Append<uint16_t>(file, 0x30C4);
    Append<uint8_t>(file, 4);
    Append<int32_t>(file, 0);
    Append<uint8_t>(file, 0);
    Append<uint32_t>(file, static_cast<uint32_t>(payload.size()));
    Append<uint32_t>(file, static_cast<uint32_t>(payload.size()));
    file.insert(file.end(), payload.begin(), payload.end());
    return file;
}

int main()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("runeschema-usmap-" + std::to_string(stamp));
    const auto mapping = root / "Mappings.usmap";
    const auto cache = root / "cache";
    fs::create_directories(root);
    const auto bytes = MakeMap();
    std::ofstream(mapping, std::ios::binary).write(
        reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    auto& index = PS::Usmap::Index::Shared();
    index.ResetForTests();
    auto status = index.Handle(mapping, "test-fingerprint", cache, {{"Action", "Status"}});
    Check(status.value("Ok", false), "status succeeds");
    Check(!status.value("Parsed", true), "status does not parse mapping");

    auto description = index.Handle(mapping, "test-fingerprint", cache,
        {{"Action", "DescribeType"}, {"Type", "/Script/Test.TestType"}});
    Check(description.value("Found", false), "full type path resolves");
    Check(description["Type"]["Properties"][0].value("Name", "") == "Health", "property name parsed");
    Check(description["Type"]["Properties"][0].value("Type", "") == "Int", "property kind parsed");
    Check(description.value("Source", "") == "usmap", "first query uses USMAP");

    auto property = index.Handle(mapping, "test-fingerprint", cache,
        {{"Action", "HasProperty"}, {"Type", "TestType"}, {"Property", "health"}});
    Check(property.value("HasProperty", false), "property check is case-insensitive");
    auto search = index.Handle(mapping, "test-fingerprint", cache,
        {{"Action", "SearchTypes"}, {"Query", "testtype"}, {"Limit", 10}});
    Check(search["Results"].size() == 1, "type search returns indexed row");

    index.ResetForTests();
    auto cached = index.Handle(mapping, "test-fingerprint", cache,
        {{"Action", "DescribeType"}, {"Type", "/Script/Test.TestType"}});
    Check(cached.value("Found", false), "cached type resolves");
    Check(cached.value("Source", "") == "cache", "second process-shaped query uses cache");
    Check(!index.Snapshot().value("Parsed", true), "cache hit does not parse mapping");

    std::error_code error;
    fs::remove_all(root, error);
    std::cout << "USMAP lazy-index contract passed.\n";
}
