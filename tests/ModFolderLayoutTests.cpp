#include "Utility/ModFolderLayout.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc != 2 || fs::exists(argv[1])) return 2;
    const fs::path root = fs::absolute(argv[1]);
    fs::create_directories(root);
    int checks = 0;
    auto check = [&](const fs::path& folder, bool expected, bool failed = false) {
        std::error_code error;
        const auto found = PS::ModFolderLayout::ContainsLegacyPakContent(folder, error);
        if (found != expected || bool(error) != failed) throw std::runtime_error(folder.string());
        ++checks;
    };
    auto file = [&](const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path);
        if (!stream) throw std::runtime_error("fixture creation failed");
    };
    fs::create_directories(root / "empty");
    check(root / "empty", false);
    file(root / "json" / "nested" / "data.json");
    check(root / "json", false);
    fs::create_directories(root / "fake" / "folder.pak");
    check(root / "fake", false);
    for (const auto* extension : {".pak", ".utoc", ".ucas", ".sig", ".PAK", ".UToC"}) {
        const fs::path folder = root / (std::string("legacy") + extension);
        file(folder / "nested" / (std::string("Pack") + extension));
        check(folder, true);
    }
    file(root / "mod" / "paks" / "PackA" / "PackA.pak");
    file(root / "mod" / "paks" / "PackB" / "PackB.pak");
    check(root / "mod" / "paks", true);
    file(root / "flat" / "Pack.pak");
    check(root / "flat", true);
    file(root / "unicode" / fs::path(L"\u7070\u8272") / "Pack.pak");
    check(root / "unicode", true);
    check(root / "missing", false, true);
    check(root / "flat" / "Pack.pak", false, true);
    std::error_code stale = std::make_error_code(std::errc::permission_denied);
    if (PS::ModFolderLayout::ContainsLegacyPakContent(root / "empty", stale) || stale) return 3;
    std::cout << ++checks << " filesystem checks passed\n";
}
