#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("WinGDK journal contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc != 6) throw std::runtime_error("Five WinGDK journal contract inputs are required");
    const auto hierarchy = Read(argv[1]);
    const auto journalContract = Read(argv[2]);
    const auto mesh = Read(argv[3]);
    const auto bridge = Read(argv[4]);
    const auto loader = Read(argv[5]);
    Require(hierarchy.find("AllowsGamePassNativeSignatures()") != std::string::npos,
        "WinGDK journal selection is not storefront-gated");
    Require(hierarchy.find("JournalWinGDKContract::Definitions") != std::string::npos
        && hierarchy.find("JournalNativeContract::Definitions") != std::string::npos,
        "Steam and WinGDK journal contracts are not isolated");
    Require(hierarchy.find("std::array<FixedCategory, 3>") != std::string::npos,
        "WinGDK category entry points are not modeled separately");
    for (const auto* name : {"JournalHierarchyInsertWinGDK", "JournalCategory1WinGDK",
            "JournalCategory2WinGDK", "JournalCategory3WinGDK", "JournalHierarchyBuilderLayoutWinGDK"})
        Require(journalContract.find(name) != std::string::npos, "A verified WinGDK journal binding is missing");
    Require(journalContract.find("4c894c24204c894424185355565741544155415641574883ec68") != std::string::npos,
        "WinGDK journal insert is not the verified 40-byte-key/28-byte-value map specialization");
    Require(journalContract.find("48895c24184c894c242055565741544155415641574883ec40") == std::string::npos,
        "Unsafe WinGDK 64-byte-key/8-byte-value map specialization is still configured");
    Require(mesh.find("WearableMeshRoutineReturnWinGDK") != std::string::npos,
        "WinGDK wearable mesh binding is missing");
    Require(bridge.find("Reader=reinterpret_cast<uintptr_t>(table[2])") != std::string::npos
        && bridge.find("Writer=reinterpret_cast<uintptr_t>(table[1])") != std::string::npos,
        "WinGDK journal read/write interface was not mapped");
    Require(bridge.find("called(Reader,0xd7)") != std::string::npos
        && bridge.find("called(Writer,0x1cd)") != std::string::npos
        && bridge.find("called(Writer,0x1f0)") != std::string::npos,
        "WinGDK journal JSON calls were not mapped");
    Require(bridge.find("struct alignas(16) StringView") != std::string::npos
        && bridge.find("struct alignas(16) Array") != std::string::npos
        && bridge.find("struct alignas(16) Shared") != std::string::npos,
        "WinGDK journal JSON arguments are not aligned for native SIMD loads");
    Require(loader.find("JournalPersistence::Install(this") != std::string::npos
        && loader.find("temporary journal mode requires a verified WinGDK writer adapter") == std::string::npos,
        "WinGDK journal persistence bridge is not installed");
    std::cout << "WinGDK journal and wearable-mesh lane contract passed.\n";
}
