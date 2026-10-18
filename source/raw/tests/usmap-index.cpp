#include "Core/UsmapIndex.h"
#include "Runtime/MappingBackbone.h"

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

static std::vector<uint8_t> MakeMap(const std::string& typePath="/Script/Test.TestType")
{
    std::vector<uint8_t> payload;
    Append<uint32_t>(payload, 3);
    AppendName(payload, "TestType");
    AppendName(payload, "Health");
    AppendName(payload, typePath);
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

int main(int argc,char** argv)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("runeschema-usmap-" + std::to_string(stamp));
    const auto mapping = root / "Mappings.usmap";
    const auto cache = root / "cache";
    fs::create_directories(root);
    const auto bytes = MakeMap();
    std::ofstream(mapping, std::ios::binary).write(
        reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const auto fallback=PS::MappingBackbone::Current(root);
    const auto freshFingerprint=PS::MappingBackbone::Fingerprint(mapping);
    PS::MappingBackbone::Activate({mapping,bytes.size(),freshFingerprint,true});
    const auto generated=PS::MappingBackbone::Current(root);
    Check(generated.Available&&generated.Path==mapping&&generated.Fingerprint==freshFingerprint,"validated mapping snapshot activated");
    Check(fallback.Fingerprint==freshFingerprint,"previous mapping value snapshot remains valid");
    PS::MappingBackbone::Generated.store(nullptr);

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

    const auto moduleMap=root/"NativeModule.usmap";
    const auto moduleBytes=MakeMap("/Script/Test");
    std::ofstream(moduleMap,std::ios::binary).write(reinterpret_cast<const char*>(moduleBytes.data()),static_cast<std::streamsize>(moduleBytes.size()));
    // A v1 negative lookup must not survive the corrected path normalization.
    const nlohmann::json stale={{"SchemaVersion",1},{"Fingerprint","module-fingerprint"},{"Types",{
        {"/script/test.testtype",{{"Found",false},{"Ambiguous",false}}}}}};
    std::ofstream(cache/"usmap-module-fingerprint.json")<<stale.dump();
    index.ResetForTests();
    const auto qualified=index.Handle(moduleMap,"module-fingerprint",cache,{{"Action","DescribeType"},{"Type","/Script/Test.TestType"}});
    Check(qualified.value("Found",false),"module path resolves to qualified native type");
    Check(qualified.at("Type").at("Path")=="/Script/Test.TestType","native module path appends type name");
    Check(qualified.value("Source","")=="usmap","old negative cache is invalidated");
    Check(index.Handle(moduleMap,"module-fingerprint",cache,{{"Action","SearchTypes"},{"Query","TestType"}}).at("Results").size()==1,"native type search includes qualified type name");
    Check(!index.Handle(moduleMap,"module-fingerprint",cache,{{"Action","DescribeType"},{"Type","/Script/Other.TestType"}}).value("Found",true),"wrong module does not resolve by bare-name fallback");
    const auto blueprintMap=root/"BlueprintPackage.usmap";
    const auto blueprintBytes=MakeMap("/Game/Test/BP_Test");
    std::ofstream(blueprintMap,std::ios::binary).write(reinterpret_cast<const char*>(blueprintBytes.data()),static_cast<std::streamsize>(blueprintBytes.size()));
    index.ResetForTests();
    Check(index.Handle(blueprintMap,"blueprint-fingerprint",cache,{{"Action","DescribeType"},{"Type","/Game/Test/BP_Test.TestType"}}).value("Found",false),"Blueprint package path resolves to qualified type");

    if(argc==3) {
        index.ResetForTests();
        const std::string query=argv[2];
        const nlohmann::json request=query.starts_with("?")?nlohmann::json{{"Action","SearchTypes"},{"Query",query.substr(1)},{"Limit",30}}:nlohmann::json{{"Action","DescribeType"},{"Type",query}};
        std::cout<<index.Handle(fs::path(argv[1]),"external-schema-inspection",root/"native-cache",request).dump(2)<<'\n';
    }
    if(argc==2) {
        index.ResetForTests();
        const auto capture=index.Handle(fs::path(argv[1]),"external-native-capture",root/"native-cache",
            {{"Action","DescribeType"},{"Type","/Script/Dominion.HeldEquipmentData"}});
        if(!capture.value("Found",false)) {
            std::cerr<<capture.dump(2)<<'\n'<<index.Handle(fs::path(argv[1]),"external-native-capture",root/"native-cache",
                {{"Action","DescribeType"},{"Type","HeldEquipmentData"}}).dump(2)<<'\n'<<index.Snapshot().dump(2)<<'\n';
        }
        Check(capture.value("Found",false),"native HeldEquipmentData resolves");
        bool projectileObject=false;
        for(const auto& field:capture.at("Type").at("Properties"))
            if(field.value("Name","")=="ProjectileClass")projectileObject=field.value("Type","")=="Object";
        Check(projectileObject,"1.0.1 ProjectileClass is an Object, not the old SoftObject");
        std::cout<<"Native 1.0.1 mapping verified.\n";
    }
    std::error_code error;
    fs::remove_all(root, error);
    std::cout << "USMAP lazy-index contract passed.\n";
}
