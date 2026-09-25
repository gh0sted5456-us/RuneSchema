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
    if (argc != 4) throw std::runtime_error("Three WinGDK journal contract inputs are required");
    const auto hierarchy = Read(argv[1]);
    const auto journalContract = Read(argv[2]);
    const auto mesh = Read(argv[3]);
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
    Require(mesh.find("WearableMeshRoutineReturnWinGDK") != std::string::npos,
        "WinGDK wearable mesh binding is missing");
    std::cout << "WinGDK journal and wearable-mesh lane contract passed.\n";
}
